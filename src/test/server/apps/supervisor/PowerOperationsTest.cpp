/*
 * Project Ambrose by Imjustchico
 * Runs power as tracked operations over copies of the helper program named as the apps they stand in for: the protected hours read their windows and find the one covering a moment, a restart while a backup restore holds the app is refused naming the restore while the app reports restoring with its progress and its console stays readable, a restart inside the protected hours is refused naming the window and goes through only for an owner who gives a reason, which the audit log keeps, while a scheduled one is refused the same way, a kill during a stop that never finishes ends the app as a requested exit, a stack restart stops the gameserver before the loginserver and starts the loginserver before the gameserver, a power request answers 202 with an operation id whose progress and result reach the observers and the audit log, and a disabled app stops and refuses a start naming the reason until it is enabled.
 */

#include "AdminAuth.h"
#include "AdminRouter.h"
#include "AdminStatus.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelAudit.h"
#include "PanelEventCatalog.h"
#include "PanelStore.h"
#include "ProtectedHours.h"
#include "Supervisor.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    std::filesystem::path Utf8Path(std::string_view text)
    {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }

    std::filesystem::path HelperPath()
    {
        return Utf8Path(AMBROSE_CHILD_PROCESS_HELPER);
    }

    std::string Slashes(std::filesystem::path const& path)
    {
        std::string text = ConfigMgr::PathToUtf8(path);
        std::replace(text.begin(), text.end(), '\\', '/');
        return text;
    }

    std::filesystem::path CopyHelperAs(std::filesystem::path const& folder, std::string const& program)
    {
        std::filesystem::create_directories(folder);
        std::filesystem::path const target = folder / (program + HelperPath().extension().string());
        std::filesystem::copy_file(HelperPath(), target, std::filesystem::copy_options::overwrite_existing);
        std::string_view dlls = AMBROSE_CHILD_PROCESS_HELPER_DLLS;
        while (!dlls.empty())
        {
            std::size_t const end = dlls.find('|');
            std::filesystem::path const dll = Utf8Path(std::string(dlls.substr(0, end)));
            std::filesystem::copy_file(dll, folder / dll.filename(), std::filesystem::copy_options::skip_existing);
            dlls = end == std::string_view::npos ? std::string_view() : dlls.substr(end + 1);
        }
        return target;
    }

    ChildBreakSender HelperBreak()
    {
        return [](int64 group, std::string& error)
        {
            ChildProcessOptions options;
            options.Program = HelperPath();
            options.Arguments = { "console-break", std::to_string(group) };
            options.Timeout = 10s;
            ChildProcessResult const result = ChildProcess::Run(options);
            if (result.Succeeded())
                return true;
            error = result.Error.empty() ? "the helper could not send Ctrl+Break" : result.Error;
            return false;
        };
    }

    void EndFromOutside(int64 id)
    {
        std::optional<ChildProcessIdentity> const identity = ChildProcessHandle::Describe(id);
        if (!identity)
            return;
        std::string error;
        ChildProcessHandle handle = ChildProcessHandle::Adopt(*identity, {}, error);
        if (handle)
            handle.EndTree(error);
    }

    AdminRequest Request(std::string method, std::string path, std::string body = {})
    {
        AdminRequest request;
        request.Method = std::move(method);
        request.Path = std::move(path);
        request.RemoteAddress = "127.0.0.1";
        request.Authorization = std::string("Bearer ") + Token;
        request.Body = std::move(body);
        request.Id = "0123456789abcdef";
        return request;
    }

    std::string ReadyLine(std::string const& program)
    {
        return fmt::format("2026-10-01_00:00:00.000 INFO  [server.{}] {} ready", program, program);
    }

    struct Stand
    {
        std::string Name;
        std::string Program;
        std::vector<std::string> Script;
        std::string Extra;
    };

    std::vector<std::string> Serving(std::string const& program)
    {
        return { "echo", ReadyLine(program), "wait-for-stop" };
    }

    class StackRig
    {
    public:
        explicit StackRig(std::vector<Stand> const& apps, std::string const& extra = {})
        {
            std::string names;
            std::string sections;
            for (Stand const& app : apps)
            {
                std::filesystem::path const program = CopyHelperAs(_directory.Path() / "programs" / app.Name, app.Program);
                std::string lines;
                for (std::string const& line : app.Script)
                    lines += "#" + line + "\n";
                std::filesystem::path const script = _directory.Write(app.Name + ".conf", lines);
                names += (names.empty() ? "" : ", ") + app.Name;
                sections += fmt::format("App.{0}.Program = \"{1}\"\nApp.{0}.Config = \"{2}\"\n{3}", app.Name, Slashes(program), Slashes(script), app.Extra);
            }
            std::string const settings = fmt::format("Supervisor.Apps = {}\n{}Supervisor.StateFile = \"{}\"\nSupervisor.OutputDir = \"{}\"\n{}", names, sections,
                Slashes(_directory.Path() / "state.json"), Slashes(_directory.Path() / "output"), extra);
            EXPECT_TRUE(_config.LoadInitial(_directory.Write("supervisor.conf", settings)).Succeeded());
        }

        ~StackRig()
        {
            std::vector<int64> processes;
            if (_instance)
                for (AppSnapshot const& app : _instance->Snapshots())
                    if (app.ProcessId)
                        processes.push_back(*app.ProcessId);
            if (_instance)
            {
                _instance->Shutdown();
                _instance.reset();
            }
            for (int64 const process : processes)
                EndFromOutside(process);
        }

        bool Open(AppStatusObserver observer = {}, PowerObservers power = {})
        {
            _instance = std::make_unique<Supervisor>(_harness.GetLog(), HelperBreak());
            _instance->SetStatusObserver(std::move(observer));
            _instance->SetPowerHooks({ [this](AdminRequest const&) { return Owner; }, std::move(power) });
            std::vector<std::string> problems;
            std::string error;
            SupervisorSettings const settings = SupervisorSettings::Load(_config, _directory.Path() / "data", HelperPath().parent_path(), _directory.Path(), problems);
            bool const started = _instance->Start(_config, settings, true, problems, error);
            EXPECT_TRUE(error.empty()) << error;
            return started;
        }

        Supervisor& Instance() { return *_instance; }
        LogTestHarness& Harness() { return _harness; }
        LogTestDirectory& Directory() { return _directory; }

        AppSnapshot App(std::string_view name) const
        {
            for (AppSnapshot const& app : _instance->Snapshots())
                if (app.Name == name)
                    return app;
            return {};
        }

        bool WaitFor(std::string_view name, std::function<bool(AppSnapshot const&)> const& ready, std::chrono::milliseconds timeout = 30s) const
        {
            std::chrono::steady_clock::time_point const until = std::chrono::steady_clock::now() + timeout;
            while (std::chrono::steady_clock::now() < until)
            {
                if (ready(App(name)))
                    return true;
                std::this_thread::sleep_for(20ms);
            }
            return false;
        }

        bool Owner = false;

    private:
        LogTestHarness _harness;
        LogTestDirectory _directory;
        ConfigMgr _config;
        std::unique_ptr<Supervisor> _instance;
    };

    bool Is(AppState state, AppSnapshot const& app)
    {
        return app.State == state;
    }

    class Collected
    {
    public:
        PowerObservers Observers()
        {
            return { [this](PowerReport const& report)
                {
                    std::lock_guard<std::mutex> const lock(_mutex);
                    Accepted.push_back(report);
                },
                [this](PowerStep const& step)
                {
                    std::lock_guard<std::mutex> const lock(_mutex);
                    Steps.push_back(step);
                },
                [this](PowerReport const& report)
                {
                    std::lock_guard<std::mutex> const lock(_mutex);
                    Finished.push_back(report);
                } };
        }

        std::optional<PowerReport> ResultOf(std::string const& operation, std::chrono::milliseconds timeout = 60s)
        {
            std::chrono::steady_clock::time_point const until = std::chrono::steady_clock::now() + timeout;
            while (std::chrono::steady_clock::now() < until)
            {
                {
                    std::lock_guard<std::mutex> const lock(_mutex);
                    for (PowerReport const& report : Finished)
                        if (report.Operation == operation)
                            return report;
                }
                std::this_thread::sleep_for(20ms);
            }
            return std::nullopt;
        }

        std::vector<PowerStep> StepsOf(std::string const& operation)
        {
            std::lock_guard<std::mutex> const lock(_mutex);
            std::vector<PowerStep> found;
            for (PowerStep const& step : Steps)
                if (step.Operation == operation)
                    found.push_back(step);
            return found;
        }

        std::vector<PowerReport> Accepted;
        std::vector<PowerStep> Steps;
        std::vector<PowerReport> Finished;

    private:
        std::mutex _mutex;
    };

    std::chrono::system_clock::time_point At(int hour, int minute)
    {
        return std::chrono::sys_days{ std::chrono::year{ 2026 } / 10 / 1 } + std::chrono::hours(hour) + std::chrono::minutes(minute);
    }
}

