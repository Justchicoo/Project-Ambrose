/*
 * Project Ambrose by Imjustchico
 * Runs one app's controller: commands queue under a lock and run on the controller thread, which every tenth of a second reads the app's new output, notices its exit, looks for readiness while it starts and ends a start that runs past its timeout, escalates a stop that has not finished, and starts it again once a restart falls due; the app's admin API is found from the app's own config and token exactly as the app finds them, an adopted app with its admin API off counts as running at once because nothing else could say so, a process it would not take back is said so at the top of the run that replaces it, and a start that never became ready is recorded and not tried again until someone starts it.
 */

#include "ManagedApp.h"
#include "ListenerSettings.h"
#include "AdminToken.h"
#include "ConfigMgr.h"
#include "Log.h"
#include "ThreadName.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <utility>

namespace
{
    int64 NowEpochMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string DescribeStop(StopMethod method, uint32 countdown)
    {
        std::string const after = countdown == 0 ? std::string("now") : fmt::format("after a countdown of {} s", countdown);
        switch (method)
        {
            case StopMethod::AdminApi: return fmt::format("through its admin API, {}", after);
            case StopMethod::Input: return fmt::format("with a shutdown line on its input, {}", after);
            case StopMethod::Interrupt: return "with an interrupt to its process group, since neither its admin API nor its input could take the request";
            case StopMethod::EndTree: return "by ending its process tree, since nothing gentler could reach it";
            case StopMethod::None: break;
        }
        return "without a way to reach it";
    }
}

ManagedApp::ManagedApp(AppDefinition definition, SupervisorState& state, std::filesystem::path const& outputFolder, uint64 maxOutputBytes, std::filesystem::path dataFolder, ChildBreakSender sendBreak, Log& log)
    : _definition(std::move(definition)), _state(state), _output(outputFolder / ConfigMgr::PathFromUtf8(_definition.Name), maxOutputBytes), _dataFolder(std::move(dataFolder)), _sendBreak(std::move(sendBreak)), _log(log)
{
    _readySuffix = fmt::format("[server.{}] {} ready", _definition.ProgramName, _definition.ProgramName);
    _view.Name = _definition.Name;
    _view.ProgramName = _definition.ProgramName;
    _view.Program = _definition.Program;
    _view.Config = _definition.Config;
    _view.WantRunning = _definition.Autostart;
}

ManagedApp::~ManagedApp()
{
    Shutdown();
}

std::string_view ManagedApp::StateName(AppState state) noexcept
{
    switch (state)
    {
        case AppState::Offline: return "offline";
        case AppState::Starting: return "starting";
        case AppState::Running: return "running";
        case AppState::Stopping: return "stopping";
        case AppState::Crashed: return "crashed";
    }
    return "offline";
}

std::string_view ManagedApp::StopMethodName(StopMethod method) noexcept
{
    switch (method)
    {
        case StopMethod::None: return "none";
        case StopMethod::AdminApi: return "admin_api";
        case StopMethod::Input: return "input";
        case StopMethod::Interrupt: return "interrupt";
        case StopMethod::EndTree: return "end_tree";
    }
    return "none";
}

std::string_view ManagedApp::ActionName(PowerAction action) noexcept
{
    switch (action)
    {
        case PowerAction::Start: return "start";
        case PowerAction::Stop: return "stop";
        case PowerAction::Restart: return "restart";
        case PowerAction::Kill: return "kill";
    }
    return "start";
}

std::optional<PowerAction> ManagedApp::ParseAction(std::string_view text) noexcept
{
    for (PowerAction const action : { PowerAction::Start, PowerAction::Stop, PowerAction::Restart, PowerAction::Kill })
        if (ActionName(action) == text)
            return action;
    return std::nullopt;
}

