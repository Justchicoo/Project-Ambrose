/*
 * Project Ambrose by Imjustchico
 * Runs an app from arguments to exit: rejects bad options and missing config with exit code 1, stops gracefully on signals or requests, and ticks updates on its io loop.
 */

#include "ServerApp.h"
#include "AppOptions.h"
#include "Banner.h"
#include "ConfigMgr.h"
#include "GitRevision.h"
#include "Log.h"
#include "SignalHandler.h"

#include <asio/post.hpp>

#include <fmt/format.h>

#include <cstdlib>
#include <filesystem>
#include <ostream>

ServerApp::ServerApp(ServerAppInfo info, ConfigMgr& config, Log& log, std::ostream& out, std::ostream& err)
    : _info(std::move(info)), _category("server." + _info.Name), _config(config), _log(log), _out(out), _err(err), _updateTimer(_io.GetImpl())
{
}

ServerApp::~ServerApp() = default;

bool ServerApp::OnStart()
{
    return true;
}

void ServerApp::OnUpdate(std::chrono::milliseconds)
{
}

std::chrono::milliseconds ServerApp::GetUpdateInterval() const
{
    return std::chrono::milliseconds(0);
}

void ServerApp::OnStop()
{
}

int ServerApp::Run(std::vector<std::string> const& arguments)
{
    _stopping = false;
    AppOptions const options = AppOptions::Parse(arguments, _info.DefaultConfigFile);
    if (!options.Error.empty())
    {
        _err << _info.Name << ": " << options.Error << "\n" << AppOptions::Usage(_info.Name, _info.DefaultConfigFile);
        return EXIT_FAILURE;
    }
    if (options.ShowHelp)
    {
        _out << AppOptions::Usage(_info.Name, _info.DefaultConfigFile);
        return EXIT_SUCCESS;
    }
    if (options.ShowVersion)
    {
        _out << GitRevision::GetFullVersion() << "\n";
        return EXIT_SUCCESS;
    }

    std::error_code pathError;
    std::filesystem::path const configPath = std::filesystem::absolute(std::filesystem::path(options.ConfigFile), pathError);
    std::filesystem::path const configFile = pathError ? std::filesystem::path(options.ConfigFile) : configPath;
    ConfigLoadResult const config = _config.LoadInitial(configFile, arguments, options.Overrides);
    if (!config.Succeeded())
    {
        _err << _info.Name << ": cannot load the configuration from " << ConfigMgr::PathToUtf8(configFile) << "\n";
        for (ConfigIssue const& issue : config.Errors)
            _err << "  " << issue.ToString() << "\n";
        std::error_code existsError;
        if (!std::filesystem::exists(configFile, existsError))
            _err << "Copy " << _info.DefaultConfigFile << ".dist to that path and edit it, or pass --config <file>.\n";
        _stopRequested = false;
        return EXIT_FAILURE;
    }

    LogConfigResult const logResult = _log.LoadFromConfig(_config);
    Ambrose::Banner::Show(_info.Name, [this](std::string_view line) { LogLifecycle(LogLevel::Info, std::string(line)); });
    for (ConfigIssue const& issue : logResult.Warnings)
        AMBROSE_LOG(_log, LogLevel::Warn, "server.logging", "{}", issue.ToString());
    for (ConfigIssue const& issue : logResult.Errors)
    {
        AMBROSE_LOG(_log, LogLevel::Error, "server.logging", "{}", issue.ToString());
        _err << "  " << issue.ToString() << "\n";
    }
    if (!logResult.Succeeded())
    {
        _err << _info.Name << ": the logging configuration has errors\n";
        FinishShutdown();
        return EXIT_FAILURE;
    }
    _log.AttachConfigWarnings(_config);
    for (std::string const& name : _log.GetPendingAppenderNames())
        AMBROSE_LOG(_log, LogLevel::Warn, "server.logging", "appender {} has a type this app does not provide and stays inactive", name);

    _io.Restart();
    _io.Poll();
    _io.Restart();
    _work.emplace(_io.MakeWorkGuard());
    _signals = std::make_unique<Ambrose::Asio::SignalHandler>(_io, Ambrose::Asio::SignalHandler::ShutdownSignals(), [this](int signal)
    {
        StopNow(fmt::format("signal {}", signal));
    });

    if (!OnStart())
    {
        LogLifecycle(LogLevel::Error, fmt::format("{} failed to start", _info.Name));
        _work.reset();
        FinishShutdown();
        return EXIT_FAILURE;
    }

    _lastUpdate = std::chrono::steady_clock::now();
    if (GetUpdateInterval().count() > 0)
        ScheduleUpdate();
    _ready = true;
    LogLifecycle(LogLevel::Info, fmt::format("{} ready", _info.Name));
    if (_stopRequested.load())
        StopNow("a stop request");
    _io.Run();

    _ready = false;
    OnStop();
    LogLifecycle(LogLevel::Info, fmt::format("{} stopped", _info.Name));
    FinishShutdown();
    return EXIT_SUCCESS;
}

void ServerApp::RequestStop()
{
    _stopRequested = true;
    asio::post(_io.GetExecutor(), [this] { StopNow("a stop request"); });
}

void ServerApp::LogLifecycle(LogLevel level, std::string const& text)
{
    if (_log.ShouldLog(_category, level))
        _log.Write(_category, level, "{}", text);
}

void ServerApp::FinishShutdown()
{
    _log.DetachConfigWarnings();
    _log.Shutdown();
    _signals.reset();
    _stopRequested = false;
}

void ServerApp::ScheduleUpdate()
{
    std::chrono::milliseconds const interval = GetUpdateInterval();
    _updateTimer.expires_after(interval.count() > 0 ? interval : std::chrono::milliseconds(50));
    _updateTimer.async_wait([this](std::error_code const& error)
    {
        if (error || _stopping)
            return;
        auto const now = std::chrono::steady_clock::now();
        auto const diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastUpdate);
        _lastUpdate = now;
        if (GetUpdateInterval().count() > 0)
            OnUpdate(diff);
        ScheduleUpdate();
    });
}

void ServerApp::StopNow(std::string const& reason)
{
    if (_stopping || !_work)
        return;
    _stopping = true;
    LogLifecycle(LogLevel::Info, fmt::format("{} shutting down after {}", _info.Name, reason));
    _updateTimer.cancel();
    if (_signals)
        _signals->Cancel();
    _work.reset();
}