TEST(ProtectedHoursTest, WindowsAreReadAndTheOneCoveringAMomentIsNamed)
{
    std::string error;
    std::optional<ProtectedHours> const hours = ProtectedHours::Parse(" 02:00-04:00, 22:30-01:15 ", "UTC", error);
    ASSERT_TRUE(hours.has_value()) << error;
    ASSERT_EQ(hours->Windows().size(), 2u);
    EXPECT_FALSE(hours->Covering(At(1, 59)).has_value());
    ASSERT_TRUE(hours->Covering(At(2, 0)).has_value());
    EXPECT_EQ(hours->Describe(*hours->Covering(At(3, 59))), "02:00-04:00 UTC");
    EXPECT_FALSE(hours->Covering(At(4, 0)).has_value()) << "a window's end minute is outside it";
    EXPECT_EQ(hours->Covering(At(23, 0))->Describe(), "22:30-01:15") << "a window may run past midnight";
    EXPECT_EQ(hours->Covering(At(0, 30))->Describe(), "22:30-01:15");
    EXPECT_FALSE(hours->Covering(At(1, 15)).has_value());

    std::optional<ProtectedHours> const york = ProtectedHours::Parse("02:00-04:00", "America/New_York", error);
    ASSERT_TRUE(york.has_value()) << error;
    EXPECT_FALSE(york->Covering(At(3, 0)).has_value()) << "03:00 UTC is 23:00 the day before in New York";
    EXPECT_TRUE(york->Covering(At(7, 0)).has_value()) << "07:00 UTC is 03:00 in New York in October";

    std::optional<ProtectedHours> const none = ProtectedHours::Parse("", "", error);
    ASSERT_TRUE(none.has_value()) << error;
    EXPECT_TRUE(none->Empty());
    EXPECT_EQ(none->Zone(), "UTC");

    for (std::string_view const bad : { "2-4", "24:00-01:00", "02:60-03:00", "02:00", "02:00-02:00", "02:00-04:00,", "02:00-04:00 x" })
    {
        error.clear();
        EXPECT_FALSE(ProtectedHours::Parse(bad, "UTC", error).has_value()) << bad;
        EXPECT_NE(error.find("Power.ProtectedHours"), std::string::npos) << error;
    }
    error.clear();
    EXPECT_FALSE(ProtectedHours::Parse("02:00-04:00", "Nowhere/Atlantis", error).has_value());
    EXPECT_NE(error.find("Nowhere/Atlantis"), std::string::npos) << error;
}

