/*
 * Project Ambrose by Imjustchico
 * Runs the event socket: a connection is kept from its upgrade until its close with the request that opened it, whether it has said hello, who it is and the stream sessions it follows; frames are read on the listener's own thread and each connection's state changes under its own lock, never under the table's, so the sweeper and a closing socket never wait on each other. A caller's permissions are read once, at hello, from its role and its grants, and a ticket's scopes narrow what it may follow. A frame outside a stream is written only for a type the catalog marks as sent, so a handler that answers with anything else fails rather than reach a page with no handler for it, and a second resume of one stream at one scope closes the session it replaces before the new one opens. Each stream session writes through a sink that drops the layer's own hello, turns a dropped range into a dropped frame naming its stream and closes the socket with 4429 when a stream that never drops overflows. The sweeper wakes every second and closes with 4401 any socket that has not said hello within ten seconds of opening.
 */

#include "PanelEventSocket.h"
#include "ConstantTime.h"
#include "Log.h"
#include "PanelEventCatalog.h"
#include "PanelGrants.h"
#include "PanelPermissions.h"
#include "PanelUsers.h"
#include "StringUtil.h"
#include "ThreadName.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr char const* PanelCategory = "server.panel";

    class PanelStreamSink final : public StreamSink
    {
    public:
        PanelStreamSink(std::shared_ptr<AdminSocket> socket, std::string stream, std::string app)
            : _socket(std::move(socket)), _stream(std::move(stream)), _app(std::move(app))
        {
        }

        void Send(std::string text) override
        {
            _socket->SendText(std::move(text));
        }

        void Close(std::string) override
        {
        }

        void Opened(uint64, uint64, std::size_t) override
        {
        }

        void Dropped(uint64 from, uint64 to, uint64 count) override
        {
            _socket->SendText(PanelEventFrame::Write("dropped", std::nullopt, _app, std::nullopt, PanelEventFrame::NowMs(), PanelEventFrame::DroppedData(_stream, from, to, count)));
        }

        void Overflowed() override
        {
            _socket->CloseWith(PanelEventCatalog::Limits, PanelEventSocket::CloseReason("the " + _stream + " stream fell too far behind; resume after the last sequence seen"));
        }

    private:
        std::shared_ptr<AdminSocket> _socket;
        std::string _stream;
        std::string _app;
    };

    std::string Listed(std::vector<std::string> const& apps)
    {
        std::string text;
        for (std::string const& app : apps)
            text += (text.empty() ? "" : ", ") + app;
        return text;
    }
}

struct PanelEventSocket::Connection
{
    struct Following
    {
        std::string Stream = {};
        std::shared_ptr<PanelEventSession> Session = {};
    };

    std::shared_ptr<AdminSocket> Socket = {};
    AdminRequest Upgrade = {};
    Clock::time_point OpenedAt = {};
    std::mutex Mutex;
    bool Ready = false;
    bool Closing = false;
    PanelEventCaller Caller = {};
    std::map<std::string, Following> Sessions = {};
};

bool PanelEventCaller::Holds(std::string_view permission) const
{
    return std::find(Panel.begin(), Panel.end(), permission) != Panel.end();
}

bool PanelEventCaller::HoldsAnythingOn(std::string_view app) const
{
    if (Scopes && std::find(Scopes->begin(), Scopes->end(), app) == Scopes->end())
        return false;
    if (!Panel.empty())
        return true;
    auto const found = Apps.find(app);
    return found != Apps.end() && !found->second.empty();
}

PermissionVerdict PanelEventCaller::Weigh(std::string_view permission, std::string_view app) const
{
    if (Scopes)
    {
        if (app.empty())
            return PermissionVerdict::Forbidden;
        if (std::find(Scopes->begin(), Scopes->end(), app) == Scopes->end())
            return PermissionVerdict::OutOfScope;
    }
    auto const found = app.empty() ? Apps.end() : Apps.find(app);
    bool const holdsAnythingHere = found != Apps.end() && !found->second.empty();
    bool const grantedHere = holdsAnythingHere && std::find(found->second.begin(), found->second.end(), permission) != found->second.end();
    if (Asking)
        return PanelAuthorization::Weigh(*Asking, permission, grantedHere, holdsAnythingHere, !app.empty());
    if (grantedHere)
        return PermissionVerdict::Allowed;
    return !app.empty() && !holdsAnythingHere ? PermissionVerdict::OutOfScope : PermissionVerdict::Forbidden;
}

