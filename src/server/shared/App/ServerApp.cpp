/*
 * Project Ambrose by Imjustchico
 * Runs an app from arguments to exit: rejects bad options and missing config with exit code 1, stops gracefully on signals, requests or the shutdown command, now or after a delay, lets a start in progress run queued signal handlers without blocking so a stop during OnStart exits cleanly without reporting ready, ticks updates on its io loop, and runs queued console lines on a command thread that shutdown waits for, answering on the same writer the log lines use.
 */

#include "ServerApp.h"
#include "AppOptions.h"
#include "Banner.h"
#include "ConfigMgr.h"
#include "ConsoleInput.h"
#include "ConsoleReader.h"
#include "ConsoleWriter.h"
#include "GitRevision.h"
#include "Log.h"
#include "SignalHandler.h"
#include "StringUtil.h"
#include "TerminalConsoleInput.h"

#include <asio/post.hpp>

#include <fmt/format.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <ostream>

ServerApp::ServerApp(ServerAppInfo info, ConfigMgr& config, Log& log, std::ostream& out, std::ostream& err)
    : _info(std::move(info)), _category("server." + _info.Name), _config(config), _log(log), _out(out), _err(err), _updateTimer(_io.GetImpl()), _shutdownTimer(_io.GetImpl())
{
    _commands.Register({ "help", "[command]", "list commands, or the commands starting with the given words", false,
        [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            std::string prefix;
            for (std::string const& argument : arguments)
                prefix += (prefix.empty() ? "" : " ") + argument;
            std::vector<std::string> const lines = _commands.DescribeCommands(prefix);
            if (lines.empty())
                reply(fmt::format("No command starts with '{}'", prefix));
            for (std::string const& line : lines)
                reply(line);
            return true;
        } });
    _commands.Register({ "status", "", "show the revision, how long this server has run and what it is doing", false,
        [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (!arguments.empty())
                return false;
            std::vector<std::pair<std::string, std::string>> fields;
            fields.emplace_back("server", _info.Name);
            fields.emplace_back("revision", GitRevision::GetFullVersion());
            fields.emplace_back("uptime", Ambrose::FormatDuration(GetUptime()));
            fields.emplace_back("state", _stopping.load() ? "shutting down" : (_ready.load() ? "ready" : "starting"));
            OnStatus(fields);
            for (auto const& [name, value] : fields)
                reply(fmt::format("{:<10}{}", name + ':', value));
            return true;
        } });
    _commands.Register({ "shutdown", "[seconds|cancel]", "stop the server gracefully, now or after a delay", false,
        [this](std::vector<std::string> const& arguments, ConsoleCommandTable::Reply const& reply)
        {
            if (arguments.size() > 1)
                return false;
            if (arguments.empty())
            {
                reply(fmt::format("{} is shutting down", _info.Name));
                RequestStop("the shutdown command");
                return true;
            }
            if (Ambrose::EqualsIgnoreCase(arguments.front(), "cancel"))
            {
                reply(CancelScheduledStop() ? "The pending shutdown is cancelled" : "No shutdown is pending");
                return true;
            }
            std::optional<Seconds> const delay = Ambrose::ParseDuration(arguments.front());
            if (!delay)
                return false;
            ScheduleStop(*delay);
            reply(fmt::format("{} stops in {}", _info.Name, Ambrose::FormatDuration(*delay)));
            return true;
        } });
}

ServerApp::~ServerApp()
{
    StopConsole();
}

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

void ServerApp::OnStatus(std::vector<std::pair<std::string, std::string>>&)
{
}

std::unique_ptr<ConsoleInput> ServerApp::CreateConsoleInput()
{
    ConsoleWriter& console = _log.GetConsole();
    if (TerminalConsoleInput::IsAvailable(console))
        return std::make_unique<TerminalConsoleInput>(console, "Ambrose> ", [this](std::string_view prefix) { return _commands.CompleteNames(prefix); });
    return std::make_unique<StandardConsoleInput>();
}

Seconds ServerApp::GetUptime() const
{
    if (_startedAt.time_since_epoch().count() == 0)
        return Seconds(0);
    return std::chrono::duration_cast<Seconds>(std::chrono::steady_clock::now() - _startedAt);
}