TEST(PowerOperationsTest, ARestartWhileARestoreHoldsTheAppIsRefusedNamingTheRestore)
{
    StackRig rig({ { "login", "loginserver", Serving("loginserver"), {} } });
    std::mutex mutex;
    std::vector<std::string> states;
    ASSERT_TRUE(rig.Open([&mutex, &states](AppSnapshot const& snapshot)
    {
        std::lock_guard<std::mutex> const lock(mutex);
        states.push_back(nlohmann::json::parse(Supervisor::StatusData(snapshot))["state"].get<std::string>());
    }));
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return Is(AppState::Running, app); }));

    PowerBegun held;
    std::shared_ptr<OperationLease> restore = rig.Instance().Hold("login", AppState::Restoring, "backup restore 7", held);
    ASSERT_NE(restore, nullptr) << held.Message;
    restore->Progress("swapping the restored tables in");
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return ManagedApp::Reported(app) == AppState::Restoring; }));

    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    rig.Instance().Register(router, [] { return AdminStatusSnapshot{}; });
    AdminResponse const refused = router.Dispatch(Request("POST", "/api/apps/login/power", R"({"action":"restart"})"));
    ASSERT_EQ(refused.Status, 409) << refused.Body;
    nlohmann::json const body = nlohmann::json::parse(refused.Body);
    EXPECT_EQ(body["error"], "protected");
    EXPECT_NE(body["message"].get<std::string>().find("restoring for backup restore 7"), std::string::npos) << body["message"];
    EXPECT_EQ(body["holder"]["action"], "restoring");
    EXPECT_EQ(body["holder"]["operation"], restore->Id());
    EXPECT_EQ(body["holder"]["by"], "backup restore 7");
    EXPECT_GT(body["holder"]["started_epoch_ms"].get<int64>(), 0);
    PowerAsk stack;
    stack.Target.Kind = PowerTargetKind::Stack;
    stack.Action = PowerAction::Restart;
    EXPECT_EQ(rig.Instance().Begin(stack).Code, "protected") << "a stack restart is refused while one of its apps is held";
    EXPECT_EQ(rig.Instance().Power("login", PowerAction::Kill, 0).Code, "protected");

    AdminResponse const shown = router.Dispatch(Request("GET", "/api/apps/login"));
    ASSERT_EQ(shown.Status, 200) << shown.Body;
    nlohmann::json const app = nlohmann::json::parse(shown.Body);
    EXPECT_EQ(app["state"], "restoring");
    EXPECT_EQ(app["process_state"], "running");
    EXPECT_EQ(app["held"]["progress"], "swapping the restored tables in");
    EXPECT_EQ(router.Dispatch(Request("GET", "/api/apps/login/output/current")).Status, 200) << "the console stays readable while the app is held";
    EXPECT_NE(router.Dispatch(Request("POST", "/api/apps/login/api/command", R"({"command":"ping"})")).Status, 409) << "and a command is not refused for the hold";

    restore.reset();
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return ManagedApp::Reported(app) == AppState::Running; }));
    AdminResponse const restarted = router.Dispatch(Request("POST", "/api/apps/login/power", R"({"action":"restart"})"));
    EXPECT_EQ(restarted.Status, 202) << restarted.Body;
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return app.Restarts == 1 && Is(AppState::Running, app); }));
    std::lock_guard<std::mutex> const lock(mutex);
    EXPECT_NE(std::find(states.begin(), states.end(), "restoring"), states.end()) << "the hold reached the status stream";
}