PanelEventSocket::Call::Call(PanelEventSocket& socket, std::shared_ptr<Connection> connection, PanelIncomingFrame const& frame)
    : _socket(socket), _connection(std::move(connection)), _frame(frame)
{
}

PanelEventCaller PanelEventSocket::Call::Caller() const
{
    std::lock_guard const lock(_connection->Mutex);
    return _connection->Caller;
}

void PanelEventSocket::Call::Answer(std::string_view type, std::string const& data) const
{
    _socket.Send(*_connection, type, _frame.Id, data);
}

void PanelEventSocket::Call::Refuse(std::string_view code, std::string const& message) const
{
    _socket.Report(*_connection, _frame.Id, code, message, false);
}

void PanelEventSocket::Call::Fail(std::string const& text) const
{
    _socket.Report(*_connection, _frame.Id, "failed", text, true);
}

PanelEventSocket::PanelEventSocket(Log& log, PanelEventStreams& streams, PanelEventTickets& tickets, SessionSource& sessions, PanelUsers& users, PanelGrants& grants, AdminRouter& routes)
    : _log(log), _streams(streams), _tickets(tickets), _sessions(sessions), _users(users), _grants(grants), _routes(routes)
{
    Handle("ping", [](Call const& call) { call.Answer("pong", "{}"); });
    Handle("resume", [this](Call const& call) { Resume(call); });
}

PanelEventSocket::~PanelEventSocket()
{
    Stop();
}

AdminSocketRoute PanelEventSocket::MakeRoute()
{
    AdminSocketRoute route;
    route.Path = std::string(Path);
    route.Admit = [this](AdminRequest const& request) { return Admit(request); };
    route.Opened = [this](AdminSocket& socket) { Opened(socket); };
    route.Received = [this](AdminSocket& socket, std::string const& message, bool binary) { Received(socket, message, binary); };
    route.Closed = [this](AdminSocket& socket, std::string const&, uint16) { Closed(socket); };
    return route;
}

std::optional<AdminResponse> PanelEventSocket::Admit(AdminRequest const& request)
{
    if (request.HasQuery || !request.QueryValues.empty())
    {
        if (auto const ticket = request.QueryValues.find("ticket"); ticket != request.QueryValues.end())
            _tickets.Burn(ticket->second);
        return AdminResponse::Problem(400, "credentials_in_url", "The panel's event socket takes nothing in its address; a script sends its ticket in the hello frame, which also spends any ticket this address named");
    }
    if (!request.Origin.empty() && !Ambrose::EqualsIgnoreCase(request.Origin, _routes.ExpectedOrigin(request)))
        return AdminResponse::Problem(403, "cross_origin", "The panel's event socket opens only from the panel's own page");
    return std::nullopt;
}