std::string ManagedApp::DescribeExit(std::string_view name, AppExit const& exit)
{
    std::string how;
    if (exit.Signal)
        how = fmt::format("was ended by signal {}", *exit.Signal);
    else if (exit.Code && *exit.Code >= 0xC0000000LL && *exit.Code <= 0xFFFFFFFFLL)
        how = fmt::format("ended with exception 0x{:08X}", static_cast<uint64>(*exit.Code));
    else if (exit.Code)
        how = fmt::format("exited with code {}", *exit.Code);
    else
        how = "exited, and its exit code could not be read because an earlier supervisor started it";
    std::string_view const why = exit.Requested ? "as asked" : exit.During == AppState::Starting ? "before it was ready" : "without being asked";
    return fmt::format("{} {} {} after {} ms", name, how, why, exit.UptimeMs);
}

void ManagedApp::Note(std::string const& text, bool warning)
{
    _output.Note(text);
    if (warning)
        AMBROSE_LOG(_log, LogLevel::Warn, "server.supervisor", "{}", text);
    else
        AMBROSE_LOG(_log, LogLevel::Info, "server.supervisor", "{}", text);
}

AppState ManagedApp::GetState() const
{
    std::lock_guard<std::mutex> const lock(_mutex);
    return _view.State;
}

bool ManagedApp::WantsRunning() const
{
    std::lock_guard<std::mutex> const lock(_mutex);
    return _view.WantRunning;
}

void ManagedApp::SetWantRunning(bool wantRunning)
{
    std::lock_guard<std::mutex> const lock(_mutex);
    _view.WantRunning = wantRunning;
}

void ManagedApp::Save()
{
    SavedApp saved;
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        saved.WantRunning = _view.WantRunning;
        saved.StartedEpochMs = _view.StartedEpochMs;
    }
    if (_process)
        saved.Process = _process.GetIdentity();
    std::string error;
    if (!_state.Put(_definition.Name, saved, error))
        Note(fmt::format("The supervisor could not save the state of {}: {}", _definition.Name, error), true);
}

void ManagedApp::Start()
{
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        if (_thread.joinable())
            return;
        _quit = false;
        _view.Watching = true;
    }
    _thread = std::thread([this] { Run(); });
}

void ManagedApp::Shutdown()
{
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        _quit = true;
        _view.Watching = false;
    }
    _wake.notify_all();
    if (_thread.joinable())
        _thread.join();
}

void ManagedApp::Run()
{
    Ambrose::Threading::SetCurrentThreadName(fmt::format("Supervise {}", _definition.Name));
    Begin();
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_quit)
    {
        while (!_commands.empty() && !_quit)
        {
            Command const command = _commands.front();
            _commands.pop_front();
            lock.unlock();
            Handle(command);
            lock.lock();
        }
        if (_quit)
            break;
        lock.unlock();
        Step();
        lock.lock();
        _wake.wait_for(lock, TickInterval, [this] { return _quit || !_commands.empty(); });
    }
}

void ManagedApp::Begin()
{
    std::optional<SavedApp> const saved = _state.Get(_definition.Name);
    SetWantRunning(saved ? saved->WantRunning : _definition.Autostart);
    if (saved && saved->Process)
    {
        std::string error;
        ChildProcessHandle adopted = ChildProcessHandle::Adopt(*saved->Process, _sendBreak, error);
        if (adopted)
        {
            _process = std::move(adopted);
            _output.Attach();
            LoadAdmin();
            _startedAt = Clock::now();
            _nextHealth = _startedAt;
            _watchStart = false;
            bool admin = false;
            {
                std::lock_guard<std::mutex> const lock(_mutex);
                _view.State = AppState::Starting;
                _view.ProcessId = _process.GetIdentity().Id;
                _view.Adopted = true;
                _view.StartedEpochMs = saved->StartedEpochMs;
                _view.ReadyEpochMs = 0;
                admin = _admin.has_value();
            }
            Note(fmt::format("Took back {} as process {}, which it has been since before the supervisor restarted", _definition.Name, _process.GetIdentity().Id));
            if (!admin)
                MarkReady("it was already running, and with its admin API off nothing else can say more");
            else
                CheckReady(_output.Lines(OutputRun::Current, 0));
            return;
        }
        std::string const refused = fmt::format("Did not take back process {} for {}: {}", saved->Process->Id, _definition.Name, error);
        if (WantsRunning())
            _pendingNote = refused;
        else
            Note(refused, true);
        Save();
    }
    if (WantsRunning())
    {
        Launch();
        return;
    }
    _output.Attach();
    std::lock_guard<std::mutex> const lock(_mutex);
    _view.State = AppState::Offline;
}

