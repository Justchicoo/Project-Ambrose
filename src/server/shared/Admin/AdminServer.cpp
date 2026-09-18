/*
 * Project Ambrose by Imjustchico
 * Runs the admin API on Crow: it resolves the token, refuses a bind the remote-access rule forbids, says out loud what a bind it allows still costs, proves no other socket holds the address before Crow takes it, answers every request from the shared table in a middleware that runs before Crow's own routing, answers from the same table again after it the requests Crow replies to before a connection has an address of its own, such as OPTIONS, hands only a real WebSocket upgrade to the route registered for it under the same authentication, and on a reload rotates the token live, rebinds a changed address, or brings the old listener back when the new one cannot bind.
 */

#include "AdminServer.h"
#include "AdminToken.h"
#include "ConfigMgr.h"
#include "IpAddress.h"
#include "Log.h"

#include <crow.h>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <future>
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

    class CrowAdminSocket : public AdminSocket
    {
    public:
        explicit CrowAdminSocket(crow::websocket::connection& connection) : _connection(connection) {}

        void SendText(std::string text) override { _connection.send_text(std::move(text)); }
        void Close(std::string reason) override { _connection.close(reason); }
        std::string GetRemoteAddress() override { return _connection.get_remote_ip(); }

    private:
        crow::websocket::connection& _connection;
    };

    AdminRequest ToAdminRequest(crow::request const& request)
    {
        AdminRequest incoming;
        incoming.Method = crow::method_name(request.method);
        incoming.Path = request.url;
        incoming.RemoteAddress = request.remote_ip_address;
        incoming.Authorization = request.get_header_value("Authorization");
        incoming.Body = request.body;
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
            Apply(response, _router->Dispatch(ToAdminRequest(request)));
            response.end();
        }

        void after_handle(crow::request& request, crow::response& response, context&)
        {
            if (!_router || !request.remote_ip_address.empty())
                return;
            Apply(response, _router->Dispatch(ToAdminRequest(request)));
        }

    private:
        AdminRouter* _router = nullptr;
    };

    using AdminApp = crow::App<AdminGate>;

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

    AdminSocketRoute const* RouteOf(crow::websocket::connection& connection)
    {
        return static_cast<AdminSocketRoute const*>(connection.userdata());
    }
}

struct AdminServer::Listener
{
    AdminApp App;
    std::future<void> Worker;
    std::string BindIp;
    uint16 Port = 0;
};

