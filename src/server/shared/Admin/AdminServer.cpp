/*
 * Project Ambrose by Imjustchico
 * Runs the admin API on Crow: it resolves the token, tells every route whose socket is still open when the listener stops and detaches the handle first so nothing reaches a connection Crow has let go, refuses a bind the remote-access rule forbids, says out loud what a bind it allows still costs, proves no other socket holds the address before Crow takes it, answers every request from the shared table in a middleware that runs before Crow's own routing, with the host, origin, cookie and CSRF headers a browser session is checked by and a caller's request id when it has the router's form, answers from the same table again after it the requests Crow replies to before a connection has an address of its own, such as OPTIONS, hands only a real WebSocket upgrade to the route registered for it under the same host check and authentication, serves the built panel and its sign-in, which trades the token once for a session cookie named after the port because cookies ignore ports, logs every error with its request id, gives each open socket a handle a route may keep and write to from any thread until the socket closes, owning every socket's binding in the listener so one Crow drops without a close is still freed, queues a close behind the frames sent before it, and on a reload rotates the token live and ends every session with the old one, applies the panel folder, allowed hosts and session lifetimes without rebinding, rebinds a changed address, or brings the old listener back when the new one cannot bind.
 */

#include "AdminServer.h"
#include "AdminToken.h"
#include "ConfigMgr.h"
#include "IpAddress.h"
#include "Log.h"
#include "StringUtil.h"
#include "TlsCertificate.h"
#include "TrustedProxies.h"
#include "TlsServerContext.h"