void ManagedApp::LoadAdmin()
{
    std::optional<AdminClient> client;
    std::string problem;
    ConfigMgr config;
    ConfigLoadResult const loaded = config.LoadInitial(_definition.Config);
    if (!loaded.Succeeded())
        problem = fmt::format("its config {} could not be read, so its admin API is unknown", ConfigMgr::PathToUtf8(_definition.Config));
    else
    {
        ListenerSettings const settings = ListenerSettings::Load(config, "Admin", 0);
        if (!settings.Enable)
            problem = "its admin API is off (Admin.Enable = 0), so the supervisor goes by its output";
        else if (settings.Port == 0)
            problem = "its admin API has no fixed port (Admin.Port = 0), so the supervisor cannot reach it";
        else
        {
            AdminTokenResult const token = AdminToken::Resolve(settings, _definition.ProgramName, _dataFolder, _definition.Config.parent_path());
            if (!token.Succeeded())
                problem = token.Error;
            else
                client.emplace(AdminClient::ConnectHost(settings.BindIp), settings.Port, token.Token);
        }
    }
    std::lock_guard<std::mutex> const lock(_mutex);
    _admin = client;
    _view.AdminEnabled = client.has_value();
    _view.AdminHost = client ? client->GetHost() : std::string();
    _view.AdminPort = client ? client->GetPort() : 0;
    _view.AdminProblem = problem;
}

void ManagedApp::Launch()
{
    LoadAdmin();
    std::string error;
    if (!_output.BeginRun(error))
        Note(fmt::format("The output of {} will not be kept: {}", _definition.Name, error), true);
    ChildLaunchOptions options;
    options.Program = _definition.Program;
    options.Arguments = { "--config", ConfigMgr::PathToUtf8(_definition.Config) };
    options.WorkingDirectory = _definition.WorkingDirectory;
    options.OutputFile = _output.OutputFile();
    options.ErrorFile = _output.ErrorFile();
    options.KeepInput = true;
    options.SendBreak = _sendBreak;
    if (!_pendingNote.empty())
    {
        Note(_pendingNote, true);
        _pendingNote.clear();
    }
    ChildProcessHandle process = ChildProcessHandle::Launch(options, error);
    int64 const now = NowEpochMs();
    if (!process)
    {
        {
            std::lock_guard<std::mutex> const lock(_mutex);
            _view.Exits.push_back(AppExit{ now, std::nullopt, std::nullopt, false, AppState::Starting, 0 });
            if (_view.Exits.size() > MaxExits)
                _view.Exits.erase(_view.Exits.begin());
            ++_view.FailedStarts;
            _view.State = AppState::Crashed;
            _view.ProcessId.reset();
            _view.Message = error;
        }
        Note(fmt::format("{} could not be started: {}", _definition.Name, error), true);
        Save();
        return;
    }
    _process = std::move(process);
    _startedAt = Clock::now();
    _nextHealth = _startedAt + FirstHealthDelay;
    _stopRequested = false;
    _interrupted = false;
    _ended = false;
    _startTimedOut = false;
    _watchStart = true;
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        _view.State = AppState::Starting;
        _view.ProcessId = _process.GetIdentity().Id;
        _view.Adopted = false;
        _view.StartedEpochMs = now;
        _view.ReadyEpochMs = 0;
        _view.Stop = StopMethod::None;
        _view.StopRequestedEpochMs = 0;
        _view.RestartEpochMs = 0;
        _view.Message.clear();
        _view.Identity = AppIdentity{};
    }
    Note(fmt::format("Started {} as process {}", _definition.Name, _process.GetIdentity().Id));
    Save();
}