AdminResponse PanelEventSocket::MintTicket(AdminRequest const& request)
{
    if (request.SessionCsrf)
        return AdminResponse::Problem(403, "session_signs_in", "A browser signs the event socket in with its session and CSRF token; tickets are for scripts with an API credential");
    nlohmann::json const body = Ambrose::Trim(request.Body).empty() ? nlohmann::json::object() : nlohmann::json::parse(request.Body, nullptr, false);
    if (!body.is_object())
        return AdminResponse::Invalid("A ticket request takes a JSON object, or nothing", { { "scopes", "Give a list of scopes such as [{\"app\":\"gameserver-1\"}], or leave it out for the whole panel" } });
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<std::string> apps;
    for (auto entry = body.begin(); entry != body.end(); ++entry)
    {
        if (entry.key() != "scopes")
        {
            fields.emplace_back(entry.key(), "A ticket request takes only scopes");
            continue;
        }
        nlohmann::json const& scopes = entry.value();
        bool readable = scopes.is_array();
        if (readable)
        {
            for (nlohmann::json const& scope : scopes)
            {
                auto const app = scope.is_object() ? scope.find("app") : scope.end();
                if (!scope.is_object() || scope.size() != 1 || app == scope.end() || !app->is_string() || app->get_ref<std::string const&>().empty())
                {
                    readable = false;
                    break;
                }
                apps.push_back(app->get<std::string>());
            }
        }
        if (!readable)
            fields.emplace_back("scopes", "Give a list of scopes, each naming one app, such as [{\"app\":\"gameserver-1\"}]");
    }
    if (!fields.empty())
        return AdminResponse::Invalid("The ticket request has problems", std::move(fields));

    std::string failure;
    std::optional<PanelEventCaller> const caller = CallerOf(request.Principal, failure);
    if (!caller)
    {
        if (!failure.empty())
            return AdminResponse::Problem(503, "store_unavailable", "The panel could not read who is asking; try again");
        return AdminResponse::Problem(403, "forbidden", "This account cannot take a ticket");
    }
    for (std::string const& app : apps)
        if (!KnowsApp(app) || !caller->HoldsAnythingOn(app))
            return AdminResponse::Problem(404, "not_found", "The panel has no app named " + Ambrose::ForLog(app, 64));

    std::string const ticket = _tickets.Mint(request.Principal, request.RemoteAddress, apps);
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} took an event socket ticket from {} for {} (request {})", request.Principal, request.RemoteAddress,
        apps.empty() ? std::string("the whole panel") : Listed(apps), request.Id);
    nlohmann::json answer;
    answer["ticket"] = ticket;
    answer["expires_in"] = PanelEventTickets::Lifetime.count();
    return AdminResponse::Json(201, answer.dump());
}

void PanelEventSocket::Handle(std::string type, Handler handler)
{
    std::lock_guard const lock(_mutex);
    _handlers.insert_or_assign(std::move(type), std::move(handler));
}

std::vector<std::string> PanelEventSocket::HandledTypes() const
{
    std::vector<std::string> types{ "hello" };
    std::lock_guard const lock(_mutex);
    for (auto const& entry : _handlers)
        types.push_back(entry.first);
    return types;
}

void PanelEventSocket::SetAppSource(AppSource source)
{
    std::lock_guard const lock(_mutex);
    _appSource = std::move(source);
}

void PanelEventSocket::SetTimeSource(TimeSource source)
{
    std::lock_guard const lock(_clockMutex);
    _timeSource = std::move(source);
}

PanelEventSocket::Clock::time_point PanelEventSocket::Now() const
{
    TimeSource source;
    {
        std::lock_guard const lock(_clockMutex);
        source = _timeSource;
    }
    return source ? source() : Clock::now();
}

void PanelEventSocket::Start()
{
    {
        std::lock_guard const lock(_sweepMutex);
        if (_sweeping)
            return;
        _sweeping = true;
    }
    _sweeper = std::thread([this]
    {
        Ambrose::Threading::SetCurrentThreadName("Panel sockets");
        std::unique_lock lock(_sweepMutex);
        while (_sweeping)
        {
            _sweepWake.wait_for(lock, SweepInterval, [this] { return !_sweeping; });
            if (!_sweeping)
                return;
            lock.unlock();
            Sweep();
            lock.lock();
        }
    });
}

void PanelEventSocket::Stop()
{
    {
        std::lock_guard const lock(_sweepMutex);
        _sweeping = false;
    }
    _sweepWake.notify_all();
    if (_sweeper.joinable())
        _sweeper.join();
}

std::size_t PanelEventSocket::Sweep()
{
    Clock::time_point const now = Now();
    std::vector<std::shared_ptr<Connection>> late;
    {
        std::lock_guard const lock(_mutex);
        for (auto const& entry : _connections)
        {
            std::lock_guard const held(entry.second->Mutex);
            if (!entry.second->Ready && !entry.second->Closing && now - entry.second->OpenedAt >= HelloDeadline)
                late.push_back(entry.second);
        }
    }
    for (std::shared_ptr<Connection> const& connection : late)
        Shut(*connection, PanelEventCatalog::SessionEnded, "no hello arrived within 10 seconds of opening");
    return late.size();
}