#include <crow.h>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    constexpr char const* RootRoutePattern = "/";
    constexpr char const* SocketRoutePattern = "/<path>";

    class CrowLogBridge : public crow::ILogHandler
    {
    public:
        void Attach(Log* log) { _log.store(log); }

        void log(std::string const& message, crow::LogLevel level) override
        {
            Log* const target = _log.load();
            if (!target)
                return;
            LogLevel mapped = LogLevel::Debug;
            switch (level)
            {
                case crow::LogLevel::Warning:
                    mapped = LogLevel::Warn;
                    break;
                case crow::LogLevel::Error:
                    mapped = LogLevel::Error;
                    break;
                case crow::LogLevel::Critical:
                    mapped = LogLevel::Fatal;
                    break;
                default:
                    break;
            }
            AMBROSE_LOG(*target, mapped, "server.admin", "{}", message);
        }

    private:
        std::atomic<Log*> _log{ nullptr };
    };

    CrowLogBridge& LogBridge()
    {
        static CrowLogBridge bridge;
        return bridge;
    }

    void CloseAfterPendingFrames(crow::websocket::connection& connection, std::string reason);

    class CrowAdminSocket : public AdminSocket
    {
    public:
        explicit CrowAdminSocket(crow::websocket::connection& connection) : _connection(&connection) {}

        void SendText(std::string text) override
        {
            std::lock_guard const lock(_mutex);
            if (_connection)
                _connection->send_text(std::move(text));
        }

        void Close(std::string reason) override
        {
            std::lock_guard const lock(_mutex);
            if (_connection)
                CloseAfterPendingFrames(*_connection, std::move(reason));
        }

        std::string GetRemoteAddress() override
        {
            std::lock_guard const lock(_mutex);
            return _connection ? _connection->get_remote_ip() : std::string();
        }

        std::shared_ptr<AdminSocket> Keep() override { return _self.lock(); }

        void Bind(std::shared_ptr<CrowAdminSocket> const& self) { _self = self; }

        void Detach()
        {
            std::lock_guard const lock(_mutex);
            _connection = nullptr;
        }

    private:
        std::mutex _mutex;
        crow::websocket::connection* _connection;
        std::weak_ptr<CrowAdminSocket> _self;
    };

    struct SocketBinding
    {
        AdminSocketRoute const* Route = nullptr;
        std::shared_ptr<CrowAdminSocket> Socket;
    };

    class OpenSockets
    {
    public:
        SocketBinding* Accept(AdminSocketRoute const* route)
        {
            std::lock_guard const lock(_mutex);
            return _bindings.emplace_back(std::make_unique<SocketBinding>(SocketBinding{ route, nullptr })).get();
        }

        std::shared_ptr<CrowAdminSocket> Open(SocketBinding* binding, crow::websocket::connection& connection)
        {
            std::lock_guard const lock(_mutex);
            if (std::ranges::none_of(_bindings, [binding](std::unique_ptr<SocketBinding> const& held) { return held.get() == binding; }))
                return nullptr;
            binding->Socket = std::make_shared<CrowAdminSocket>(connection);
            binding->Socket->Bind(binding->Socket);
            return binding->Socket;
        }

        std::unique_ptr<SocketBinding> Release(SocketBinding const* binding)
        {
            std::lock_guard const lock(_mutex);
            auto const found = std::ranges::find_if(_bindings, [binding](std::unique_ptr<SocketBinding> const& held) { return held.get() == binding; });
            if (found == _bindings.end())
                return nullptr;
            std::unique_ptr<SocketBinding> released = std::move(*found);
            _bindings.erase(found);
            return released;
        }

        std::vector<std::unique_ptr<SocketBinding>> TakeAll()
        {
            std::lock_guard const lock(_mutex);
            return std::exchange(_bindings, {});
        }

    private:
        std::mutex _mutex;
        std::vector<std::unique_ptr<SocketBinding>> _bindings;
    };

    AdminRequest ToAdminRequest(crow::request const& request, AdminRouter const& router)
    {
        AdminRequest incoming;
        incoming.Method = crow::method_name(request.method);
        incoming.Path = request.url;
        incoming.RemoteAddress = router.ResolveAddress(request.remote_ip_address, request.get_header_value("x-forwarded-for"));
        incoming.UserAgent = request.get_header_value("user-agent");
        incoming.Authorization = request.get_header_value("Authorization");
        incoming.Body = request.body;
        incoming.Host = request.get_header_value("Host");
        incoming.Origin = request.get_header_value("Origin");
        incoming.Cookie = request.get_header_value("Cookie");
        incoming.Csrf = request.get_header_value("X-CSRF-Token");
        if (std::string const offered = request.get_header_value("X-Request-Id"); AdminRouter::IsRequestId(offered))
            incoming.Id = offered;
        return incoming;
    }

    void Apply(crow::response& response, AdminResponse const& answer)
    {
        response.code = answer.Status;
        response.body = answer.Body;
        response.headers.clear();
        if (!answer.ContentType.empty())
            response.set_header("Content-Type", answer.ContentType);
        for (std::pair<std::string, std::string> const& header : answer.Headers)
            response.set_header(header.first, header.second);
    }

    crow::response ToCrowResponse(AdminResponse const& answer)
    {
        crow::response response;
        Apply(response, answer);
        return response;
    }

    bool BecomesAWebSocket(crow::request const& request)
    {
        if (!request.upgrade || request.method == crow::HTTPMethod::Options)
            return false;
        return request.get_header_value("upgrade").find("h2") != 0;
    }

    class AdminGate
    {
    public:
        struct context
        {
        };

        void Bind(AdminRouter* router) { _router = router; }

        void before_handle(crow::request& request, crow::response& response, context&)
        {
            if (!_router || BecomesAWebSocket(request))
                return;
            Apply(response, _router->Dispatch(ToAdminRequest(request, *_router)));
            response.end();
        }

        void after_handle(crow::request& request, crow::response& response, context&)
        {
            if (!_router || !request.remote_ip_address.empty())
                return;
            Apply(response, _router->Dispatch(ToAdminRequest(request, *_router)));
        }

    private:
        AdminRouter* _router = nullptr;
    };

    using AdminApp = crow::App<AdminGate>;

    void CloseAfterPendingFrames(crow::websocket::connection& connection, std::string reason)
    {
        using PlainConnection = crow::websocket::Connection<crow::SocketAdaptor, AdminApp>;
        if (PlainConnection* const plain = dynamic_cast<PlainConnection*>(&connection))
        {
            plain->post([plain, reason = std::move(reason)] { plain->close(reason, crow::websocket::CloseStatusCode::NormalClosure); });
            return;
        }
        connection.close(reason);
    }

    std::optional<uint16> ReserveEndpoint(std::string const& bindIp, uint16 port, std::string& error)
    {
        std::optional<asio::ip::address> const address = Ambrose::Asio::MakeAddress(bindIp);
        if (!address)
        {
            error = fmt::format("{} is not an IP address", bindIp);
            return std::nullopt;
        }
        asio::io_context context;
        asio::ip::tcp::acceptor acceptor(context);
        asio::ip::tcp::endpoint const endpoint(*address, port);
        std::error_code code;
        acceptor.open(endpoint.protocol(), code);
        if (code)
        {
            error = code.message();
            return std::nullopt;
        }
#ifndef _WIN32
        acceptor.set_option(asio::socket_base::reuse_address(true), code);
        if (code)
        {
            error = code.message();
            return std::nullopt;
        }
#endif
        acceptor.bind(endpoint, code);
        if (code)
        {
            error = code.message();
            return std::nullopt;
        }
        asio::ip::tcp::endpoint const bound = acceptor.local_endpoint(code);
        if (code)
        {
            error = code.message();
            return std::nullopt;
        }
        acceptor.close(code);
        return bound.port();
    }

    SocketBinding* BindingOf(crow::websocket::connection& connection)
    {
        return static_cast<SocketBinding*>(connection.userdata());
    }
}

