/*
 * Project Ambrose by Imjustchico
 * Runs an app from arguments to exit: rejects bad options and missing config with exit code 1, opens the admin API with the app's own routes and the live log stream already in it before the app starts, keeping a generated token in the data folder or, where the machine names none, beside the config file, and refuses to run when its binding is unsafe, stops gracefully on signals, requests, the shutdown command or POST /api/shutdown, now or after a delay either can cancel, with the reason logged when the delay runs out, answers GET /api/settings with the options the app declares restart-required, moves the one lifecycle state the console and the admin API both read, tells an app whether it runs only to check its start, lets a start in progress run queued signal handlers without blocking so a stop during OnStart exits cleanly without reporting ready, ticks updates on its io loop, and runs queued console lines on a command thread that shutdown waits for, answering on the same writer the log lines use.
 */

#include "ServerApp.h"
#include "AdminCapabilities.h"
#include "AdminCommand.h"
#include "AdminConfigView.h"
#include "AdminServer.h"
#include "ListenerSettings.h"
#include "AppOptions.h"
#include "Banner.h"
#include "ClientLocator.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "ConsoleInput.h"
#include "ConsoleReader.h"
#include "ConsoleWriter.h"
#include "GitRevision.h"
#include "Log.h"
#include "LogStream.h"
#include "SignalHandler.h"
#include "StringUtil.h"
#include "TerminalConsoleInput.h"

#include <asio/post.hpp>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <ostream>

namespace
{
    constexpr std::size_t LogBacklogPage = 500;
}

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
            fields.emplace_back("state", std::string(LifecycleName(GetLifecycleState())));
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
            ScheduleStop(*delay, "the shutdown command");
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

std::string ServerApp::GetRealmName() const
{
    return {};
}

void ServerApp::OnAdminApiReady(AdminServer&)
{
}

std::vector<RestartRequiredOption> ServerApp::GetRestartRequiredOptions() const
{
    return {};
}

void ServerApp::OnProblems(std::vector<AdminProblem>&)
{
}

void ServerApp::SetListener(std::string address, uint16 port)
{
    std::lock_guard const lock(_statusMutex);
    _listenerAddress = std::move(address);
    _listenerPort = port;
}

void ServerApp::SetClientSetup(bool installFound, bool typeDumpInUse, bool typeDumpStale, std::string typeDumpError)
{
    std::lock_guard const lock(_statusMutex);
    _installFound = installFound;
    _typeDumpInUse = typeDumpInUse;
    _typeDumpStale = typeDumpStale;
    _typeDumpError = std::move(typeDumpError);
}

std::optional<AdminTickWindow> ServerApp::GetTickWindow() const
{
    if (GetUpdateInterval().count() <= 0)
        return std::nullopt;
    AdminTickWindow window;
    std::lock_guard const lock(_statusMutex);
    auto const cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(window.WindowSeconds);
    double total = 0.0;
    for (auto const& [when, milliseconds] : _ticks)
    {
        if (when < cutoff)
            continue;
        total += milliseconds;
        window.MaxMs = std::max(window.MaxMs, milliseconds);
        ++window.Samples;
    }
    window.AverageMs = window.Samples ? total / window.Samples : 0.0;
    return window;
}

std::vector<AdminProblem> ServerApp::CollectProblems() const
{
    std::vector<AdminProblem> problems;
    {
        std::lock_guard const lock(_statusMutex);
        if (!_installFound)
            problems.push_back({ std::string(AdminProblemCodes::InstallMissing), "No client installation was found", "client" });
        else if (!_typeDumpInUse)
            problems.push_back({ std::string(AdminProblemCodes::TypeDumpMissing), _typeDumpError.empty() ? "No type dump is in use" : _typeDumpError, "type dump" });
        else if (_typeDumpStale)
            problems.push_back({ std::string(AdminProblemCodes::TypeDumpStale), _typeDumpError.empty() ? "The type dump does not match the installed client program" : _typeDumpError, "type dump" });
    }
    const_cast<ServerApp*>(this)->OnProblems(problems);
    return problems;
}

AdminStatusSnapshot ServerApp::BuildStatus() const
{
    AdminStatusSnapshot snapshot;
    snapshot.App.Name = _info.Name;
    snapshot.App.Role = _info.Name;
    snapshot.App.Realm = GetRealmName();
    snapshot.App.Revision = GitRevision::GetHash();
    {
        std::lock_guard const lock(_statusMutex);
        snapshot.App.Address = _listenerAddress;
        snapshot.App.Port = _listenerPort;
    }
    snapshot.State = std::string(LifecycleName(GetLifecycleState()));
    snapshot.UptimeSeconds = static_cast<uint64>(GetUptime().count());
    snapshot.Process = Ambrose::ProcessInfo::Snapshot();
    if (std::optional<Ambrose::StatValue> const sessions = sStats.Get("sessions"))
        if (int64 const* const count = std::get_if<int64>(&*sessions))
            snapshot.Sessions = static_cast<uint64>(std::max<int64>(0, *count));
    snapshot.Tick = GetTickWindow();
    snapshot.Stats = sStats.Collect();
    snapshot.Problems = CollectProblems();
    return snapshot;
}