void ManagedApp::Handle(Command const& command)
{
    bool const running = static_cast<bool>(_process);
    switch (command.Action)
    {
        case PowerAction::Start:
            SetWantRunning(true);
            _restartPending = false;
            if (running)
            {
                Save();
                return;
            }
            Launch();
            return;
        case PowerAction::Restart:
            SetWantRunning(true);
            _restartPending = false;
            if (!running)
            {
                Launch();
                return;
            }
            _restartAfterStop = true;
            Save();
            if (GetState() != AppState::Stopping)
                BeginStop(command.Countdown);
            return;
        case PowerAction::Stop:
        case PowerAction::Kill:
            break;
    }
    SetWantRunning(false);
    _restartPending = false;
    _restartAfterStop = false;
    if (!running)
    {
        {
            std::lock_guard<std::mutex> const lock(_mutex);
            _view.State = AppState::Offline;
            _view.RestartEpochMs = 0;
        }
        Save();
        return;
    }
    Save();
    if (command.Action == PowerAction::Stop)
    {
        if (GetState() != AppState::Stopping)
            BeginStop(command.Countdown);
        return;
    }
    _stopRequested = true;
    _ended = true;
    _endAt = Clock::now() + _definition.StopTimeout;
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        _view.State = AppState::Stopping;
        _view.Stop = StopMethod::EndTree;
        _view.StopRequestedEpochMs = NowEpochMs();
    }
    std::string error;
    if (_process.EndTree(error))
        Note(fmt::format("Ended the process tree of {} as asked", _definition.Name));
    else
        Note(fmt::format("The process tree of {} could not be ended: {}", _definition.Name, error), true);
}

void ManagedApp::BeginStop(uint32 countdown)
{
    _stopRequested = true;
    _interrupted = false;
    _ended = false;
    std::optional<AdminClient> const admin = GetAdminClient();
    StopMethod method = StopMethod::None;
    std::vector<std::string> misses;
    if (admin)
    {
        nlohmann::json body;
        body["seconds"] = countdown;
        AdminClientResponse const answer = admin->Send({ "POST", "/api/shutdown", body.dump(), "application/json", {} }, ShutdownRequestTimeout);
        if (answer.Answered && answer.Status == 202)
            method = StopMethod::AdminApi;
        else
            misses.push_back(answer.Answered ? fmt::format("its admin API answered {}", answer.Status) : answer.Error);
    }
    std::string error;
    if (method == StopMethod::None && _process.HasInput())
    {
        std::string const line = countdown == 0 ? std::string("shutdown\n") : fmt::format("shutdown {}\n", countdown);
        if (_process.WriteInput(line, error))
            method = StopMethod::Input;
        else
            misses.push_back(error);
    }
    if (method == StopMethod::None)
    {
        if (_process.Interrupt(error))
        {
            method = StopMethod::Interrupt;
            _interrupted = true;
        }
        else
            misses.push_back(error);
    }
    if (method == StopMethod::None)
    {
        if (_process.EndTree(error))
        {
            method = StopMethod::EndTree;
            _ended = true;
        }
        else
            misses.push_back(error);
    }
    Clock::time_point const now = Clock::now();
    _interruptAt = now + std::chrono::seconds(countdown) + _definition.StopTimeout / 2;
    _endAt = now + std::chrono::seconds(countdown) + _definition.StopTimeout;
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        _view.State = AppState::Stopping;
        _view.Stop = method;
        _view.StopRequestedEpochMs = NowEpochMs();
    }
    std::string text = fmt::format("Stopping {} {}", _definition.Name, DescribeStop(method, countdown));
    for (std::string const& miss : misses)
        text += fmt::format("; {}", miss);
    Note(text, method == StopMethod::Interrupt || method == StopMethod::EndTree || method == StopMethod::None);
}

void ManagedApp::Escalate()
{
    Clock::time_point const now = Clock::now();
    std::string error;
    if (!_interrupted && now >= _interruptAt)
    {
        _interrupted = true;
        if (_process.Interrupt(error))
            Note(fmt::format("{} has not stopped halfway through its stop timeout of {} s, so its process group was interrupted", _definition.Name, _definition.StopTimeout.count()), true);
        else
            Note(fmt::format("{} has not stopped halfway through its stop timeout, and its process group could not be interrupted: {}", _definition.Name, error), true);
    }
    if (!_ended && now >= _endAt)
    {
        _ended = true;
        if (_process.EndTree(error))
            Note(fmt::format("{} did not stop within its stop timeout of {} s, so its process tree was ended", _definition.Name, _definition.StopTimeout.count()), true);
        else
            Note(fmt::format("{} did not stop within its stop timeout, and its process tree could not be ended: {}", _definition.Name, error), true);
    }
}