struct AdminServer::Listener
{
    AdminApp App;
    std::future<void> Worker;
    std::shared_ptr<OpenSockets> Open = std::make_shared<OpenSockets>();
    TlsCertificate Certificate;
    TlsServerContext Tls;
    std::string BindIp;
    uint16 Port = 0;
};

AdminServer::AdminServer(Log& log, std::string appName, std::filesystem::path dataFolder, std::filesystem::path configFolder)
    : _log(log), _appName(std::move(appName)), _dataFolder(std::move(dataFolder)), _configFolder(std::move(configFolder)), _auth(ListenerSettings{}.AuthFailureBurst, ListenerSettings{}.AuthFailuresPerSecond), _router(_auth)
{
    _router.SetMaxBodyBytes(ListenerSettings{}.MaxRequestBytes);
    _router.AddOpen("GET", "/api/health", [this](AdminRequest const&)
    {
        if (!_health)
            return AdminResponse::Problem(503, "not_ready", "The admin API has no health source yet");
        AdminHealth const health = _health();
        nlohmann::json body;
        body["app"] = health.App;
        body["realm"] = health.Realm;
        body["revision"] = health.Revision;
        body["uptime"] = health.UptimeSeconds;
        body["state"] = health.State;
        return AdminResponse::Json(200, body.dump());
    });

    _router.SetFiles([this](AdminRequest const& request) { return _files.Serve(request); });
    _router.SetProblemLog([this](AdminRequest const& request, AdminResponse const& response)
    {
        LogLevel const level = response.Status >= 500 ? LogLevel::Error : (response.Status == 429 ? LogLevel::Debug : LogLevel::Info);
        nlohmann::json const body = nlohmann::json::parse(response.Body, nullptr, false);
        std::string const code = body.is_object() && body.contains("error") && body["error"].is_string() ? body["error"].get<std::string>() : std::string();
        AMBROSE_LOG(_log, level, "server.admin", "{} {} from {} answered {} {} (request {})", request.Method, request.Path, request.RemoteAddress, response.Status, code, request.Id);
    });
    _router.AddPublic("POST", "/api/session", [this](AdminRequest const& request) { return SignIn(request); });
    _router.AddPublic("GET", "/api/session", [this](AdminRequest const& request)
    {
        nlohmann::json body;
        body["app"] = _appName;
        body["idle_seconds"] = _sessions.GetIdleLifetime().count();
        body["lifetime_seconds"] = _sessions.GetAbsoluteLifetime().count();
        body["signed_in"] = false;
        body["signed_in_with"] = nullptr;
        body["csrf"] = nullptr;
        if (!request.Authorization.empty())
        {
            AdminRequest checked = request;
            AdminAuthResult const result = _router.Authenticate(checked);
            if (result != AdminAuthResult::Ok)
                return AdminRouter::Refused(result);
            body["signed_in"] = true;
            body["signed_in_with"] = "token";
            return AdminResponse::Json(200, body.dump());
        }
        if (std::optional<std::string> const secret = _router.SessionSecret(request))
        {
            if (std::optional<std::string> const csrf = _sessions.Find(*secret))
            {
                body["signed_in"] = true;
                body["signed_in_with"] = "session";
                body["csrf"] = *csrf;
            }
        }
        return AdminResponse::Json(200, body.dump());
    });
    _router.AddOpen("DELETE", "/api/session", [this](AdminRequest const& request)
    {
        if (_sessionSource != nullptr)
            return AdminResponse::Problem(404, "no_token_sign_in", "This listener signs in with its own accounts rather than with an admin token");
        if (!request.SessionCsrf)
            return AdminResponse::Problem(400, "no_session", "Only a browser session signs out; a bearer token has no session to end");
        if (std::optional<std::string> const secret = _router.SessionSecret(request))
            _sessions.Close(*secret);
        AdminResponse response;
        response.Status = 204;
        response.ContentType.clear();
        response.Headers.emplace_back("Set-Cookie", SessionCookie(std::string(), true));
        return response;
    });
}

