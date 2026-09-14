/*
 * Project Ambrose by Imjustchico
 * The lifecycle every server app shares: options, config, logging, banner, shutdown signals, an optional update tick, and a clean exit code.
 */

#ifndef AMBROSE_SERVERAPP_H
#define AMBROSE_SERVERAPP_H

#include "IoContext.h"
#include "LogCommon.h"

#include <asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ConfigMgr;
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

    int Run(std::vector<std::string> const& arguments);
    void RequestStop();

    bool IsReady() const noexcept { return _ready.load(); }
    ServerAppInfo const& GetInfo() const noexcept { return _info; }

protected:
    virtual bool OnStart();
    virtual void OnUpdate(std::chrono::milliseconds diff);
    virtual std::chrono::milliseconds GetUpdateInterval() const;
    virtual void OnStop();

    ConfigMgr& Config() noexcept { return _config; }
    Log& Logger() noexcept { return _log; }
    Ambrose::Asio::IoContext& GetIoContext() noexcept { return _io; }

private:
    void ScheduleUpdate();
    void StopNow(std::string const& reason);
    void LogLifecycle(LogLevel level, std::string const& text);
    void FinishShutdown();

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
    std::chrono::steady_clock::time_point _lastUpdate;
    std::atomic<bool> _ready{ false };
    std::atomic<bool> _stopRequested{ false };
    bool _stopping = false;
};

#endif
