/*
 * Project Ambrose by Imjustchico
 * The supervisor's apps and its admin routes: it builds each app from the definitions and the saved state, watches them until it stops, which leaves them running for the next supervisor to take back, answers GET /api/apps with itself and every app in the one list shape the panel reads, GET /api/supervisor with each app's state, exits and stop in progress, POST /api/apps/{name}/power to start, stop, restart or kill one with a countdown, GET /api/apps/{name}/output/current and /previous with its captured output, and relays any other /api/apps/{name}/api/... request to that app's own admin API with the app's token and the request's id, so the browser never talks to an app directly.
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
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

class AdminRouter;
class ConfigMgr;
class Log;
struct AdminRequest;
struct AdminResponse;

struct SupervisorSettings
{
    static constexpr uint64 MaxOutputBytesLimit = 1024ull * 1024 * 1024;

    std::filesystem::path StateFile;
    std::filesystem::path OutputFolder;
    uint64 MaxOutputBytes = OutputLog::DefaultMaxFileBytes;
    std::filesystem::path DataFolder;
    std::filesystem::path ProgramFolder;
    std::filesystem::path WorkingFolder;

    static SupervisorSettings Load(ConfigMgr const& config, std::filesystem::path dataFolder, std::filesystem::path programFolder, std::filesystem::path workingFolder, std::vector<std::string>& problems);
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
    std::vector<std::pair<std::string, std::string>> CollectErrorReports();
    static std::string_view PermissionFor(std::string_view tail) noexcept;
    std::optional<AdminResponse> Refuse(AdminRequest const& request, std::string_view permission) const;

    PowerResult Power(std::string_view name, PowerAction action, uint32 countdownSeconds);
    std::vector<AppSnapshot> Snapshots() const;
    std::vector<OutputLine> Output(std::string_view name, OutputRun run, uint64 after) const;

    static std::string SupervisionJson(std::vector<AppSnapshot> const& snapshots);
    static std::string AppsJson(AdminStatusSnapshot const& self, std::vector<AppSnapshot> const& snapshots);
    static std::string OutputJson(std::string_view name, OutputRun run, std::vector<OutputLine> const& lines);

private:
    AdminResponse Answer(AdminRequest const& request);
    AdminResponse PowerRoute(ManagedApp& app, AdminRequest const& request);
    AdminResponse Relay(ManagedApp& app, AdminRequest const& request, std::string_view path);
    ManagedApp* Find(std::string_view name) const;

    Log& _log;
    ChildBreakSender _sendBreak;
    std::unique_ptr<SupervisorState> _state;
    mutable std::shared_mutex _mutex;
    AdminRouter* _routes = nullptr;
    std::vector<std::unique_ptr<ManagedApp>> _apps;
};

#endif
