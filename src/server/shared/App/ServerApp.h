/*
 * Project Ambrose by Imjustchico
 * The lifecycle every server app shares: options, config, logging, banner, shutdown signals that a start in progress can poll for, an optional update tick, console commands on their own thread, and a clean exit code.
 */

#ifndef AMBROSE_SERVERAPP_H
#define AMBROSE_SERVERAPP_H

#include "ConsoleCommandTable.h"
#include "IoContext.h"
#include "LogCommon.h"

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
#include <thread>
#include <vector>

class ConfigMgr;
class ConsoleInput;
class ConsoleReader;
class Log;

namespace Ambrose::Asio
{
    class SignalHandler;
}

struct ServerAppInfo
{
    std::string Name;
    std::string DefaultConfigFile;
};

class ServerApp
{
public:
    ServerApp(ServerAppInfo info, ConfigMgr& config, Log& log, std::ostream& out, std::ostream& err);
    virtual ~ServerApp();
    ServerApp(ServerApp const&) = delete;
    ServerApp& operator=(ServerApp const&) = delete;

    static constexpr std::size_t MaxQueuedCommands = 256;

    int Run(std::vector<std::string> const& arguments);
    void RequestStop(std::string reason = "a stop request");

    bool IsReady() const noexcept { return _ready.load(); }
    ConsoleCommandTable& Commands() noexcept { return _commands; }
    ServerAppInfo const& GetInfo() const noexcept { return _info; }

protected:
    virtual bool OnStart();
    virtual void OnUpdate(std::chrono::milliseconds diff);
    virtual std::chrono::milliseconds GetUpdateInterval() const;
    virtual void OnStop();
    virtual std::unique_ptr<ConsoleInput> CreateConsoleInput();

    ConfigMgr& Config() noexcept { return _config; }
    Log& Logger() noexcept { return _log; }
    Ambrose::Asio::IoContext& GetIoContext() noexcept { return _io; }
    bool PollStopRequested();

private:
    void ScheduleUpdate();
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
    std::unique_ptr<Ambrose::Asio::SignalHandler> _signals;
    ConsoleCommandTable _commands;
    std::unique_ptr<ConsoleReader> _console;
    std::thread _commandThread;
    std::mutex _commandMutex;
    std::condition_variable _commandWake;
    std::deque<std::string> _commandQueue;
    bool _commandStop = false;
    std::chrono::steady_clock::time_point _lastUpdate;
    std::atomic<bool> _ready{ false };
    std::atomic<bool> _stopRequested{ false };
    std::atomic<bool> _stopping{ false };
    std::atomic<bool> _starting{ false };
    std::mutex _pollMutex;
};

#endif