void ManagedApp::CheckReady(std::vector<OutputLine> const& lines)
{
    for (OutputLine const& line : lines)
    {
        if (line.Stream != "stdout" || line.Text.size() < _readySuffix.size())
            continue;
        if (std::string_view(line.Text).substr(line.Text.size() - _readySuffix.size()) == _readySuffix)
        {
            MarkReady("it printed its ready line");
            return;
        }
    }
}

void ManagedApp::CheckHealth()
{
    Clock::time_point const now = Clock::now();
    if (now < _nextHealth)
        return;
    _nextHealth = now + HealthInterval;
    std::optional<AdminClient> const admin = GetAdminClient();
    if (!admin)
        return;
    AdminClientResponse const answer = admin->Send({ "GET", "/api/health", {}, "application/json", {} }, HealthTimeout);
    if (!answer.Answered || answer.Status != 200)
        return;
    nlohmann::json const body = nlohmann::json::parse(answer.Body, nullptr, false);
    if (body.is_object() && body.value("state", std::string()) == "running")
        MarkReady("its admin API reports it running");
}

void ManagedApp::MarkReady(std::string const& reason)
{
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        if (_view.State != AppState::Starting)
            return;
        _view.State = AppState::Running;
        _view.ReadyEpochMs = NowEpochMs();
    }
    Note(fmt::format("{} is ready: {}", _definition.Name, reason));
    FetchIdentity();
}

void ManagedApp::FetchIdentity()
{
    std::optional<AdminClient> const admin = GetAdminClient();
    if (!admin)
        return;
    AdminClientResponse const answer = admin->Send({ "GET", "/api/apps", {}, "application/json", {} }, HealthTimeout);
    if (!answer.Answered || answer.Status != 200)
        return;
    nlohmann::json const body = nlohmann::json::parse(answer.Body, nullptr, false);
    if (!body.is_array() || body.empty() || !body[0].is_object())
        return;
    nlohmann::json const& entry = body[0];
    AppIdentity identity;
    identity.Role = entry.value("role", std::string());
    identity.Realm = entry.value("realm", std::string());
    identity.Address = entry.value("address", std::string());
    identity.Port = entry.value("port", uint16{ 0 });
    identity.Revision = entry.value("revision", std::string());
    std::lock_guard<std::mutex> const lock(_mutex);
    _view.Identity = std::move(identity);
}

void ManagedApp::OnExit()
{
    std::optional<ChildExit> const exit = _process.GetExit();
    int64 const now = NowEpochMs();
    AppExit record;
    record.EpochMs = now;
    if (exit)
    {
        record.Code = exit->Code;
        record.Signal = exit->Signal;
    }
    record.Requested = _stopRequested;
    _process = ChildProcessHandle();
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        record.During = _view.State;
        record.UptimeMs = _view.StartedEpochMs == 0 ? 0 : now - _view.StartedEpochMs;
        _view.Exits.push_back(record);
        if (_view.Exits.size() > MaxExits)
            _view.Exits.erase(_view.Exits.begin());
        _view.ProcessId.reset();
        _view.Adopted = false;
        _view.Stop = StopMethod::None;
        _view.StopRequestedEpochMs = 0;
    }
    _output.Poll();
    Note(DescribeExit(_definition.Name, record), !record.Requested);
    bool const requested = _stopRequested;
    _stopRequested = false;
    if (requested)
    {
        if (_restartAfterStop)
        {
            _restartAfterStop = false;
            {
                std::lock_guard<std::mutex> const lock(_mutex);
                ++_view.Restarts;
            }
            Launch();
            return;
        }
        {
            std::lock_guard<std::mutex> const lock(_mutex);
            _view.State = AppState::Offline;
        }
        Save();
        return;
    }
    bool restart = false;
    {
        std::lock_guard<std::mutex> const lock(_mutex);
        if (record.During == AppState::Running)
        {
            ++_view.Crashes;
            restart = _view.WantRunning;
            _view.Message = restart ? fmt::format("It exited without being asked and starts again in {} s", RestartDelay.count()) : "It exited without being asked";
        }
        else
        {
            ++_view.FailedStarts;
            _view.Message = _startTimedOut ? fmt::format("It did not become ready within {} s", _definition.StartTimeout.count()) : "It exited before it was ready; start it again once the cause in its output is fixed";
        }
        _view.State = AppState::Crashed;
        _view.RestartEpochMs = restart ? now + std::chrono::duration_cast<std::chrono::milliseconds>(RestartDelay).count() : 0;
    }
    _restartPending = restart;
    _restartAt = Clock::now() + RestartDelay;
    Save();
}

