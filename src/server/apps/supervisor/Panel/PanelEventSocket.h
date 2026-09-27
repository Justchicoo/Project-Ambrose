/*
 * Project Ambrose by Imjustchico
 * The panel's one browser socket, /api/panel/events. Its upgrade carries no query, since a credential in an address ends up in logs and history, and any ticket such an address names is burned; an Origin, when one is sent, must be the panel's own exactly. The first frame must be hello within ten seconds: a browser names its session's CSRF token, checked in constant time against the session its cookie holds and only from the panel's own origin, and a script names a ticket, spent from the address it was minted for. The socket then answers ready with the protocol version, the server's time, this run's instance, the caller's permissions at panel scope and per app, the apps it may see and its realms, and from then on hands each frame to the handler registered for its type, ping and resume being built in and later milestones adding theirs. A resume opens the caller's session on one stream at one scope after the sequence it last saw, checked against the stream's read permission the way a route is, so an app the caller holds nothing on is not found and one it holds something on but not this is forbidden. Every error carries a correlation id that one supervisor log line repeats with the full text, which the caller sees only for its own mistakes or when it holds debug.errors. A stream that fell behind sends a dropped frame with the missed range, and one that never drops closes the socket with 4429 so the page resumes rather than silently losing a record.
 */

#ifndef AMBROSE_PANELEVENTSOCKET_H
#define AMBROSE_PANELEVENTSOCKET_H

#include "AdminRouter.h"
#include "AdminServer.h"
#include "PanelAuthorization.h"
#include "PanelEventFrame.h"
#include "PanelEventStreams.h"
#include "PanelEventTickets.h"
#include "Types.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

class Log;
class PanelGrants;
class PanelUsers;

struct PanelEventCaller
{
    std::string Principal = {};
    std::string Name = {};
    std::optional<PanelAsking> Asking = {};
    std::vector<std::string> Panel = {};
    std::map<std::string, std::vector<std::string>, std::less<>> Apps = {};
    std::optional<std::vector<std::string>> Scopes = {};

    bool Holds(std::string_view permission) const;
    bool HoldsAnythingOn(std::string_view app) const;
    PermissionVerdict Weigh(std::string_view permission, std::string_view app) const;
};

class PanelEventSocket
{
public:
    using Clock = std::chrono::steady_clock;
    using TimeSource = std::function<Clock::time_point()>;
    using AppSource = std::function<std::vector<std::string>()>;

    static constexpr std::string_view Path = "/api/panel/events";
    static constexpr std::string_view TicketPath = "/api/panel/events/ticket";
    static constexpr uint32 TicketCost = 5;
    static constexpr std::chrono::seconds HelloDeadline{ 10 };
    static constexpr std::chrono::seconds SweepInterval{ 1 };
    static constexpr uint16 InternalError = 1011;
    static constexpr std::size_t MaxCloseReason = 120;

    struct Connection;

    class Call
    {
    public:
        Call(PanelEventSocket& socket, std::shared_ptr<Connection> connection, PanelIncomingFrame const& frame);

        PanelIncomingFrame const& Frame() const noexcept { return _frame; }
        PanelEventCaller Caller() const;
        void Answer(std::string_view type, std::string const& data) const;
        void Refuse(std::string_view code, std::string const& message) const;
        void Fail(std::string const& text) const;

    private:
        friend class PanelEventSocket;

        PanelEventSocket& _socket;
        std::shared_ptr<Connection> _connection;
        PanelIncomingFrame const& _frame;
    };

    using Handler = std::function<void(Call const&)>;

    PanelEventSocket(Log& log, PanelEventStreams& streams, PanelEventTickets& tickets, SessionSource& sessions, PanelUsers& users, PanelGrants& grants, AdminRouter& routes);
    ~PanelEventSocket();

    PanelEventSocket(PanelEventSocket const&) = delete;
    PanelEventSocket& operator=(PanelEventSocket const&) = delete;

    AdminSocketRoute MakeRoute();
    std::optional<AdminResponse> Admit(AdminRequest const& request);
    AdminResponse MintTicket(AdminRequest const& request);

    void Handle(std::string type, Handler handler);
    std::vector<std::string> HandledTypes() const;
    void SetAppSource(AppSource source);
    void SetTimeSource(TimeSource source);

    void Start();
    void Stop();
    std::size_t Sweep();
    std::size_t GetConnectionCount() const;

    static std::string ReadyData(PanelEventCaller const& caller, std::string_view instance, std::vector<std::string> const& apps, int64 nowMs);
    static std::string ErrorData(std::optional<std::string> const& request, std::string_view code, std::string_view message, std::string_view correlation);
    static std::string CloseReason(std::string_view text);

private:
    void Opened(AdminSocket& socket);
    void Received(AdminSocket& socket, std::string const& message, bool binary);
    void Closed(AdminSocket& socket);
    void Hello(std::shared_ptr<Connection> const& connection, PanelIncomingFrame const& frame);
    void Dispatch(std::shared_ptr<Connection> const& connection, PanelIncomingFrame const& frame);
    void Resume(Call const& call);
    void Follow(std::shared_ptr<Connection> const& connection, PanelEventRequest const& request, Call const& call);
    void Shut(Connection& connection, uint16 code, std::string_view reason);
    void Send(Connection& connection, std::string_view type, std::optional<std::string> const& id, std::string const& data);
    void Report(Connection& connection, std::optional<std::string> const& id, std::string_view code, std::string const& text, bool internal);
    std::optional<PanelEventCaller> CallerOf(std::string const& principal, std::string& failure);
    std::vector<std::string> Apps() const;
    std::vector<std::string> VisibleApps(PanelEventCaller const& caller) const;
    bool KnowsApp(std::string_view app) const;
    std::shared_ptr<Connection> Find(AdminSocket& socket) const;
    Clock::time_point Now() const;

    Log& _log;
    PanelEventStreams& _streams;
    PanelEventTickets& _tickets;
    SessionSource& _sessions;
    PanelUsers& _users;
    PanelGrants& _grants;
    AdminRouter& _routes;

    mutable std::mutex _mutex;
    std::map<AdminSocket*, std::shared_ptr<Connection>> _connections;
    std::map<std::string, Handler, std::less<>> _handlers;
    AppSource _appSource;

    mutable std::mutex _clockMutex;
    TimeSource _timeSource;

    std::mutex _sweepMutex;
    std::condition_variable _sweepWake;
    std::thread _sweeper;
    bool _sweeping = false;
};

#endif