AdminResponse AdminServer::SignIn(AdminRequest const& request)
{
    if (_sessionSource != nullptr)
        return AdminResponse::Problem(404, "no_token_sign_in", "This listener signs in with its own accounts rather than with an admin token");
    if (!Ambrose::EqualsIgnoreCase(request.Origin, _router.ExpectedOrigin(request)))
        return AdminRouter::Refused(AdminAuthResult::Forbidden);

    nlohmann::json const body = nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object())
        return AdminResponse::Invalid("Signing in takes a JSON object holding this app's admin token", { { "token", "Enter this app's admin token" } });
    std::vector<std::pair<std::string, std::string>> fields;
    for (auto const& [key, value] : body.items())
        if (key != "token")
            fields.emplace_back(key, "Signing in takes only the token");
    auto const token = body.find("token");
    bool const hasToken = token != body.end() && token->is_string() && !Ambrose::Trim(token->get_ref<std::string const&>()).empty();
    if (!hasToken)
        fields.emplace_back("token", "Enter this app's admin token");
    if (!fields.empty())
        return AdminResponse::Invalid("Signing in takes this app's admin token and nothing else", std::move(fields));

    AdminAuthResult const result = _auth.Check(request.RemoteAddress, "Bearer " + std::string(Ambrose::Trim(token->get_ref<std::string const&>())));
    if (result == AdminAuthResult::RateLimited)
        return AdminRouter::Refused(result);
    if (result != AdminAuthResult::Ok)
        return AdminResponse::Problem(401, "wrong_token", "That is not this app's admin token");

    AdminSession const opened = _sessions.Open();
    LogPanelOrAdmin(LogLevel::Info, "A browser signed in to {} from {} (request {})", Label(), request.RemoteAddress, request.Id);
    nlohmann::json answer;
    answer["app"] = _appName;
    answer["signed_in"] = true;
    answer["signed_in_with"] = "session";
    answer["csrf"] = opened.Csrf;
    answer["idle_seconds"] = _sessions.GetIdleLifetime().count();
    answer["lifetime_seconds"] = _sessions.GetAbsoluteLifetime().count();
    AdminResponse response = AdminResponse::Json(201, answer.dump());
    response.Headers.emplace_back("Set-Cookie", SessionCookie(opened.Secret, false));
    return response;
}