void ManagedApp::Step()
{
    std::vector<OutputLine> const lines = _output.Poll();
    if (_process)
    {
        if (_process.WaitForExit(std::chrono::milliseconds(0)))
        {
            OnExit();
            return;
        }
        AppState const state = GetState();
        if (state == AppState::Starting)
        {
            CheckReady(lines);
            if (GetState() == AppState::Starting)
                CheckHealth();
            if (GetState() == AppState::Starting && _watchStart && !_startTimedOut && Clock::now() - _startedAt >= _definition.StartTimeout)
            {
                _startTimedOut = true;
                std::string error;
                if (_process.EndTree(error))
                    Note(fmt::format("{} did not become ready within {} s, so its process tree was ended", _definition.Name, _definition.StartTimeout.count()), true);
                else
                    Note(fmt::format("{} did not become ready within {} s, and its process tree could not be ended: {}", _definition.Name, _definition.StartTimeout.count(), error), true);
            }
        }
        else if (state == AppState::Stopping)
            Escalate();
        return;
    }
    if (_restartPending && Clock::now() >= _restartAt)
    {
        _restartPending = false;
        if (!WantsRunning())
            return;
        {
            std::lock_guard<std::mutex> const lock(_mutex);
            ++_view.Restarts;
        }
        Note(fmt::format("Starting {} again after it exited without being asked", _definition.Name));
        Launch();
    }
}

PowerResult ManagedApp::Power(PowerAction action, uint32 countdownSeconds)
{
    std::lock_guard<std::mutex> const lock(_mutex);
    if (!_view.Watching)
        return { false, 503, "not_watching", fmt::format("The supervisor is not watching {}", _definition.Name) };
    bool const alive = _view.State == AppState::Starting || _view.State == AppState::Running || _view.State == AppState::Stopping;
    if (action == PowerAction::Start && alive)
        return { false, 409, "already_running", fmt::format("{} is already {}", _definition.Name, StateName(_view.State)) };
    if (action == PowerAction::Stop && _view.State == AppState::Stopping)
        return { false, 409, "already_stopping", fmt::format("{} is already stopping", _definition.Name) };
    if (action == PowerAction::Stop && !alive && !_view.WantRunning)
        return { false, 409, "already_stopped", fmt::format("{} is already {}", _definition.Name, StateName(_view.State)) };
    if (action == PowerAction::Kill && !alive)
        return { false, 409, "not_running", fmt::format("{} is not running", _definition.Name) };
    _commands.push_back(Command{ action, countdownSeconds });
    _wake.notify_all();
    return { true, 202, {}, fmt::format("{} {}: accepted", ActionName(action), _definition.Name) };
}

AppSnapshot ManagedApp::Snapshot() const
{
    std::lock_guard<std::mutex> const lock(_mutex);
    return _view;
}

std::vector<OutputLine> ManagedApp::Output(OutputRun run, uint64 after) const
{
    return _output.Lines(run, after);
}

std::optional<AdminClient> ManagedApp::GetAdminClient() const
{
    std::lock_guard<std::mutex> const lock(_mutex);
    return _admin;
}
