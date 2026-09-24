/*
 * Project Ambrose by Imjustchico
 * The lifecycle every server app shares: options, config, logging, banner, the one start time and lifecycle state its console and its admin API both report, the optional admin API listener an app fills with its own routes before it opens, with the live log stream, the read half of the settings API and a shutdown with a countdown on it, shutdown signals that a start in progress can poll for, an optional update tick, console commands on their own thread with their replies on the log's own writer, and a clean exit code.
 */

#ifndef AMBROSE_SERVERAPP_H
#define AMBROSE_SERVERAPP_H

#include "AdminStatus.h"
#include "ConfigMgr.h"
#include "ConsoleCommandTable.h"
#include "Duration.h"
#include "IoContext.h"
#include "LogCommon.h"
#include "Types.h"

#include <asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

class AdminRouter;
class AdminServer;
class ConsoleInput;
class ConsoleReader;
class Log;
class LogStreamService;

namespace Ambrose::Asio
{
    class SignalHandler;
}

enum class AppLifecycle
{
    Stopped,
    Starting,
    Running,
    Stopping
};

struct ServerAppInfo
{
    std::string Name;
    std::string DefaultConfigFile;
    uint16 AdminPort = 0;
};

class ServerApp
{
public:
    ServerApp(ServerAppInfo info, ConfigMgr& config, Log& log, std::ostream& out, std::ostream& err);
    virtual ~ServerApp();
    ServerApp(ServerApp const&) = delete;
    ServerApp& operator=(ServerApp const&) = delete;

    static constexpr std::size_t MaxQueuedCommands = 256;
    static constexpr int64 MaxShutdownDelaySeconds = 86400;

    int Run(std::vector<std::string> const& arguments);
    void RequestStop(std::string reason = "a stop request");
    Seconds GetUptime() const noexcept;

    bool IsReady() const noexcept { return GetLifecycleState() == AppLifecycle::Running; }
    bool IsCheckOnly() const noexcept { return _checkOnly.load(); }
    ConsoleCommandTable& Commands() noexcept { return _commands; }
    ServerAppInfo const& GetInfo() const noexcept { return _info; }
    AppLifecycle GetLifecycleState() const noexcept { return _lifecycle.load(); }
    AdminServer* GetAdminApi() const noexcept { return _admin.get(); }
    bool ReloadAdminApi();
    void RegisterStandardRoutes(AdminRouter& routes);
    std::filesystem::path CommandAuditFile() const;

    void SetListener(std::string address, uint16 port);
    void SetClientSetup(bool installFound, bool typeDumpInUse, bool typeDumpStale, std::string typeDumpError);
    std::optional<AdminTickWindow> GetTickWindow() const;
    std::vector<AdminProblem> CollectProblems() const;
    AdminStatusSnapshot BuildStatus() const;

    static std::string_view LifecycleName(AppLifecycle state) noexcept;

protected:
    virtual void OnProblems(std::vector<AdminProblem>& problems);
    virtual bool OnStart();
    virtual void OnUpdate(std::chrono::milliseconds diff);
    virtual std::chrono::milliseconds GetUpdateInterval() const;
    virtual void OnStop();
    virtual void OnStatus(std::vector<std::pair<std::string, std::string>>& fields);
    virtual std::unique_ptr<ConsoleInput> CreateConsoleInput();
    virtual std::string GetRealmName() const;
    virtual void OnAdminApiReady(AdminServer& admin);
    virtual std::vector<RestartRequiredOption> GetRestartRequiredOptions() const;

    ConfigMgr& Config() noexcept { return _config; }
    Log& Logger() noexcept { return _log; }
    Ambrose::Asio::IoContext& GetIoContext() noexcept { return _io; }
    bool PollStopRequested();

private:
    bool IsStopping() const noexcept { return GetLifecycleState() == AppLifecycle::Stopping; }
    bool StartAdminApi();
    void ScheduleUpdate();
    void ScheduleStop(Seconds delay, std::string reason);
    bool CancelScheduledStop();
    void StopNow(std::string const& reason);
    void LogLifecycle(LogLevel level, std::string const& text);
    void FinishShutdown();
    void StartConsole();
    void StopConsole();
    void QueueConsoleLine(std::string line);
    void RunConsoleCommands();
    void RunConsoleLine(std::string const& line);

    ServerAppInfo _info;
    std::string _category;
    ConfigMgr& _config;
    Log& _log;
    std::ostream& _out;
    std::ostream& _err;
    Ambrose::Asio::IoContext _io;
    std::optional<Ambrose::Asio::IoContext::WorkGuard> _work;
    asio::steady_timer _updateTimer;
    asio::steady_timer _shutdownTimer;
    std::unique_ptr<Ambrose::Asio::SignalHandler> _signals;
    ConsoleCommandTable _commands;
    std::unique_ptr<AdminServer> _admin;
    std::unique_ptr<LogStreamService> _logStream;
    std::unique_ptr<ConsoleReader> _console;
    std::thread _commandThread;
    std::mutex _commandMutex;
    std::condition_variable _commandWake;
    std::deque<std::string> _commandQueue;
    bool _commandStop = false;
    std::atomic<bool> _checkOnly{ false };
    std::chrono::steady_clock::time_point _lastUpdate;
    std::atomic<std::chrono::steady_clock::time_point> _startedAt{ std::chrono::steady_clock::time_point() };
    std::atomic<AppLifecycle> _lifecycle{ AppLifecycle::Stopped };
    std::atomic<bool> _stopScheduled{ false };
    std::atomic<bool> _stopRequested{ false };
    std::atomic<bool> _starting{ false };
    std::mutex _pollMutex;
    mutable std::mutex _statusMutex;
    std::string _listenerAddress;
    uint16 _listenerPort = 0;
    bool _installFound = true;
    bool _typeDumpInUse = true;
    bool _typeDumpStale = false;
    std::string _typeDumpError;
    std::deque<std::pair<std::chrono::steady_clock::time_point, double>> _ticks;
};

#endif