std::string AdminServer::SessionCookie(std::string const& value, bool clear) const
{
    AdminBrowserAccess const browser = _router.GetBrowserAccess();
    return fmt::format("{}={}; Path=/; HttpOnly; SameSite=Strict{}{}", browser.CookieName, value, browser.Secure ? "; Secure" : "", clear ? "; Max-Age=0" : "");
}

void AdminServer::ApplyLiveSettings(ListenerSettings const& settings)
{
    _files.SetRoot(settings.DashboardFolder());
    _router.SetAllowedHosts(settings.AllowedHosts);
    _sessions.SetLifetimes(std::chrono::minutes(settings.SessionIdleMinutes), std::chrono::hours(settings.SessionLifetimeHours));
}

AdminServer::~AdminServer()
{
    Close();
}

void AdminServer::SetSessionSource(SessionSource* source)
{
    _sessionSource = source;
}

void AdminServer::SetHealthSource(std::function<AdminHealth()> health)
{
    _health = std::move(health);
}

void AdminServer::AddSocket(AdminSocketRoute route)
{
    std::lock_guard const lock(_socketMutex);
    _sockets.push_back(std::move(route));
}

AdminSocketRoute const* AdminServer::FindSocket(std::string const& path) const
{
    std::lock_guard const lock(_socketMutex);
    for (auto route = _sockets.rbegin(); route != _sockets.rend(); ++route)
        if (route->Path == path)
            return &*route;
    return nullptr;
}

bool AdminServer::IsRunning() const
{
    return _listener != nullptr;
}

uint16 AdminServer::GetPort() const
{
    return _listener ? _listener->Port : uint16{ 0 };
}

std::string AdminServer::GetBindIp() const
{
    return _listener ? _listener->BindIp : std::string();
}

std::string AdminServer::GetToken() const
{
    return _token;
}

void AdminServer::AdoptIdentity(ListenerSettings const& settings)
{
    _active.Prefix = settings.Prefix;
    _active.Label = settings.Label;
    _active.LogCategory = settings.LogCategory;
    _active.Secrets = settings.Secrets;
}

bool AdminServer::Start(ListenerSettings const& settings, std::string& error)
{
    AdoptIdentity(settings);
    if (!settings.Enable)
    {
        Close();
        _active = settings;
        return true;
    }
    if (std::optional<std::string> const refused = settings.RemoteAccessError())
    {
        error = *refused;
        return false;
    }
    AdminTokenResult const token = AdminToken::Resolve(settings, _appName, _dataFolder, _configFolder);
    if (!token.Succeeded())
    {
        error = token.Error;
        return false;
    }
    if (!token.Warning.empty())
        LogPanelOrAdmin(LogLevel::Warn, "{}", token.Warning);
    if (token.Generated)
        LogPanelOrAdmin(LogLevel::Info, "{} generated a token in {}, readable only by this user", Capitalised(), ConfigMgr::PathToUtf8(token.File));
    return Open(settings, token.Token, error);
}

std::string AdminServer::Capitalised() const
{
    std::string text = _active.Label;
    if (!text.empty())
        text.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(text.front())));
    return text;
}

bool AdminServer::SwapCertificate()
{
    if (!_listener || !_listener->Tls.IsAttached())
        return true;
    TlsCertificate fresh;
    std::string problem;
    if (!fresh.Load(_listener->Certificate.GetCertificateFile(), _listener->Certificate.GetKeyFile(), problem)
        || !_listener->Tls.Swap(fresh, problem))
    {
        LogPanelOrAdmin(LogLevel::Error, "{} keeps serving its old certificate: {}", Capitalised(), problem);
        return false;
    }
    if (fresh.GetInfo().Fingerprint != _listener->Certificate.GetInfo().Fingerprint)
    {
        _listener->Certificate = std::move(fresh);
        LogPanelOrAdmin(LogLevel::Info, "{} now serves {}", Capitalised(), _listener->Certificate.Describe());
        for (std::string const& warning : _listener->Certificate.Warnings(static_cast<int64>(std::time(nullptr))))
            LogPanelOrAdmin(LogLevel::Warn, "{}", warning);
    }
    return true;
}