Seconds ServerApp::GetUptime() const noexcept
{
    std::chrono::steady_clock::time_point const started = _startedAt.load();
    if (started == std::chrono::steady_clock::time_point())
        return Seconds(0);
    return std::chrono::duration_cast<Seconds>(std::chrono::steady_clock::now() - started);
}

std::string_view ServerApp::LifecycleName(AppLifecycle state) noexcept
{
    switch (state)
    {
        case AppLifecycle::Starting:
            return "starting";
        case AppLifecycle::Running:
            return "running";
        case AppLifecycle::Stopping:
            return "stopping";
        case AppLifecycle::Stopped:
            break;
    }
    return "stopped";
}

bool ServerApp::StartAdminApi()
{
    std::vector<std::string> problems;
    ListenerSettings const settings = ListenerSettings::Load(_config, "Admin", _info.AdminPort, &problems);
    for (std::string const& problem : problems)
        AMBROSE_LOG(_log, LogLevel::Warn, "server.admin", "{}", problem);

    LocalClientSystem const system;
    _admin = std::make_unique<AdminServer>(_log, _info.Name, ClientLocator::GetDataFolder(system), _config.GetFilename().parent_path());
    _admin->SetHealthSource([this]
    {
        AdminHealth health;
        health.App = _info.Name;
        health.Realm = GetRealmName();
        health.Revision = GitRevision::GetHash();
        health.UptimeSeconds = static_cast<uint64>(GetUptime().count());
        health.State = LifecycleName(GetLifecycleState());
        return health;
    });
    sAdminCapabilities.RegisterStandardProblems();
    sAdminCapabilities.AddReloadTarget("admin");
    RegisterStandardRoutes(_admin->Routes());
    _logStream = std::make_unique<LogStreamService>(_log.GetStreamHub());
    _logStream->Start();
    _admin->AddSocket(_logStream->MakeSocketRoute("/api/logs"));
    _admin->Routes().AddGuardedPrefix("GET", "/api/logs/after/", "console.read", [this](AdminRequest const& request)
    {
        std::string_view tail(request.Path);
        tail.remove_prefix(std::string_view("/api/logs/after/").size());
        std::optional<uint64> const after = Ambrose::StringTo<uint64>(tail);
        if (!after)
            return AdminResponse::Invalid("Reading a log takes the sequence number to read after", { { "after", "Give a whole number, or 0 for everything kept" } });
        return AdminResponse::Json(200, LogStreamService::BacklogJson(_log.GetStreamHub(), *after, LogBacklogPage));
    });

    OnAdminApiReady(*_admin);

    std::string error;
    if (_admin->Start(settings, error))
        return true;
    AMBROSE_LOG(_log, LogLevel::Error, "server.admin", "{}", error);
    _err << _info.Name << ": " << error << "\n";
    _admin.reset();
    _logStream.reset();
    return false;
}

std::filesystem::path ServerApp::CommandAuditFile() const
{
    std::filesystem::path const folder = _config.GetFilename().parent_path();
    return folder / "audit" / (_info.Name + "-commands.jsonl");
}

void ServerApp::RegisterStandardRoutes(AdminRouter& routes)
{
    AdminStatus::Register(routes, [this] { return BuildStatus(); });
    AdminConfigView::Register(routes, _config, GetRestartRequiredOptions());
    AdminCommand::Register(routes, _commands, _info.Name, CommandAuditFile());
    routes.AddGuarded("POST", "/api/shutdown", "power.stop", [this](AdminRequest const& request)
    {
        nlohmann::json const body = request.Body.empty() ? nlohmann::json::object() : nlohmann::json::parse(request.Body, nullptr, false);
        if (!body.is_object())
            return AdminResponse::Invalid("A shutdown takes a JSON object", { { "seconds", "Give the countdown in whole seconds, or cancel" } });
        std::vector<std::pair<std::string, std::string>> fields;
        for (auto const& [key, value] : body.items())
            if (key != "seconds" && key != "cancel")
                fields.emplace_back(key, "A shutdown takes only seconds or cancel");
        bool cancel = false;
        if (body.contains("cancel"))
        {
            if (body["cancel"].is_boolean())
                cancel = body["cancel"].get<bool>();
            else
                fields.emplace_back("cancel", "Give cancel as true or false");
        }
        std::optional<int64> seconds;
        if (body.contains("seconds"))
        {
            nlohmann::json const& value = body["seconds"];
            if (!value.is_number_integer() || value.get<int64>() < 0 || value.get<int64>() > MaxShutdownDelaySeconds)
                fields.emplace_back("seconds", fmt::format("Give the countdown in whole seconds from 0 to {}", MaxShutdownDelaySeconds));
            else
                seconds = value.get<int64>();
        }
        if (cancel && seconds)
            fields.emplace_back("cancel", "Cancel a shutdown or schedule one, not both");
        if (!fields.empty())
            return AdminResponse::Invalid("The shutdown request has problems", std::move(fields));

        if (cancel)
        {
            bool const cancelled = CancelScheduledStop();
            LogLifecycle(LogLevel::Info, fmt::format("The admin API {} (request {})", cancelled ? "cancelled the pending shutdown" : "found no shutdown to cancel", request.Id));
            nlohmann::json answer;
            answer["cancelled"] = cancelled;
            return AdminResponse::Json(200, answer.dump());
        }
        int64 const delay = seconds.value_or(0);
        LogLifecycle(LogLevel::Info, fmt::format("The admin API asked {} to stop {} (request {})", _info.Name, delay == 0 ? std::string("now") : fmt::format("in {}", Ambrose::FormatDuration(Seconds(delay))), request.Id));
        if (delay == 0)
            RequestStop("the admin API");
        else
            ScheduleStop(Seconds(delay), "the admin API");
        nlohmann::json answer;
        answer["stopping_in"] = delay;
        return AdminResponse::Json(202, answer.dump());
    });
}