int ServerApp::Run(std::vector<std::string> const& arguments)
{
    _stopping = false;
    _stopScheduled = false;
    _startedAt = std::chrono::steady_clock::now();
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

    _io.Restart();
    _io.Poll();
    _io.Restart();
    _work.emplace(_io.MakeWorkGuard());
    _signals = std::make_unique<Ambrose::Asio::SignalHandler>(_io, Ambrose::Asio::SignalHandler::ShutdownSignals(), [this](int signal)
    {
        StopNow(fmt::format("signal {}", signal));
    });

    _starting = true;
    bool const started = OnStart();
    _starting = false;
    if (!started)
    {
        bool const stopped = _stopping.load() || _stopRequested.load();
        if (stopped)
            LogLifecycle(LogLevel::Info, fmt::format("{} stopped before it finished starting", _info.Name));
        else
            LogLifecycle(LogLevel::Error, fmt::format("{} failed to start", _info.Name));
        _work.reset();
        FinishShutdown();
        return stopped ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    bool const stoppedWhileStarting = _stopping.load();

    for (std::string const& name : _log.GetPendingAppenderNames())
        AMBROSE_LOG(_log, LogLevel::Warn, "server.logging", "appender {} has a type this app does not provide and stays inactive", name);

    _lastUpdate = std::chrono::steady_clock::now();
    if (stoppedWhileStarting)
        _io.Restart();
    else
    {
        if (GetUpdateInterval().count() > 0)
            ScheduleUpdate();
        _ready = true;
        LogLifecycle(LogLevel::Info, fmt::format("{} ready", _info.Name));
        if (_stopRequested.load())
            StopNow("a stop request");
        else if (options.CheckOnly)
            StopNow("--check");
        else
            StartConsole();
    }
    _io.Run();

    StopConsole();
    _ready = false;
    OnStop();
    LogLifecycle(LogLevel::Info, fmt::format("{} stopped", _info.Name));
    FinishShutdown();
    return EXIT_SUCCESS;
}

bool ServerApp::PollStopRequested()
{
    if (_starting.load())
    {
        std::unique_lock const lock(_pollMutex, std::try_to_lock);
        if (lock.owns_lock())
            _io.Poll();
    }
    return _stopping.load() || _stopRequested.load();
}

void ServerApp::RequestStop(std::string reason)
{
    _stopRequested = true;
    asio::post(_io.GetExecutor(), [this, reason = std::move(reason)] { StopNow(reason); });
}

void ServerApp::LogLifecycle(LogLevel level, std::string const& text)
{
    if (_log.ShouldLog(_category, level))
        _log.Write(_category, level, "{}", text);
}

void ServerApp::StartConsole()
{
    if (!_config.GetOption<bool>("Console.Enable", true, true))
        return;
    std::unique_ptr<ConsoleInput> input = CreateConsoleInput();
    if (!input)
        return;
    {
        std::lock_guard const lock(_commandMutex);
        _commandStop = false;
        _commandQueue.clear();
    }
    _commandThread = std::thread([this] { RunConsoleCommands(); });
    _console = std::make_unique<ConsoleReader>(std::move(input),
        [this](std::string line) { QueueConsoleLine(std::move(line)); },
        [this] { AMBROSE_LOG(_log, LogLevel::Info, "commands.console", "Console input closed; the server keeps running"); });
    _console->Start();
}

void ServerApp::StopConsole()
{
    if (_console)
        _console->Stop();
    _console.reset();
    {
        std::lock_guard const lock(_commandMutex);
        _commandStop = true;
    }
    _commandWake.notify_all();
    if (_commandThread.joinable())
        _commandThread.join();
}

void ServerApp::QueueConsoleLine(std::string line)
{
    std::lock_guard const lock(_commandMutex);
    if (_commandStop)
        return;
    if (_commandQueue.size() >= MaxQueuedCommands)
    {
        AMBROSE_LOG(_log, LogLevel::Warn, "commands.console", "Dropped a console line because {} are already waiting", _commandQueue.size());
        return;
    }
    _commandQueue.push_back(std::move(line));
    _commandWake.notify_one();
}

void ServerApp::RunConsoleCommands()
{
    for (;;)
    {
        std::string line;
        {
            std::unique_lock lock(_commandMutex);
            _commandWake.wait(lock, [this] { return _commandStop || !_commandQueue.empty(); });
            if (_commandQueue.empty())
                return;
            line = std::move(_commandQueue.front());
            _commandQueue.pop_front();
        }
        RunConsoleLine(line);
    }
}

void ServerApp::RunConsoleLine(std::string const& line)
{
    std::string const described = _commands.DescribeForLog(line);
    if (described.empty())
        return;
    auto const reply = [this](std::string_view text)
    {
        std::string answer(text);
        answer.push_back('\n');
        _log.GetConsole().WriteLines(answer, ConsoleColor::Default);
    };
    if (_stopping)
    {
        AMBROSE_LOG(_log, LogLevel::Warn, "commands.console", "Console: {} did not run because {} is shutting down", described, _info.Name);
        reply(fmt::format("{} is shutting down, so '{}' did not run", _info.Name, described));
        return;
    }
    AMBROSE_LOG(_log, LogLevel::Info, "commands.console", "Console: {}", described);
    try
    {
        _commands.Execute(line, reply);
    }
    catch (std::exception const& failure)
    {
        AMBROSE_LOG(_log, LogLevel::Error, "commands.console", "Console: {} failed with {}", described, failure.what());
        reply(fmt::format("'{}' failed: {}", described, failure.what()));
    }
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

void ServerApp::ScheduleStop(Seconds delay)
{
    _stopScheduled = true;
    asio::post(_io.GetExecutor(), [this, delay]
    {
        if (!_stopScheduled.load())
            return;
        LogLifecycle(LogLevel::Info, fmt::format("{} stops in {}", _info.Name, Ambrose::FormatDuration(delay)));
        _shutdownTimer.expires_after(delay);
        _shutdownTimer.async_wait([this](std::error_code const& error)
        {
            if (error || !_stopScheduled.exchange(false))
                return;
            StopNow("the shutdown command");
        });
    });
}

bool ServerApp::CancelScheduledStop()
{
    if (!_stopScheduled.exchange(false))
        return false;
    asio::post(_io.GetExecutor(), [this] { _shutdownTimer.cancel(); });
    return true;
}

void ServerApp::StopNow(std::string const& reason)
{
    if (_stopping || !_work)
        return;
    _stopping = true;
    _stopScheduled = false;
    LogLifecycle(LogLevel::Info, fmt::format("{} shutting down after {}", _info.Name, reason));
    _shutdownTimer.cancel();
    _updateTimer.cancel();
    if (_signals)
        _signals->Cancel();
    _work.reset();
}