std::size_t PanelEventSocket::GetConnectionCount() const
{
    std::lock_guard const lock(_mutex);
    return _connections.size();
}

std::string PanelEventSocket::CloseReason(std::string_view text)
{
    std::string reason;
    for (char const character : text)
    {
        if (reason.size() >= MaxCloseReason)
            break;
        if (character >= 0x20 && character <= 0x7E)
            reason.push_back(character);
    }
    return reason;
}

std::string PanelEventSocket::ReadyData(PanelEventCaller const& caller, std::string_view instance, std::vector<std::string> const& apps, int64 nowMs)
{
    nlohmann::json granted = nlohmann::json::object();
    for (auto const& [app, keys] : caller.Apps)
        granted[app] = keys;
    nlohmann::json permissions = nlohmann::json::object();
    permissions["panel"] = caller.Panel;
    permissions["apps"] = std::move(granted);
    nlohmann::json list = nlohmann::json::array();
    for (std::string const& app : apps)
    {
        nlohmann::json entry = nlohmann::json::object();
        entry["name"] = app;
        list.push_back(std::move(entry));
    }
    nlohmann::json data = nlohmann::json::object();
    data["version"] = PanelEventCatalog::Version;
    data["server_time"] = nowMs;
    data["instance"] = std::string(instance);
    data["permissions"] = std::move(permissions);
    data["apps"] = std::move(list);
    data["realms"] = nlohmann::json::array();
    return data.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string PanelEventSocket::ErrorData(std::optional<std::string> const& request, std::string_view code, std::string_view message, std::string_view correlation)
{
    nlohmann::json data = nlohmann::json::object();
    data["request"] = request ? nlohmann::json(*request) : nlohmann::json(nullptr);
    data["code"] = std::string(code);
    data["message"] = std::string(message);
    data["correlation"] = std::string(correlation);
    return data.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::shared_ptr<PanelEventSocket::Connection> PanelEventSocket::Find(AdminSocket& socket) const
{
    std::lock_guard const lock(_mutex);
    auto const found = _connections.find(&socket);
    return found == _connections.end() ? nullptr : found->second;
}

std::vector<std::string> PanelEventSocket::Apps() const
{
    AppSource source;
    {
        std::lock_guard const lock(_mutex);
        source = _appSource;
    }
    return source ? source() : std::vector<std::string>();
}

std::vector<std::string> PanelEventSocket::VisibleApps(PanelEventCaller const& caller) const
{
    std::vector<std::string> visible;
    for (std::string const& app : Apps())
        if (caller.HoldsAnythingOn(app))
            visible.push_back(app);
    return visible;
}

bool PanelEventSocket::KnowsApp(std::string_view app) const
{
    std::vector<std::string> const apps = Apps();
    return std::find(apps.begin(), apps.end(), app) != apps.end();
}

std::optional<PanelEventCaller> PanelEventSocket::CallerOf(std::string const& principal, std::string& failure)
{
    PanelEventCaller caller;
    caller.Principal = principal;
    caller.Name = principal;
    constexpr std::string_view Signed = "user:";
    if (!principal.starts_with(Signed))
        return caller;
    std::optional<int64> const id = Ambrose::StringTo<int64>(std::string_view(principal).substr(Signed.size()));
    if (!id)
        return std::nullopt;
    std::string error;
    std::optional<PanelUser> const user = _users.FindById(*id, error);
    if (!user)
    {
        failure = error;
        return std::nullopt;
    }
    if (user->Disabled)
        return std::nullopt;
    caller.Name = user->Username;
    caller.Asking = PanelAsking{ user->Id, user->Role, user->IsOwner };
    for (std::string_view const key : PanelPermissions::KeysOf(user->Role))
        caller.Panel.emplace_back(key);
    std::vector<PanelGrant> const grants = _grants.Of(user->Id, error);
    if (!error.empty())
    {
        failure = error;
        return std::nullopt;
    }
    for (PanelGrant const& grant : grants)
        caller.Apps[grant.App].push_back(grant.Permission);
    return caller;
}

void PanelEventSocket::Opened(AdminSocket& socket)
{
    std::shared_ptr<AdminSocket> const kept = socket.Keep();
    if (!kept)
    {
        socket.CloseWith(InternalError, "this socket cannot be kept open");
        return;
    }
    auto connection = std::make_shared<Connection>();
    connection->Socket = kept;
    connection->Upgrade = socket.GetUpgrade();
    connection->OpenedAt = Now();
    std::lock_guard const lock(_mutex);
    _connections[&socket] = std::move(connection);
}

void PanelEventSocket::Closed(AdminSocket& socket)
{
    std::shared_ptr<Connection> connection;
    {
        std::lock_guard const lock(_mutex);
        auto const found = _connections.find(&socket);
        if (found == _connections.end())
            return;
        connection = std::move(found->second);
        _connections.erase(found);
    }
    std::map<std::string, Connection::Following> sessions;
    {
        std::lock_guard const lock(connection->Mutex);
        connection->Closing = true;
        sessions.swap(connection->Sessions);
    }
    for (auto const& entry : sessions)
        _streams.Close(entry.second.Stream, entry.second.Session);
}

void PanelEventSocket::Received(AdminSocket& socket, std::string const& message, bool binary)
{
    std::shared_ptr<Connection> const connection = Find(socket);
    if (!connection)
        return;
    bool ready = false;
    {
        std::lock_guard const lock(connection->Mutex);
        if (connection->Closing)
            return;
        ready = connection->Ready;
    }
    std::string problem;
    std::optional<PanelIncomingFrame> const frame = PanelEventFrame::Read(message, binary, problem);
    if (!frame)
    {
        Shut(*connection, PanelEventCatalog::Malformed, problem);
        return;
    }
    if (!ready)
    {
        Hello(connection, *frame);
        return;
    }
    Dispatch(connection, *frame);
}

void PanelEventSocket::Shut(Connection& connection, uint16 code, std::string_view reason)
{
    {
        std::lock_guard const lock(connection.Mutex);
        if (connection.Closing)
            return;
        connection.Closing = true;
    }
    connection.Socket->CloseWith(code, CloseReason(reason));
}

void PanelEventSocket::Send(Connection& connection, std::string_view type, std::optional<std::string> const& id, std::string const& data)
{
    if (!PanelEventCatalog::IsSent(type, ""))
        throw std::logic_error(fmt::format("{} is not a message the catalog marks as sent outside a stream, so no page handles it", Ambrose::ForLog(type, 64)));
    connection.Socket->SendText(PanelEventFrame::Write(type, id, "", std::nullopt, PanelEventFrame::NowMs(), data));
}

void PanelEventSocket::Report(Connection& connection, std::optional<std::string> const& id, std::string_view code, std::string const& text, bool internal)
{
    std::string const correlation = AdminRouter::NewRequestId();
    PanelEventCaller caller;
    {
        std::lock_guard const lock(connection.Mutex);
        caller = connection.Caller;
    }
    bool const full = !internal || caller.Holds("debug.errors");
    std::string const message = full ? text : fmt::format("The panel could not do that; quote {} to whoever runs it", correlation);
    std::string const who = caller.Principal.empty() ? std::string("a socket that has not signed in") : fmt::format("{} ({})", caller.Name, caller.Principal);
    AMBROSE_LOG(_log, internal ? LogLevel::Error : LogLevel::Info, PanelCategory, "The event socket answered {} from {} with {} (correlation {}): {}",
        who, connection.Upgrade.RemoteAddress, code, correlation, text);
    Send(connection, "error", id, ErrorData(id, code, message, correlation));
}

void PanelEventSocket::Hello(std::shared_ptr<Connection> const& connection, PanelIncomingFrame const& frame)
{
    if (frame.Type != "hello")
    {
        Shut(*connection, PanelEventCatalog::Malformed, "the first frame must be hello");
        return;
    }
    std::string problem;
    if (!PanelEventCatalog::Validate(PanelEventDirection::FromClient, "hello", frame.Data, problem))
    {
        Shut(*connection, PanelEventCatalog::Malformed, problem);
        return;
    }
    nlohmann::json const& version = frame.Data.at("version");
    if (!version.is_number_unsigned() || version.get<uint64>() != PanelEventCatalog::Version)
    {
        Shut(*connection, PanelEventCatalog::Malformed, fmt::format("protocol version {} is not spoken here; this panel speaks {}", version.dump(), PanelEventCatalog::Version));
        return;
    }
    bool const withCsrf = frame.Data.contains("csrf");
    bool const withTicket = frame.Data.contains("ticket");
    if (withCsrf == withTicket)
    {
        Shut(*connection, PanelEventCatalog::Malformed, "hello carries the session's CSRF token or a ticket, and not both");
        return;
    }

    AdminRequest const& upgrade = connection->Upgrade;
    std::optional<std::string> principal;
    std::optional<std::vector<std::string>> scopes;
    std::string refusal;
    if (withCsrf)
    {
        std::optional<std::string> const secret = _routes.SessionSecret(upgrade);
        std::optional<SessionHolder> const held = secret ? _sessions.Hold(*secret) : std::nullopt;
        if (upgrade.Origin.empty() || !Ambrose::EqualsIgnoreCase(upgrade.Origin, _routes.ExpectedOrigin(upgrade)))
            refusal = "a browser signs in only from the panel's own page";
        else if (!held)
            refusal = "this socket carries no signed-in session";
        else if (!Ambrose::Crypto::ConstantTimeEquals(frame.Data.at("csrf").get_ref<std::string const&>(), held->Csrf))
            refusal = "the CSRF token is not this session's";
        else
            principal = held->Principal;
    }
    else
    {
        std::optional<PanelTicket> const ticket = _tickets.Redeem(frame.Data.at("ticket").get_ref<std::string const&>(), upgrade.RemoteAddress);
        if (!ticket)
            refusal = "the ticket is unknown, spent, expired or from another address";
        else
        {
            principal = ticket->Principal;
            if (!ticket->Apps.empty())
                scopes = ticket->Apps;
        }
    }
    if (!principal)
    {
        AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "An event socket from {} was not signed in: {}", upgrade.RemoteAddress, refusal);
        Shut(*connection, PanelEventCatalog::SessionEnded, refusal);
        return;
    }

    std::string failure;
    std::optional<PanelEventCaller> caller = CallerOf(*principal, failure);
    if (!caller)
    {
        if (!failure.empty())
        {
            Report(*connection, frame.Id, "failed", "The panel could not read who is signing in: " + failure, true);
            Shut(*connection, InternalError, "the panel could not read who is signing in");
            return;
        }
        AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "An event socket from {} was not signed in: {} cannot sign in", upgrade.RemoteAddress, *principal);
        Shut(*connection, PanelEventCatalog::SessionEnded, "the account behind this session cannot sign in");
        return;
    }
    caller->Scopes = std::move(scopes);
    {
        std::lock_guard const lock(connection->Mutex);
        if (connection->Closing)
            return;
        connection->Caller = *caller;
        connection->Ready = true;
    }
    Send(*connection, "ready", frame.Id, ReadyData(*caller, _streams.Instance(), VisibleApps(*caller), PanelEventFrame::NowMs()));
    AMBROSE_LOG(_log, LogLevel::Info, PanelCategory, "{} signed in to the event socket from {} with {}", caller->Name, upgrade.RemoteAddress, withCsrf ? "a browser session" : "a ticket");
}

