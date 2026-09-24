/*
 * Project Ambrose by Imjustchico
 * One app the supervisor runs: a controller thread of its own starts it with its config, its output going to the supervisor's files and its input a pipe, calls it ready when its admin API reports it running or, with the admin API off, when it prints its ready line, stops it by asking its admin API to shut down with the countdown, else by a shutdown line on its input, else by Ctrl+Break or SIGTERM to its group, interrupts it halfway through its stop timeout and ends its whole tree when the timeout passes, restarts it a second after it exits unexpectedly from running, records every exit with its code, saves its desired state and process identity whenever they change, and takes back the process an earlier supervisor started while that identity still matches.
 */

#ifndef AMBROSE_MANAGEDAPP_H
#define AMBROSE_MANAGEDAPP_H

#include "AdminClient.h"
#include "AppDefinition.h"
#include "ChildProcess.h"
#include "OutputLog.h"
#include "SupervisorState.h"
#include "Types.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

class Log;

enum class AppState : uint8
{
    Offline,
    Starting,
    Running,
    Stopping,
    Crashed
};

enum class PowerAction : uint8
{
    Start,
    Stop,
    Restart,
    Kill
};

enum class StopMethod : uint8
{
    None,
    AdminApi,
    Input,
    Interrupt,
    EndTree
};

struct AppExit
{
    int64 EpochMs = 0;
    std::optional<int64> Code;
    std::optional<int> Signal;
    bool Requested = false;
    AppState During = AppState::Running;
    int64 UptimeMs = 0;
};

struct AppIdentity
{
    std::string Role;
    std::string Realm;
    std::string Address;
    uint16 Port = 0;
    std::string Revision;
};

struct AppSnapshot
{
    std::string Name;
    std::string ProgramName;
    std::filesystem::path Program;
    std::filesystem::path Config;
    AppState State = AppState::Offline;
    bool Watching = false;
    bool WantRunning = false;
    std::optional<int64> ProcessId;
    bool Adopted = false;
    int64 StartedEpochMs = 0;
    int64 ReadyEpochMs = 0;
    bool AdminEnabled = false;
    std::string AdminHost;
    uint16 AdminPort = 0;
    std::string AdminProblem;
    StopMethod Stop = StopMethod::None;
    int64 StopRequestedEpochMs = 0;
    int64 RestartEpochMs = 0;
    uint32 Crashes = 0;
    uint32 FailedStarts = 0;
    uint32 Restarts = 0;
    std::vector<AppExit> Exits;
    std::string Message;
    AppIdentity Identity;
};

struct PowerResult
{
    bool Accepted = false;
    int Status = 202;
    std::string Code;
    std::string Message;
};

class ManagedApp
{
public:
    static constexpr std::chrono::seconds RestartDelay{ 1 };
    static constexpr std::chrono::milliseconds TickInterval{ 100 };
    static constexpr std::chrono::milliseconds FirstHealthDelay{ 200 };
    static constexpr std::chrono::milliseconds HealthInterval{ 500 };
    static constexpr std::chrono::milliseconds HealthTimeout{ 1000 };
    static constexpr std::chrono::milliseconds ShutdownRequestTimeout{ 5000 };
    static constexpr uint32 MaxCountdownSeconds = 86400;
    static constexpr std::size_t MaxExits = 20;

    ManagedApp(AppDefinition definition, SupervisorState& state, std::filesystem::path const& outputFolder, uint64 maxOutputBytes, std::filesystem::path dataFolder, ChildBreakSender sendBreak, Log& log);
    ~ManagedApp();

    ManagedApp(ManagedApp const&) = delete;
    ManagedApp& operator=(ManagedApp const&) = delete;

    void Start();
    void Shutdown();

    PowerResult Power(PowerAction action, uint32 countdownSeconds);
    AppSnapshot Snapshot() const;
    std::vector<OutputLine> Output(OutputRun run, uint64 after) const;
    std::optional<AdminClient> GetAdminClient() const;
    AppDefinition const& GetDefinition() const noexcept { return _definition; }

    static std::string_view StateName(AppState state) noexcept;
    static std::string_view StopMethodName(StopMethod method) noexcept;
    static std::string_view ActionName(PowerAction action) noexcept;
    static std::optional<PowerAction> ParseAction(std::string_view text) noexcept;
    static std::string DescribeExit(std::string_view name, AppExit const& exit);

private:
    using Clock = std::chrono::steady_clock;

    struct Command
    {
        PowerAction Action = PowerAction::Start;
        uint32 Countdown = 0;
    };

    void Run();
    void Begin();
    void Handle(Command const& command);
    void Step();
    void Launch();
    void BeginStop(uint32 countdown);
    void Escalate();
    void CheckReady(std::vector<OutputLine> const& lines);
    void CheckHealth();
    void MarkReady(std::string const& reason);
    void OnExit();
    void LoadAdmin();
    void FetchIdentity();
    void SetWantRunning(bool wantRunning);
    void Save();
    void Note(std::string const& text, bool warning = false);
    AppState GetState() const;
    bool WantsRunning() const;

    AppDefinition _definition;
    SupervisorState& _state;
    OutputLog _output;
    std::filesystem::path _dataFolder;
    ChildBreakSender _sendBreak;
    Log& _log;
    std::string _readySuffix;
    std::string _pendingNote;

    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<Command> _commands;
    bool _quit = false;
    AppSnapshot _view;
    std::optional<AdminClient> _admin;
    std::thread _thread;

    ChildProcessHandle _process;
    Clock::time_point _startedAt{};
    Clock::time_point _nextHealth{};
    Clock::time_point _interruptAt{};
    Clock::time_point _endAt{};
    Clock::time_point _restartAt{};
    bool _restartPending = false;
    bool _stopRequested = false;
    bool _interrupted = false;
    bool _ended = false;
    bool _restartAfterStop = false;
    bool _startTimedOut = false;
    bool _watchStart = true;
};

#endif