TEST(PowerOperationsTest, ARestartInsideTheProtectedHoursIsRefusedUnlessAnOwnerOverridesItWithAReason)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    ConfigMgr panelConfig;
    ASSERT_TRUE(panelConfig.LoadInitial(directory.Write("panel.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
    Panel panel(harness.GetLog(), directory.Path() / "data", directory.Path());
    std::string error;
    ASSERT_TRUE(panel.Start(panelConfig, error)) << error;

    StackRig rig({ { "login", "loginserver", Serving("loginserver"), {} } });
    Collected collected;
    PowerObservers observers = collected.Observers();
    auto const finished = observers.Finished;
    observers.Finished = [&panel, finished](PowerReport const& report)
    {
        finished(report);
        AuditEvent event;
        event.EventId = PanelAudit::NewEventId();
        event.Name = "app:power.result";
        event.ActorName = report.Ask.By;
        event.Reason = report.Ask.Reason;
        event.Properties = Supervisor::PowerAcceptedData(report);
        std::string failure;
        AuditEvent const& recorded = event;
        EXPECT_TRUE(panel.Record(recorded, {}, failure)) << failure;
    };
    ASSERT_TRUE(rig.Open({}, observers));
    rig.Instance().SetClock([] { return At(3, 0); });
    rig.Instance().SetProtectedHours("02:00-04:00", "UTC");
    rig.Instance().SetAuditRecorder([&panel](AdminRequest const& request, std::string_view app, std::string_view action, std::function<AdminResponse()> operation)
    {
        return panel.AuditRequest(request, app, action, std::move(operation));
    });
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return Is(AppState::Running, app); }));

    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter admin(auth);
    rig.Instance().Register(admin, [] { return AdminStatusSnapshot{}; });
    AdminRouter operators(auth);
    operators.SetPermissionCheck([](AdminRequest const&, std::string_view) { return PermissionVerdict::Allowed; });
    rig.Instance().Register(operators, [] { return AdminStatusSnapshot{}; });

    AdminRequest asOperator = Request("POST", "/api/apps/login/power", R"({"action":"restart"})");
    asOperator.Principal = "user:2";
    AdminResponse const refused = operators.Dispatch(asOperator);
    ASSERT_EQ(refused.Status, 409) << refused.Body;
    nlohmann::json const body = nlohmann::json::parse(refused.Body);
    EXPECT_EQ(body["error"], "protected_hours");
    EXPECT_EQ(body["window"], "02:00-04:00 UTC");
    EXPECT_NE(body["message"].get<std::string>().find("02:00-04:00 UTC"), std::string::npos) << body["message"];
    EXPECT_EQ(rig.Instance().Power("login", PowerAction::Stop, 0).Status, 202) << "a stop is not a restart";
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return Is(AppState::Offline, app); }));
    ASSERT_EQ(rig.Instance().Power("login", PowerAction::Start, 0).Status, 202) << "nor is a start";
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return Is(AppState::Running, app); }));

    asOperator.Body = R"({"action":"restart","override":true})";
    EXPECT_EQ(operators.Dispatch(asOperator).Status, 422) << "an override needs a reason";
    asOperator.Body = R"({"action":"restart","override":true,"reason":"hotfix for the login crash"})";
    AdminResponse const notOwner = operators.Dispatch(asOperator);
    ASSERT_EQ(notOwner.Status, 403) << notOwner.Body;
    EXPECT_EQ(nlohmann::json::parse(notOwner.Body)["error"], "owner_only");

    PowerAsk scheduled;
    scheduled.Target = PowerTarget{ PowerTargetKind::App, "login" };
    scheduled.Action = PowerAction::Restart;
    scheduled.Origin = PowerOrigin::Schedule;
    scheduled.Override = true;
    scheduled.Reason = "nightly";
    PowerBegun const schedule = rig.Instance().Begin(scheduled);
    EXPECT_FALSE(schedule.Accepted);
    EXPECT_EQ(schedule.Code, "protected_hours");
    EXPECT_EQ(schedule.Window, "02:00-04:00 UTC");
    EXPECT_NE(schedule.Message.find("scheduled restart"), std::string::npos) << schedule.Message;

    rig.Owner = true;
    AdminResponse const overridden = operators.Dispatch(asOperator);
    ASSERT_EQ(overridden.Status, 202) << overridden.Body;
    nlohmann::json const accepted = nlohmann::json::parse(overridden.Body);
    EXPECT_EQ(accepted["window"], "02:00-04:00 UTC");
    std::optional<PowerReport> const result = collected.ResultOf(accepted["operation"].get<std::string>());
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->Succeeded) << result->Message;
    EXPECT_EQ(result->OverriddenWindow, "02:00-04:00 UTC");

    std::optional<PanelStore::Statement> rows = panel.Store().Prepare(
        "SELECT name, result, reason, properties FROM audit_event WHERE name LIKE 'app:power.%' ORDER BY id", error);
    ASSERT_TRUE(rows.has_value()) << error;
    std::vector<std::string> seen;
    bool overrideKept = false;
    while (rows->Step(error))
    {
        seen.push_back(fmt::format("{} {}", rows->Text(0), rows->Text(1)));
        if (rows->Text(0) == "app:power.result" && rows->Text(2) == "hotfix for the login crash")
            overrideKept = nlohmann::json::parse(rows->Text(3))["window"] == "02:00-04:00 UTC";
    }
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_NE(std::find(seen.begin(), seen.end(), "app:power.restart refused"), seen.end()) << fmt::format("{}", fmt::join(seen, ", "));
    EXPECT_NE(std::find(seen.begin(), seen.end(), "app:power.restart succeeded"), seen.end());
    EXPECT_TRUE(overrideKept) << "the override, its reason and the window it passed are on the result's audit row";
}

