/*
 * Project Ambrose by Imjustchico
 * The supervisor's apps and its admin routes: it builds each app from the definitions and the saved state, watches them until it stops, which leaves them running for the next supervisor to take back, answers GET /api/apps with itself and every app in the one list shape the panel reads, GET /api/supervisor with each app's state, exits and stop in progress, POST /api/apps/{name}/power to start, stop, restart or kill one with a countdown, GET /api/apps/{name}/output/current and /previous with its captured output, and relays any other /api/apps/{name}/api/... request to that app's own admin API with the app's token, the request's id and query, and for a settings route the caller's name and the rights they hold, so the app records who changed what and never grants more than the panel would, while a settings batch is charged its cost here and every relayed settings or reload answer is handed to the panel's record; before a danger permission is used, a secret is revealed or a restricted setting is changed, the listener the request came in on may ask for a fresh check of who the caller is, and that answer is given instead of relaying; the browser never talks to an app directly. Every state an app passes through is handed to one status observer, with the data the panel's status event carries: the app, its state and since when, its process, its last exit code once it is down, its crash count and when it starts again.
 */

#ifndef AMBROSE_SUPERVISOR_H
#define AMBROSE_SUPERVISOR_H

#include "AdminStatus.h"
#include "ChildProcess.h"
#include "ManagedApp.h"
#include "OutputLog.h"
#include "SupervisorState.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

class AdminRouter;
class ConfigMgr;
class Log;
struct AdminRequest;
struct AdminResponse;
using SupervisorAuditRecorder = std::function<AdminResponse(AdminRequest const&, std::string_view, std::string_view, std::function<AdminResponse()>)>;
using SupervisorCommandLevel = std::function<uint8(AdminRequest const&)>;
using SupervisorActorName = std::function<std::string(AdminRequest const&)>;

struct SupervisorSettings
{
    static constexpr uint64 MaxOutputBytesLimit = 1024ull * 1024 * 1024;

    std::filesystem::path StateFile;
    std::filesystem::path HistoryFile;
    std::chrono::seconds SampleInterval{ 5 };
    std::chrono::seconds SaveInterval{ 300 };
    std::filesystem::path OutputFolder;
    uint64 MaxOutputBytes = OutputLog::DefaultMaxFileBytes;
    std::filesystem::path DataFolder;
    std::filesystem::path ProgramFolder;
    std::filesystem::path WorkingFolder;

    static SupervisorSettings Load(ConfigMgr const& config, std::filesystem::path dataFolder, std::filesystem::path programFolder, std::filesystem::path workingFolder, std::vector<std::string>& problems);
};

struct RelayedAnswer
{
    std::string App;
    std::string Method;
    std::string Path;
    int Status = 0;
    std::string Body;
};

struct SupervisorRelayHooks
{
    std::function<std::string(AdminRequest const&)> NameOf;
    std::function<void(AdminRequest const&, RelayedAnswer const&)> Relayed;
};

class Supervisor
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr std::chrono::seconds RelayTimeout{ 120 };

    Supervisor(Log& log, ChildBreakSender sendBreak);
    ~Supervisor();

    Supervisor(Supervisor const&) = delete;
    Supervisor& operator=(Supervisor const&) = delete;

    bool Start(ConfigMgr const& config, SupervisorSettings const& settings, bool watch, std::vector<std::string>& problems, std::string& error);
    void Shutdown();
    void Register(AdminRouter& router, std::function<AdminStatusSnapshot()> self);
    void SetAuditRecorder(SupervisorAuditRecorder recorder);
    void SetCommandContext(SupervisorCommandLevel level, SupervisorActorName actor);
    void SetRelayHooks(SupervisorRelayHooks hooks);
    void SetStatusObserver(AppStatusObserver observer);
    static std::string StatusData(AppSnapshot const& snapshot);
    static std::vector<std::pair<std::string, std::string>> ForwardedHeaders(AdminRequest const& request, AdminRouter const& router, std::string_view method, std::string_view tail,
        std::string_view permission, std::string const& name);
    static std::string QueryString(AdminRequest const& request);
    std::vector<std::pair<std::string, std::string>> CollectErrorReports();
    static std::optional<std::string_view> PermissionFor(std::string_view method, std::string_view tail) noexcept;
    static std::optional<AdminResponse> Refuse(AdminRequest const& request, std::string_view permission, AdminRouter const& router);
    static std::optional<AdminResponse> StepUpFor(AdminRequest const& request, AdminRouter const& router, std::string_view method, std::string_view tail);

    PowerResult Power(std::string_view name, PowerAction action, uint32 countdownSeconds);
    std::vector<AppSnapshot> Snapshots() const;
    std::optional<std::string> AskApp(std::string_view name, std::string_view path, std::chrono::milliseconds timeout) const;
    std::optional<AdminClientResponse> CallApp(std::string_view name, std::string_view method, std::string_view path, std::string_view body,
        std::chrono::milliseconds timeout) const;
    std::vector<OutputLine> Output(std::string_view name, OutputRun run, uint64 after) const;

    static std::string SupervisionJson(std::vector<AppSnapshot> const& snapshots);
    static std::string AppsJson(AdminStatusSnapshot const& self, std::vector<AppSnapshot> const& snapshots);
    static std::string OutputJson(std::string_view name, OutputRun run, std::vector<OutputLine> const& lines);
    static std::string PrepareCommandRelayBody(std::string_view body, uint8 maximumLevel);

private:
    AdminResponse Answer(AdminRequest const& request, AdminRouter const& router);
    AdminResponse AnswerCore(AdminRequest const& request, AdminRouter const& router);
    AdminResponse Audited(AdminRequest const& request, std::string_view app, std::string_view action, std::function<AdminResponse()> operation);
    AdminResponse PowerRoute(ManagedApp& app, AdminRequest const& request, AdminRouter const& router);
    AdminResponse Relay(ManagedApp& app, AdminRequest const& request, std::string_view path, std::vector<std::pair<std::string, std::string>> headers = {});
    AdminResponse Forward(ManagedApp& app, AdminRequest const& request, std::string_view path, std::chrono::milliseconds timeout,
        std::vector<std::pair<std::string, std::string>> headers);
    ManagedApp* Find(std::string_view name) const;
    void ForwardStatus(AppSnapshot const& snapshot);

    Log& _log;
    ChildBreakSender _sendBreak;
    std::unique_ptr<SupervisorState> _state;
    mutable std::shared_mutex _mutex;
    std::vector<std::unique_ptr<ManagedApp>> _apps;
    SupervisorAuditRecorder _auditRecorder;
    SupervisorCommandLevel _commandLevel;
    SupervisorActorName _actorName;
    SupervisorRelayHooks _hooks;
    std::mutex _observerMutex;
    AppStatusObserver _statusObserver;
};

#endif