AdminServer::AdminServer(Log& log, std::string appName, std::filesystem::path dataFolder, std::filesystem::path configFolder)
    : _log(log), _appName(std::move(appName)), _dataFolder(std::move(dataFolder)), _configFolder(std::move(configFolder)), _auth(AdminSettings{}.AuthFailureBurst, AdminSettings{}.AuthFailuresPerSecond), _router(_auth)
{
    _router.SetMaxBodyBytes(AdminSettings{}.MaxRequestBytes);
    _router.Add("GET", "/api/health", [this](AdminRequest const&)
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
}

AdminServer::~AdminServer()
{
    Close();
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

bool AdminServer::Start(AdminSettings const& settings, std::string& error)
{
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
        AMBROSE_LOG(_log, LogLevel::Warn, "server.admin", "{}", token.Warning);
    if (token.Generated)
        AMBROSE_LOG(_log, LogLevel::Info, "server.admin", "The admin API generated a token in {}, readable only by this user", ConfigMgr::PathToUtf8(token.File));
    return Open(settings, token.Token, error);
}

bool AdminServer::Reload(AdminSettings const& settings)
{
    if (!settings.Enable)
    {
        if (IsRunning())
            AMBROSE_LOG(_log, LogLevel::Info, "server.admin", "Admin.Enable = 0 stopped the admin API");
        Close();
        _active = settings;
        return true;
    }

    if (std::optional<std::string> const refused = settings.RemoteAccessError())
    {
        AMBROSE_LOG(_log, LogLevel::Error, "server.admin", "{}; the admin API keeps {}", *refused,
            IsRunning() ? fmt::format("its binding on {}:{}", _listener->BindIp, _listener->Port) : std::string("its current state"));
        return false;
    }

    AdminTokenResult const token = AdminToken::Resolve(settings, _appName, _dataFolder, _configFolder);
    if (!token.Succeeded())
    {
        AMBROSE_LOG(_log, LogLevel::Error, "server.admin", "{}; the admin API keeps its current token", token.Error);
        return false;
    }
    if (!token.Warning.empty())
        AMBROSE_LOG(_log, LogLevel::Warn, "server.admin", "{}", token.Warning);
    if (token.Generated)
        AMBROSE_LOG(_log, LogLevel::Info, "server.admin", "The admin API generated a token in {}, readable only by this user", ConfigMgr::PathToUtf8(token.File));

    AdminSettings effective = settings;
    if (IsRunning() && effective.Port == 0)
        effective.Port = _listener->Port;
    bool const rebinds = !IsRunning() || !_active.ListenerEquals(effective);
    if (!rebinds)
    {
        _auth.SetLimits(settings.AuthFailureBurst, settings.AuthFailuresPerSecond);
        _router.SetMaxBodyBytes(settings.MaxRequestBytes);
        if (token.Token != _token)
        {
            _token = token.Token;
            _auth.SetToken(_token);
            AMBROSE_LOG(_log, LogLevel::Info, "server.admin", "The admin API token changed; the old one no longer answers");
        }
        _active = effective;
        return true;
    }

    bool const sameEndpoint = IsRunning() && _listener->BindIp == settings.BindIp && effective.Port == _listener->Port;
    std::string reserveError;
    if (!sameEndpoint && !ReserveEndpoint(settings.BindIp, settings.Port, reserveError))
    {
        AMBROSE_LOG(_log, LogLevel::Error, "server.admin", "The admin API cannot bind {}:{}: {}; it keeps {}", settings.BindIp, settings.Port, reserveError,
            IsRunning() ? fmt::format("its binding on {}:{}", _listener->BindIp, _listener->Port) : std::string("its current state"));
        return false;
    }

    AdminSettings opening = settings;
    if (sameEndpoint)
        opening.Port = effective.Port;
    AdminSettings const previous = _active;
    std::string const previousToken = _token;
    bool const wasRunning = IsRunning();
    Close();
    std::string error;
    if (!Open(opening, token.Token, error))
    {
        AMBROSE_LOG(_log, LogLevel::Error, "server.admin", "The admin API cannot listen on {}:{}: {}", opening.BindIp, opening.Port, error);
        if (wasRunning)
        {
            std::string restoreError;
            if (Open(previous, previousToken, restoreError))
                AMBROSE_LOG(_log, LogLevel::Warn, "server.admin", "The admin API serves its old binding on {}:{} again", previous.BindIp, previous.Port);
            else
                AMBROSE_LOG(_log, LogLevel::Error, "server.admin", "The admin API cannot serve its old binding on {}:{} either: {}; nothing is listening", previous.BindIp, previous.Port, restoreError);
        }
        return false;
    }
    return true;
}

void AdminServer::Stop()
{
    if (IsRunning())
        AMBROSE_LOG(_log, LogLevel::Info, "server.admin", "The admin API on {}:{} is closing", _listener->BindIp, _listener->Port);
    Close();
}

bool AdminServer::Open(AdminSettings const& settings, std::string const& token, std::string& error)
{
    std::optional<uint16> const reserved = ReserveEndpoint(settings.BindIp, settings.Port, error);
    if (!reserved)
    {
        error = fmt::format("the admin API cannot bind {}:{}: {}", settings.BindIp, settings.Port, error);
        return false;
    }

    std::string const previousToken = _token;
    _token = token;
    _auth.SetToken(_token);
    _auth.SetLimits(settings.AuthFailureBurst, settings.AuthFailuresPerSecond);
    _router.SetMaxBodyBytes(settings.MaxRequestBytes);

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
    listener->App.get_middleware<AdminGate>().Bind(&_router);
    listener->App.catchall_route()([this](crow::request const& request, crow::response& response)
    {
        Apply(response, _router.Dispatch(ToAdminRequest(request)));
        response.end();
    });
    auto const takeSockets = [this, &listener, &settings](char const* pattern)
    {
        listener->App.route_dynamic(pattern).template websocket<AdminApp>(&listener->App)
            .max_payload(settings.MaxRequestBytes)
            .onaccept([this](crow::request const& request, std::optional<crow::response>& refusal, void** userdata)
            {
                AdminRequest const incoming = ToAdminRequest(request);
                AdminAuthResult const result = _router.Authenticate(incoming);
                if (result != AdminAuthResult::Ok)
                {
                    refusal = ToCrowResponse(AdminRouter::Refused(result));
                    return;
                }
                AdminSocketRoute const* const route = FindSocket(incoming.Path);
                if (!route)
                {
                    refusal = ToCrowResponse(AdminResponse::Problem(404, "not_found", "The admin API has no WebSocket on " + incoming.Path));
                    return;
                }
                *userdata = const_cast<AdminSocketRoute*>(route);
            })
            .onopen([](crow::websocket::connection& connection)
            {
                AdminSocketRoute const* const route = RouteOf(connection);
                if (!route || !route->Opened)
                    return;
                CrowAdminSocket socket(connection);
                route->Opened(socket);
            })
            .onmessage([](crow::websocket::connection& connection, std::string const& message, bool binary)
            {
                AdminSocketRoute const* const route = RouteOf(connection);
                if (!route || !route->Received)
                    return;
                CrowAdminSocket socket(connection);
                route->Received(socket, message, binary);
            })
            .onclose([](crow::websocket::connection& connection, std::string const& reason, uint16_t code)
            {
                AdminSocketRoute const* const route = RouteOf(connection);
                if (!route || !route->Closed)
                    return;
                CrowAdminSocket socket(connection);
                route->Closed(socket, reason, static_cast<uint16>(code));
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
        return abandon(fmt::format("the admin API did not start on {}:{}", settings.BindIp, *reserved));
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
        return abandon(fmt::format("the admin API could not bind {}:{}", settings.BindIp, *reserved));
    }

    listener->Port = bound;
    _listener = std::move(listener);
    _active = settings;
    _active.Port = bound;
    AMBROSE_LOG(_log, LogLevel::Info, "server.admin", "The admin API is listening on http://{}:{}", _listener->BindIp, _listener->Port);
    for (std::string const& warning : settings.Warnings())
        AMBROSE_LOG(_log, LogLevel::Warn, "server.admin", "{}", warning);
    return true;
}

void AdminServer::Close()
{
    if (_listener)
    {
        _listener->App.stop();
        if (_listener->Worker.valid())
            _listener->Worker.wait();
        _listener.reset();
    }
    LogBridge().Attach(nullptr);
}