TEST(PowerOperationsTest, AKillDuringAStopThatNeverFinishesEndsTheAppAsARequestedExit)
{
    StackRig rig({ { "game", "gameserver", { "ignore-term", "echo", ReadyLine("gameserver"), "sleep", "600000" }, "App.game.StopTimeout = 600\n" } });
    Collected collected;
    ASSERT_TRUE(rig.Open({}, collected.Observers()));
    ASSERT_TRUE(rig.WaitFor("game", [](AppSnapshot const& app) { return Is(AppState::Running, app); }));

    PowerAsk stop;
    stop.Target = PowerTarget{ PowerTargetKind::App, "game" };
    stop.Action = PowerAction::Stop;
    PowerBegun const stopping = rig.Instance().Begin(stop);
    ASSERT_TRUE(stopping.Accepted) << stopping.Message;
    ASSERT_TRUE(rig.WaitFor("game", [](AppSnapshot const& app) { return Is(AppState::Stopping, app); }));
    std::this_thread::sleep_for(500ms);
    ASSERT_TRUE(Is(AppState::Stopping, rig.App("game"))) << "the app ignores its shutdown line and its interrupt";

    PowerAsk restart = stop;
    restart.Action = PowerAction::Restart;
    PowerBegun const locked = rig.Instance().Begin(restart);
    EXPECT_EQ(locked.Code, "locked");
    ASSERT_TRUE(locked.Holder.has_value());
    EXPECT_EQ(locked.Holder->Id, stopping.Operation) << "a refusal names the operation holding the app";

    PowerAsk kill = stop;
    kill.Action = PowerAction::Kill;
    PowerBegun const killed = rig.Instance().Begin(kill);
    ASSERT_TRUE(killed.Accepted) << killed.Message;
    ASSERT_TRUE(rig.WaitFor("game", [](AppSnapshot const& app) { return Is(AppState::Offline, app); }));
    AppSnapshot const down = rig.App("game");
    ASSERT_FALSE(down.Exits.empty());
    EXPECT_TRUE(down.Exits.back().Requested);
    EXPECT_EQ(down.Crashes, 0u);
    std::optional<PowerReport> const result = collected.ResultOf(killed.Operation);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->Succeeded) << result->Message;
    std::optional<PowerReport> const stopResult = collected.ResultOf(stopping.Operation);
    ASSERT_TRUE(stopResult.has_value());
    EXPECT_TRUE(rig.Instance().Power("game", PowerAction::Start, 0).Accepted) << "both operations released the app";
}