void PanelEventSocket::Dispatch(std::shared_ptr<Connection> const& connection, PanelIncomingFrame const& frame)
{
    Call const call(*this, connection, frame);
    if (!PanelEventCatalog::Find(PanelEventDirection::FromClient, frame.Type))
    {
        bool const sentByServer = PanelEventCatalog::Find(PanelEventDirection::FromServer, frame.Type) != nullptr;
        call.Refuse("unknown_type", sentByServer ? fmt::format("{} is a message the panel sends, not one it takes", frame.Type)
                                                 : fmt::format("The panel's event socket takes no message called {}", Ambrose::ForLog(frame.Type, 64)));
        return;
    }
    if (frame.Type == "hello")
    {
        call.Refuse("invalid", "This socket has already said hello");
        return;
    }
    std::string problem;
    if (!PanelEventCatalog::Validate(PanelEventDirection::FromClient, frame.Type, frame.Data, problem))
    {
        call.Refuse("invalid", problem);
        return;
    }
    Handler handler;
    {
        std::lock_guard const lock(_mutex);
        auto const found = _handlers.find(frame.Type);
        if (found != _handlers.end())
            handler = found->second;
    }
    if (!handler)
    {
        call.Refuse("unsupported", fmt::format("This panel does not take {} yet", frame.Type));
        return;
    }
    try
    {
        handler(call);
    }
    catch (std::exception const& failure)
    {
        call.Fail(fmt::format("{} failed: {}", frame.Type, failure.what()));
    }
}