bool ServerApp::ReloadAdminApi()
{
    if (!_admin)
        return false;
    std::vector<std::string> problems;
    ListenerSettings const settings = ListenerSettings::Load(_config, "Admin", _info.AdminPort, &problems);
    for (std::string const& problem : problems)
        AMBROSE_LOG(_log, LogLevel::Warn, "server.admin", "{}", problem);
    return _admin->Reload(settings);
}

int ServerApp::Run(std::vector<std::string> const& arguments)
{
    _lifecycle = AppLifecycle::Stopped;
    _stopScheduled = false;
    AppOptions const options = AppOptions::Parse(arguments, _info.DefaultConfigFile);
    _checkOnly = options.CheckOnly;
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

    _startedAt = std::chrono::steady_clock::now();
    _lifecycle = AppLifecycle::Starting;
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

    if (!StartAdminApi())
    {
        LogLifecycle(LogLevel::Error, fmt::format("{} failed to start", _info.Name));
        _work.reset();
        FinishShutdown();
        return EXIT_FAILURE;
    }

    _starting = true;
    bool const started = OnStart();
    _starting = false;
    if (!started)
    {
        bool const stopped = IsStopping() || _stopRequested.load();
        if (stopped)
            LogLifecycle(LogLevel::Info, fmt::format("{} stopped before it finished starting", _info.Name));
        else
            LogLifecycle(LogLevel::Error, fmt::format("{} failed to start", _info.Name));
        if (_admin)
            _admin->Stop();
        _admin.reset();
        _logStream.reset();
        _work.reset();
        FinishShutdown();
        return stopped ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    bool const stoppedWhileStarting = IsStopping();

    for (std::string const& name : _log.GetPendingAppenderNames())
        AMBROSE_LOG(_log, LogLevel::Warn, "server.logging", "appender {} has a type this app does not provide and stays inactive", name);

    _lastUpdate = std::chrono::steady_clock::now();
    if (stoppedWhileStarting)
        _io.Restart();
    else
    {
        if (GetUpdateInterval().count() > 0)
            ScheduleUpdate();
        _lifecycle = AppLifecycle::Running;
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
    if (_admin)
        _admin->Stop();
    _admin.reset();
    _logStream.reset();
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
    return IsStopping() || _stopRequested.load();
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
    if (IsStopping())
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
    _lifecycle = AppLifecycle::Stopped;
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
        if (error || IsStopping())
            return;
        auto const now = std::chrono::steady_clock::now();
        auto const diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastUpdate);
        _lastUpdate = now;
        if (GetUpdateInterval().count() > 0)
        {
            OnUpdate(diff);
            auto const finished = std::chrono::steady_clock::now();
            double const tookMs = std::chrono::duration<double, std::milli>(finished - now).count();
            std::lock_guard const lock(_statusMutex);
            _ticks.emplace_back(finished, tookMs);
            auto const cutoff = finished - std::chrono::seconds(AdminTickWindow{}.WindowSeconds);
            while (!_ticks.empty() && _ticks.front().first < cutoff)
                _ticks.pop_front();
        }
        ScheduleUpdate();
    });
}

void ServerApp::ScheduleStop(Seconds delay, std::string reason)
{
    _stopScheduled = true;
    asio::post(_io.GetExecutor(), [this, delay, reason = std::move(reason)]
    {
        if (!_stopScheduled.load())
            return;
        LogLifecycle(LogLevel::Info, fmt::format("{} stops in {}", _info.Name, Ambrose::FormatDuration(delay)));
        _shutdownTimer.expires_after(delay);
        _shutdownTimer.async_wait([this, reason](std::error_code const& error)
        {
            if (error || !_stopScheduled.exchange(false))
                return;
            StopNow(reason);
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
    if (IsStopping() || !_work)
        return;
    _lifecycle = AppLifecycle::Stopping;
    _stopScheduled = false;
    LogLifecycle(LogLevel::Info, fmt::format("{} shutting down after {}", _info.Name, reason));
    _shutdownTimer.cancel();
    _updateTimer.cancel();
    if (_signals)
        _signals->Cancel();
    _work.reset();
}