TEST(PowerOperationsTest, RestartingTheStackStopsGameserversFirstAndStartsTheLoginserverFirst)
{
    StackRig rig({ { "game", "gameserver", Serving("gameserver"), {} }, { "login", "loginserver", Serving("loginserver"), {} } });
    std::mutex mutex;
    std::vector<std::string> order;
    Collected collected;
    ASSERT_TRUE(rig.Open([&mutex, &order](AppSnapshot const& snapshot)
    {
        std::lock_guard<std::mutex> const lock(mutex);
        order.push_back(fmt::format("{} {}", snapshot.Name, ManagedApp::StateName(snapshot.State)));
    }, collected.Observers()));
    ASSERT_TRUE(rig.WaitFor("game", [](AppSnapshot const& app) { return Is(AppState::Running, app); }));
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return Is(AppState::Running, app); }));
    {
        std::lock_guard<std::mutex> const lock(mutex);
        order.clear();
    }

    PowerAsk ask;
    ask.Target.Kind = PowerTargetKind::Stack;
    ask.Action = PowerAction::Restart;
    PowerBegun const begun = rig.Instance().Begin(ask);
    ASSERT_TRUE(begun.Accepted) << begun.Message;
    PowerAsk other;
    other.Target = PowerTarget{ PowerTargetKind::App, "login" };
    other.Action = PowerAction::Stop;
    EXPECT_EQ(rig.Instance().Begin(other).Code, "locked") << "the stack lock holds every app";
    std::optional<PowerReport> const result = collected.ResultOf(begun.Operation);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->Succeeded) << result->Message;

    std::lock_guard<std::mutex> const lock(mutex);
    auto const at = [&order](std::string const& entry, std::size_t from = 0)
    {
        return std::distance(order.begin(), std::find(order.begin() + static_cast<std::ptrdiff_t>(from), order.end(), entry));
    };
    std::string const joined = fmt::format("{}", fmt::join(order, ", "));
    auto const gameDown = at("game offline");
    auto const loginStopping = at("login stopping");
    auto const loginDown = at("login offline");
    ASSERT_LT(loginDown, static_cast<std::ptrdiff_t>(order.size())) << joined;
    EXPECT_LT(gameDown, loginStopping) << "the gameserver is down before the loginserver begins to stop: " << joined;
    auto const loginStarting = at("login starting", static_cast<std::size_t>(loginDown));
    auto const loginUp = at("login running", static_cast<std::size_t>(loginDown));
    auto const gameStarting = at("game starting", static_cast<std::size_t>(loginDown));
    ASSERT_LT(gameStarting, static_cast<std::ptrdiff_t>(order.size())) << joined;
    EXPECT_LT(loginStarting, gameStarting) << joined;
    EXPECT_LT(loginUp, gameStarting) << "the loginserver is ready before the gameserver starts: " << joined;
}