void PanelEventSocket::Resume(Call const& call)
{
    PanelIncomingFrame const& frame = call.Frame();
    std::string const stream = frame.Data.at("stream").get<std::string>();
    PanelEventStream const* const known = PanelEventCatalog::FindStream(stream);
    if (!known)
    {
        call.Refuse("invalid", fmt::format("{} is not one of the panel's streams", Ambrose::ForLog(stream, 64)));
        return;
    }
    if (!known->Served || !_streams.Serves(stream))
    {
        call.Refuse("unsupported", fmt::format("This panel does not serve the {} stream yet", stream));
        return;
    }
    std::string const app = frame.App.value_or(std::string());
    if (!app.empty() && !KnowsApp(app))
    {
        call.Refuse("not_found", "The panel has no app named " + Ambrose::ForLog(app, 64));
        return;
    }
    std::string const permission(known->Permission);
    switch (call.Caller().Weigh(permission, app))
    {
        case PermissionVerdict::OutOfScope:
            call.Refuse("not_found", "The panel has no app named " + Ambrose::ForLog(app, 64));
            return;
        case PermissionVerdict::Forbidden:
            call.Refuse("forbidden", "This account is not allowed to " + permission);
            return;
        case PermissionVerdict::Allowed:
            break;
    }
    PanelEventRequest request;
    request.Stream = stream;
    if (!app.empty())
        request.App = app;
    if (frame.Seq && *frame.Seq > 0)
        request.After = frame.Seq;
    Follow(call._connection, request, call);
}

void PanelEventSocket::Follow(std::shared_ptr<Connection> const& connection, PanelEventRequest const& request, Call const& call)
{
    std::string const app = request.App.value_or(std::string());
    std::string const key = request.Stream + '\n' + app;
    std::shared_ptr<PanelEventSession> previous;
    {
        std::lock_guard const lock(connection->Mutex);
        if (connection->Closing)
            return;
        auto const found = connection->Sessions.find(key);
        if (found != connection->Sessions.end())
        {
            previous = std::move(found->second.Session);
            connection->Sessions.erase(found);
        }
    }
    if (previous)
        _streams.Close(request.Stream, previous);
    std::shared_ptr<PanelEventSession> const session = _streams.Open(std::make_shared<PanelStreamSink>(connection->Socket, request.Stream, app), request);
    if (!session)
    {
        call.Refuse("unsupported", fmt::format("This panel does not serve the {} stream yet", request.Stream));
        return;
    }
    bool closing = false;
    {
        std::lock_guard const lock(connection->Mutex);
        closing = connection->Closing;
        if (!closing)
            connection->Sessions[key] = Connection::Following{ request.Stream, session };
    }
    if (closing)
        _streams.Close(request.Stream, session);
}