bool AdminServer::Reload(ListenerSettings const& settings)
{
    AdoptIdentity(settings);
    if (!settings.Enable)
    {
        if (IsRunning())
            LogPanelOrAdmin(LogLevel::Info, "{} = 0 stopped {}", settings.Option("Enable"), Label());
        Close();
        _active = settings;
        return true;
    }

    if (std::optional<std::string> const refused = settings.RemoteAccessError())
    {
        LogPanelOrAdmin(LogLevel::Error, "{}; {} keeps {}", *refused, Label(),
            IsRunning() ? fmt::format("its binding on {}:{}", _listener->BindIp, _listener->Port) : std::string("its current state"));
        return false;
    }

    AdminTokenResult const token = AdminToken::Resolve(settings, _appName, _dataFolder, _configFolder);
    if (!token.Succeeded())
    {
        LogPanelOrAdmin(LogLevel::Error, "{}; {} keeps its current token", token.Error, Label());
        return false;
    }
    if (!token.Warning.empty())
        LogPanelOrAdmin(LogLevel::Warn, "{}", token.Warning);
    if (token.Generated)
        LogPanelOrAdmin(LogLevel::Info, "{} generated a token in {}, readable only by this user", Capitalised(), ConfigMgr::PathToUtf8(token.File));

    ListenerSettings effective = settings;
    if (IsRunning() && effective.Port == 0)
        effective.Port = _listener->Port;
    bool const rebinds = !IsRunning() || !_active.ListenerEquals(effective);
    if (!rebinds)
    {
        if (!SwapCertificate())
            return false;
        _auth.SetLimits(settings.AuthFailureBurst, settings.AuthFailuresPerSecond);
        _router.SetMaxBodyBytes(settings.MaxRequestBytes);
        ApplyLiveSettings(settings);
        if (token.Token != _token)
        {
            _token = token.Token;
            _auth.SetToken(_token);
            _sessions.CloseAll();
            LogPanelOrAdmin(LogLevel::Info, "{} token changed; the old one no longer answers and every browser session it opened has ended", Capitalised());
        }
        _active = effective;
        return true;
    }

    bool const sameEndpoint = IsRunning() && _listener->BindIp == settings.BindIp && effective.Port == _listener->Port;
    std::string reserveError;
    if (!sameEndpoint && !ReserveEndpoint(settings.BindIp, settings.Port, reserveError))
    {
        LogPanelOrAdmin(LogLevel::Error, "{} cannot bind {}:{}: {}; it keeps {}", Capitalised(), settings.BindIp, settings.Port, reserveError,
            IsRunning() ? fmt::format("its binding on {}:{}", _listener->BindIp, _listener->Port) : std::string("its current state"));
        return false;
    }

    ListenerSettings opening = settings;
    if (sameEndpoint)
        opening.Port = effective.Port;
    ListenerSettings const previous = _active;
    std::string const previousToken = _token;
    bool const wasRunning = IsRunning();
    Close();
    std::string error;
    if (!Open(opening, token.Token, error))
    {
        LogPanelOrAdmin(LogLevel::Error, "{} cannot listen on {}:{}: {}", Capitalised(), opening.BindIp, opening.Port, error);
        if (wasRunning)
        {
            std::string restoreError;
            if (Open(previous, previousToken, restoreError))
                LogPanelOrAdmin(LogLevel::Warn, "{} serves its old binding on {}:{} again", Capitalised(), previous.BindIp, previous.Port);
            else
                LogPanelOrAdmin(LogLevel::Error, "{} cannot serve its old binding on {}:{} either: {}; nothing is listening", Capitalised(), previous.BindIp, previous.Port, restoreError);
        }
        return false;
    }
    return true;
}