TEST(PowerOperationsTest, APowerRequestAnswersWithAnOperationWhoseProgressAndResultArriveAndAreAudited)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    ConfigMgr panelConfig;
    ASSERT_TRUE(panelConfig.LoadInitial(directory.Write("panel.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
    Panel panel(harness.GetLog(), directory.Path() / "data", directory.Path());
    std::string error;
    ASSERT_TRUE(panel.Start(panelConfig, error)) << error;

    StackRig rig({ { "game", "gameserver", Serving("gameserver"), "App.game.Autostart = 0\n" },
        { "login", "loginserver", Serving("loginserver"), "App.login.Autostart = 0\n" } });
    Collected collected;
    ASSERT_TRUE(rig.Open({}, collected.Observers()));
    rig.Instance().SetAuditRecorder([&panel](AdminRequest const& request, std::string_view app, std::string_view action, std::function<AdminResponse()> operation)
    {
        return panel.AuditRequest(request, app, action, std::move(operation));
    });
    ASSERT_TRUE(rig.WaitFor("game", [](AppSnapshot const& app) { return Is(AppState::Offline, app); }));

    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    std::vector<std::string> asked;
    router.SetPermissionCheck([&asked](AdminRequest const& request, std::string_view permission)
    {
        asked.push_back(fmt::format("{} {}", request.Path, permission));
        return PermissionVerdict::Allowed;
    });
    rig.Instance().RegisterPanelPower(router);

    AdminResponse const bad = router.Dispatch(Request("POST", "/api/panel/power", R"({"target":{"kind":"planet"},"action":"start","colour":1})"));
    ASSERT_EQ(bad.Status, 422) << bad.Body;
    nlohmann::json const fields = nlohmann::json::parse(bad.Body)["fields"];
    EXPECT_TRUE(fields.contains("target"));
    EXPECT_TRUE(fields.contains("colour"));
    EXPECT_EQ(router.Dispatch(Request("POST", "/api/panel/power", R"({"target":{"kind":"stack","name":"x"},"action":"start"})")).Status, 422);
    EXPECT_EQ(router.Dispatch(Request("POST", "/api/panel/power", R"({"target":{"kind":"realm","name":"Nowhere"},"action":"start"})")).Status, 404);

    AdminResponse const started = router.Dispatch(Request("POST", "/api/panel/power", R"({"target":{"kind":"realm","name":"Ambrose"},"action":"start"})"));
    ASSERT_EQ(started.Status, 202) << started.Body;
    nlohmann::json const answer = nlohmann::json::parse(started.Body);
    std::string const operation = answer["operation"].get<std::string>();
    EXPECT_TRUE(operation.starts_with("op-")) << operation;
    EXPECT_EQ(answer["apps"], nlohmann::json::array({ "game" })) << "a realm's target is its gameservers";
    EXPECT_NE(std::find(asked.begin(), asked.end(), "/api/apps/game/power power.start"), asked.end()) << "the permission is asked at each app's scope";

    std::optional<PowerReport> const result = collected.ResultOf(operation);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->Succeeded) << result->Message;
    std::vector<PowerStep> const steps = collected.StepsOf(operation);
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0].Outcome, "begun");
    EXPECT_EQ(steps[1].Outcome, "done");
    EXPECT_EQ(steps[1].App, "game");
    nlohmann::json const progress = nlohmann::json::parse(Supervisor::PowerProgressData(steps[1]));
    EXPECT_EQ(progress["step"], "start");
    nlohmann::json const final = nlohmann::json::parse(Supervisor::PowerResultData(*result));
    EXPECT_EQ(final["outcome"], "succeeded");
    EXPECT_EQ(final["target"]["kind"], "realm");
    EXPECT_EQ(final["request"], "0123456789abcdef");
    std::string reason;
    EXPECT_TRUE(PanelEventCatalog::Validate(PanelEventDirection::FromServer, "power.progress", progress, reason)) << reason;
    EXPECT_TRUE(PanelEventCatalog::Validate(PanelEventDirection::FromServer, "power.result", final, reason)) << reason;
    EXPECT_TRUE(PanelEventCatalog::Validate(PanelEventDirection::FromServer, "power.accepted", nlohmann::json::parse(Supervisor::PowerAcceptedData(*result)), reason)) << reason;

    AdminResponse const failing = router.Dispatch(Request("POST", "/api/panel/power", R"({"target":{"kind":"app","name":"game"},"action":"start"})"));
    ASSERT_EQ(failing.Status, 409) << failing.Body;
    EXPECT_EQ(nlohmann::json::parse(failing.Body)["error"], "already_running") << "a request that cannot apply is refused at once";

    std::optional<PanelStore::Statement> rows = panel.Store().Prepare("SELECT result FROM audit_event WHERE name = 'app:power.start' ORDER BY id", error);
    ASSERT_TRUE(rows.has_value()) << error;
    std::vector<std::string> results;
    while (rows->Step(error))
        results.push_back(rows->Text(0));
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_NE(std::find(results.begin(), results.end(), "succeeded"), results.end()) << fmt::format("{}", fmt::join(results, ", "));
    EXPECT_NE(std::find(results.begin(), results.end(), "refused"), results.end());
}

TEST(PowerOperationsTest, ADisabledAppStopsAndRefusesAStartNamingTheReasonUntilItIsEnabled)
{
    StackRig rig({ { "login", "loginserver", Serving("loginserver"), {} } });
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return Is(AppState::Running, app); }));
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    rig.Instance().Register(router, [] { return AdminStatusSnapshot{}; });

    EXPECT_EQ(router.Dispatch(Request("POST", "/api/apps/login/disable", "{}")).Status, 422) << "disabling needs a reason";
    AdminResponse const disabled = router.Dispatch(Request("POST", "/api/apps/login/disable", R"({"reason":"database host moving tonight"})"));
    ASSERT_EQ(disabled.Status, 200) << disabled.Body;
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return ManagedApp::Reported(app) == AppState::Disabled; }));
    AppSnapshot const down = rig.App("login");
    EXPECT_FALSE(down.ProcessId.has_value());
    EXPECT_TRUE(down.Exits.back().Requested);
    ASSERT_TRUE(down.Disabled.has_value());
    EXPECT_EQ(down.Disabled->Reason, "database host moving tonight");
    EXPECT_EQ(down.Disabled->By, "token");

    AdminResponse const start = router.Dispatch(Request("POST", "/api/apps/login/power", R"({"action":"start"})"));
    ASSERT_EQ(start.Status, 409) << start.Body;
    nlohmann::json const refusal = nlohmann::json::parse(start.Body);
    EXPECT_EQ(refusal["error"], "disabled");
    EXPECT_NE(refusal["message"].get<std::string>().find("database host moving tonight"), std::string::npos) << refusal["message"];
    nlohmann::json const shown = nlohmann::json::parse(router.Dispatch(Request("GET", "/api/apps/login")).Body);
    EXPECT_EQ(shown["state"], "disabled");
    EXPECT_EQ(shown["disabled"]["by"], "token");

    rig.Instance().Shutdown();
    ASSERT_TRUE(rig.Open()) << "a new supervisor reads the disable back";
    ASSERT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return app.Watching; }));
    std::this_thread::sleep_for(300ms);
    EXPECT_FALSE(rig.App("login").ProcessId.has_value()) << "a disabled app is not started with the supervisor";
    EXPECT_TRUE(rig.App("login").Disabled.has_value());

    AdminRouter again(auth);
    rig.Instance().Register(again, [] { return AdminStatusSnapshot{}; });
    ASSERT_EQ(again.Dispatch(Request("POST", "/api/apps/login/enable", "{}")).Status, 200);
    EXPECT_EQ(again.Dispatch(Request("POST", "/api/apps/login/enable", "{}")).Status, 409);
    AdminResponse const allowed = again.Dispatch(Request("POST", "/api/apps/login/power", R"({"action":"start"})"));
    EXPECT_EQ(allowed.Status, 202) << allowed.Body;
    EXPECT_TRUE(rig.WaitFor("login", [](AppSnapshot const& app) { return Is(AppState::Running, app); }));
}