void AdminServer::Stop()
{
    if (IsRunning())
        LogPanelOrAdmin(LogLevel::Info, "{} on {}:{} is closing", Capitalised(), _listener->BindIp, _listener->Port);
    Close();
}

bool AdminServer::Open(ListenerSettings const& settings, std::string const& token, std::string& error)
{
    std::optional<uint16> const reserved = ReserveEndpoint(settings.BindIp, settings.Port, error);
    if (!reserved)
    {
        error = fmt::format("{} cannot bind {}:{}: {}", Capitalised(), settings.BindIp, settings.Port, error);
        return false;
    }

    std::string const previousToken = _token;
    _token = token;
    _auth.SetToken(_token);
    _auth.SetLimits(settings.AuthFailureBurst, settings.AuthFailuresPerSecond);
    _router.SetMaxBodyBytes(settings.MaxRequestBytes);
    ApplyLiveSettings(settings);
    _sessions.CloseAll();
    bool const secure = settings.HasTls();
    _router.SetSecure(secure);
    _router.SetTrustedProxies(TrustedProxies::Parse(settings.TrustedProxies, nullptr, settings.Option("TrustedProxies")));
    _router.SetBrowserAccess({ _sessionSource ? _sessionSource : &_sessions, fmt::format("{}ambrose_{}_{}", secure ? "__Host-" : "", Ambrose::ToLower(settings.Prefix), *reserved), secure });

    LogBridge().Attach(&_log);
    crow::logger::setHandler(&LogBridge());

    auto const abandon = [&](std::string const& reason)
    {
        error = reason;
        _token = previousToken;
        _auth.SetToken(_token);
        LogBridge().Attach(nullptr);
        return false;
    };

    auto listener = std::make_unique<Listener>();
    listener->BindIp = settings.BindIp;
    if (secure)
    {
        std::string problem;
        if (!listener->Certificate.Load(settings.CertificateFile, settings.PrivateKeyFile, problem))
            return abandon(fmt::format("{} cannot serve TLS: {}", Capitalised(), problem));
        asio::ssl::context context(asio::ssl::context::tls_server);
        context.set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3
            | asio::ssl::context::no_tlsv1 | asio::ssl::context::no_tlsv1_1 | asio::ssl::context::single_dh_use);
        context.set_verify_mode(asio::ssl::verify_none);
        if (!listener->Tls.Attach(context.native_handle(), listener->Certificate, problem))
            return abandon(fmt::format("{} cannot serve TLS: {}", Capitalised(), problem));
        listener->App.ssl(std::move(context));
    }
    listener->App.get_middleware<AdminGate>().Bind(&_router);
    listener->App.catchall_route()([this](crow::request const& request, crow::response& response)
    {
        Apply(response, _router.Dispatch(ToAdminRequest(request, _router)));
        response.end();
    });
    auto const takeSockets = [this, &listener, &settings](char const* pattern)
    {
        listener->App.route_dynamic(pattern).template websocket<AdminApp>(&listener->App)
            .max_payload(settings.MaxRequestBytes)
            .onaccept([this, open = listener->Open](crow::request const& request, std::optional<crow::response>& refusal, void** userdata)
            {
                AdminRequest incoming = ToAdminRequest(request, _router);
                incoming.Upgrade = true;
                incoming.Id = AdminRouter::NewRequestId();
                auto const refuse = [&](AdminResponse answer)
                {
                    _router.Finish(incoming, answer);
                    refusal = ToCrowResponse(answer);
                };
                if (!_router.HostAllowed(incoming.Host))
                {
                    refuse(AdminRouter::HostRefused(incoming.Host));
                    return;
                }
                AdminAuthResult const result = _router.Authenticate(incoming);
                if (result != AdminAuthResult::Ok)
                {
                    refuse(AdminRouter::Refused(result));
                    return;
                }
                AdminSocketRoute const* const route = FindSocket(incoming.Path);
                if (!route)
                {
                    refuse(AdminResponse::Problem(404, "not_found", "The admin API has no WebSocket on " + incoming.Path));
                    return;
                }
                *userdata = open->Accept(route);
            })
            .onopen([open = listener->Open](crow::websocket::connection& connection)
            {
                SocketBinding* const binding = BindingOf(connection);
                if (!binding)
                    return;
                std::shared_ptr<CrowAdminSocket> const socket = open->Open(binding, connection);
                if (socket && binding->Route && binding->Route->Opened)
                    binding->Route->Opened(*socket);
            })
            .onmessage([](crow::websocket::connection& connection, std::string const& message, bool binary)
            {
                SocketBinding* const binding = BindingOf(connection);
                if (!binding || !binding->Socket || !binding->Route || !binding->Route->Received)
                    return;
                binding->Route->Received(*binding->Socket, message, binary);
            })
            .onclose([open = listener->Open](crow::websocket::connection& connection, std::string const& reason, uint16_t code)
            {
                std::unique_ptr<SocketBinding> const binding = open->Release(BindingOf(connection));
                connection.userdata(nullptr);
                if (!binding)
                    return;
                std::shared_ptr<CrowAdminSocket> const socket = binding->Socket ? binding->Socket : std::make_shared<CrowAdminSocket>(connection);
                if (binding->Socket && binding->Route && binding->Route->Closed)
                    binding->Route->Closed(*socket, reason, static_cast<uint16>(code));
                socket->Detach();
            });
    };
    takeSockets(RootRoutePattern);
    takeSockets(SocketRoutePattern);

    listener->App.signal_clear();
    listener->App.loglevel(crow::LogLevel::Warning);
    listener->App.server_name("Ambrose");
    listener->App.websocket_max_payload(settings.MaxRequestBytes);
    listener->App.bindaddr(settings.BindIp);
    listener->App.port(*reserved);
    listener->App.concurrency(settings.Threads);
    listener->Worker = listener->App.run_async();
    if (listener->App.wait_for_server_start(std::chrono::milliseconds(10000)) == std::cv_status::timeout)
    {
        listener->App.stop();
        return abandon(fmt::format("{} did not start on {}:{}", Capitalised(), settings.BindIp, *reserved));
    }

    uint16 bound = 0;
    try
    {
        bound = listener->App.port();
    }
    catch (std::exception const&)
    {
        bound = 0;
    }
    if (bound == 0)
    {
        listener->App.stop();
        return abandon(fmt::format("{} could not bind {}:{}", Capitalised(), settings.BindIp, *reserved));
    }

    listener->Port = bound;
    _listener = std::move(listener);
    _active = settings;
    _active.Port = bound;
    LogPanelOrAdmin(LogLevel::Info, "{} is listening on {}://{}:{}", Capitalised(), secure ? "https" : "http", _listener->BindIp, _listener->Port);
    if (secure)
        LogPanelOrAdmin(LogLevel::Info, "It serves {}", _listener->Certificate.Describe());
    for (std::string const& warning : settings.Warnings())
        LogPanelOrAdmin(LogLevel::Warn, "{}", warning);
    if (secure)
    {
        for (std::string const& warning : _listener->Certificate.Warnings(static_cast<int64>(std::time(nullptr))))
            LogPanelOrAdmin(LogLevel::Warn, "{}", warning);
    }
    return true;
}

void AdminServer::Close()
{
    _sessions.CloseAll();
    if (_listener)
    {
        _listener->App.stop();
        if (_listener->Worker.valid())
            _listener->Worker.wait();
        for (std::unique_ptr<SocketBinding> const& binding : _listener->Open->TakeAll())
        {
            if (!binding->Socket)
                continue;
            binding->Socket->Detach();
            if (binding->Route && binding->Route->Closed)
                binding->Route->Closed(*binding->Socket, "the admin API listener stopped", 1001);
        }
        _listener.reset();
    }
    LogBridge().Attach(nullptr);
}
