/*
 * Project Ambrose by Imjustchico
 * Runs the supervisor over the helper program as its app: it starts it and calls it ready on its ready line, stops it with a shutdown line on its input, hands every state the app passes through to the status observer once and in order with when each began and the data the panel's status event carries, restarts it, counts one crash and starts it again when something else ends it, leaves a start that exits before it is ready alone, ends a start that never reports ready, waits past its timeout for a start step a stand-in admin API reports until the app is ready and ends one that runs past the time it asked for, takes a running app back after the supervisor is replaced and refuses the same process id once its start time no longer matches, and answers its routes: the app list carrying the supervisor and every app, the supervisor's own state, power requests refused field by field and by state, the captured output, and a relay that says why an app with its admin API off cannot be reached, with a request judged by the listener it came in on, so the admin token on the supervisor's own listener reaches the relay and power while the panel's check still refuses a caller it does not grant.
 */

#include "AdminAuth.h"
#include "AdminRouter.h"
#include "AdminServer.h"
#include "AdminStatus.h"
#include "ListenerSettings.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "Supervisor.h"

#include <asio.hpp>
#include <fmt/format.h>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    constexpr char const* Token = "0123456789abcdef0123456789abcdef";
    constexpr char const* ReadyLine = "2026-09-22_00:00:00.000 INFO  [server.child_process_helper] child_process_helper ready";

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

    class Rig
    {
    public:
        explicit Rig(std::vector<std::string> const& script, std::string const& extra = {}, std::string const& appSettings = {})
        {
            std::string lines;
            for (std::string const& line : script)
                lines += appSettings.empty() ? line + "\n" : "#" + line + "\n";
            if (!appSettings.empty())
                lines += appSettings;
            std::filesystem::path const scriptFile = _directory.Write("helper.conf", lines);
            std::string const settings = fmt::format(
                "Supervisor.Apps = helper\nApp.helper.Program = \"{}\"\nApp.helper.Config = \"{}\"\nSupervisor.StateFile = \"{}\"\nSupervisor.OutputDir = \"{}\"\n{}",
                Slashes(HelperPath()), Slashes(scriptFile), Slashes(StateFile()), Slashes(_directory.Path() / "output"), extra);
            std::filesystem::path const file = _directory.Write("supervisor.conf", settings);
            EXPECT_TRUE(_config.LoadInitial(file).Succeeded());
        }

        ~Rig()
        {
            std::optional<int64> const process = _instance ? App().ProcessId : std::nullopt;
            Close();
            if (process)
                EndFromOutside(*process);
        }

        bool Open(AppStatusObserver observer = {})
        {
            _instance = std::make_unique<Supervisor>(_harness.GetLog(), HelperBreak());
            _instance->SetStatusObserver(std::move(observer));
            std::vector<std::string> problems;
            std::string error;
            SupervisorSettings const settings = SupervisorSettings::Load(_config, _directory.Path() / "data", HelperPath().parent_path(), _directory.Path(), problems);
            bool const started = _instance->Start(_config, settings, true, problems, error);
            EXPECT_TRUE(error.empty()) << error;
            return started;
        }

        void Close()
        {
            if (!_instance)
                return;
            _instance->Shutdown();
            _instance.reset();
        }

        Supervisor& Instance() { return *_instance; }
        std::filesystem::path StateFile() const { return _directory.Path() / "state.json"; }
        LogTestDirectory& Directory() { return _directory; }

        AppSnapshot App() const
        {
            std::vector<AppSnapshot> const apps = _instance->Snapshots();
            return apps.empty() ? AppSnapshot{} : apps.front();
        }

        bool WaitFor(std::function<bool(AppSnapshot const&)> const& ready, std::chrono::milliseconds timeout = 30s) const
        {
            std::chrono::steady_clock::time_point const until = std::chrono::steady_clock::now() + timeout;
            while (std::chrono::steady_clock::now() < until)
            {
                if (ready(App()))
                    return true;
                std::this_thread::sleep_for(20ms);
            }
            return false;
        }

        bool Said(std::string_view text, OutputRun run = OutputRun::Current) const
        {
            for (OutputLine const& line : _instance->Output("helper", run, 0))
                if (line.Text.find(text) != std::string::npos)
                    return true;
            return false;
        }

    private:
        LogTestHarness _harness;
        LogTestDirectory _directory;
        ConfigMgr _config;
        std::unique_ptr<Supervisor> _instance;
    };

    class RepeatedResponseServer
    {
    public:
        explicit RepeatedResponseServer(int status)
            : _acceptor(_context, { asio::ip::make_address("127.0.0.1"), 0 })
        {
            _acceptor.non_blocking(true);
            _worker = std::thread([this, status]
            {
                while (!_stopping.load())
                {
                    asio::ip::tcp::socket socket(_context);
                    asio::error_code error;
                    _acceptor.accept(socket, error);
                    if (!error)
                    {
                        std::string const reason = status == 401 ? "Unauthorized" : "Forbidden";
                        std::string const response = fmt::format(
                            "HTTP/1.1 {} {}\r\nContent-Type: application/json\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{{}}",
                            status, reason);
                        asio::write(socket, asio::buffer(response), error);
                        continue;
                    }
                    if (error != asio::error::would_block && error != asio::error::try_again)
                        return;
                    std::this_thread::sleep_for(5ms);
                }
            });
        }

        ~RepeatedResponseServer()
        {
            _stopping = true;
            if (_worker.joinable())
                _worker.join();
        }

        uint16 Port() const
        {
            return _acceptor.local_endpoint().port();
        }

    private:
        asio::io_context _context;
        asio::ip::tcp::acceptor _acceptor;
        std::thread _worker;
        std::atomic_bool _stopping = false;
    };

    std::vector<std::string> ServerScript()
    {
        return { "echo", ReadyLine, "wait-for-stop" };
    }
}

TEST(SupervisorTest, StartsAnAppOnItsReadyLineAndStopsItThroughItsInput)
{
    Rig rig(ServerScript());
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    AppSnapshot const running = rig.App();
    EXPECT_TRUE(running.ProcessId.has_value());
    EXPECT_FALSE(running.Adopted);
    EXPECT_TRUE(running.WantRunning);
    EXPECT_FALSE(running.AdminEnabled);
    EXPECT_NE(running.AdminProblem.find("admin API"), std::string::npos) << running.AdminProblem;
    EXPECT_TRUE(rig.Said("is ready: it printed its ready line"));

    PowerResult const stop = rig.Instance().Power("helper", PowerAction::Stop, 0);
    EXPECT_TRUE(stop.Accepted) << stop.Message;
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Offline; }));
    AppSnapshot const stopped = rig.App();
    EXPECT_FALSE(stopped.WantRunning);
    EXPECT_FALSE(stopped.ProcessId.has_value());
    ASSERT_FALSE(stopped.Exits.empty());
    EXPECT_TRUE(stopped.Exits.back().Requested);
    EXPECT_EQ(stopped.Exits.back().Code, std::optional<int64>(0));
    EXPECT_EQ(stopped.Crashes, 0u);
    EXPECT_TRUE(rig.Said("Stopping helper with a shutdown line on its input"));
    EXPECT_TRUE(rig.Said("stopped by shutdown"));
}

TEST(SupervisorTest, EveryStateTheAppPassesThroughReachesTheStatusObserverOnceAndInOrder)
{
    std::mutex mutex;
    std::vector<AppSnapshot> seen;
    AppStatusObserver const observer = [&mutex, &seen](AppSnapshot const& snapshot)
    {
        std::lock_guard const lock(mutex);
        seen.push_back(snapshot);
    };
    Rig rig(ServerScript());
    ASSERT_TRUE(rig.Open(observer));
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    EXPECT_TRUE(rig.Instance().Power("helper", PowerAction::Stop, 0).Accepted);
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Offline; }));
    auto const until = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < until)
    {
        {
            std::lock_guard const lock(mutex);
            if (!seen.empty() && seen.back().State == AppState::Offline)
                break;
        }
        std::this_thread::sleep_for(20ms);
    }
    rig.Close();

    std::lock_guard const lock(mutex);
    std::vector<std::string> states;
    for (AppSnapshot const& snapshot : seen)
        states.emplace_back(ManagedApp::StateName(snapshot.State));
    EXPECT_EQ(states, (std::vector<std::string>{ "starting", "running", "stopping", "offline" }));
    for (std::size_t index = 1; index < seen.size(); ++index)
    {
        EXPECT_GE(seen[index].StateSinceEpochMs, seen[index - 1].StateSinceEpochMs) << "each state begins no earlier than the one before it";
    }
    if (seen.size() == 4)
    {
        EXPECT_TRUE(seen[0].ProcessId.has_value()) << "a start is handed on with the process it started";
        EXPECT_EQ(seen[1].ProcessId, seen[0].ProcessId);
        EXPECT_GT(seen[0].StateSinceEpochMs, 0);
        nlohmann::json const offline = nlohmann::json::parse(Supervisor::StatusData(seen[3]));
        EXPECT_EQ(offline["app"], "helper");
        EXPECT_EQ(offline["state"], "offline");
        EXPECT_TRUE(offline["pid"].is_null());
        EXPECT_EQ(offline["exit_code"], 0) << "an app that is down carries the code it exited with";
        EXPECT_EQ(offline["crashes"], 0);
        EXPECT_TRUE(offline["next_restart"].is_null());
        nlohmann::json const running = nlohmann::json::parse(Supervisor::StatusData(seen[1]));
        EXPECT_EQ(running["pid"], *seen[1].ProcessId);
        EXPECT_TRUE(running["exit_code"].is_null());
    }
}

TEST(SupervisorTest, ARestartStartsTheAppAgainAsANewProcess)
{
    Rig rig(ServerScript());
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    int64 const first = *rig.App().ProcessId;
    EXPECT_TRUE(rig.Instance().Power("helper", PowerAction::Restart, 0).Accepted);
    ASSERT_TRUE(rig.WaitFor([first](AppSnapshot const& app) { return app.State == AppState::Running && app.ProcessId && *app.ProcessId != first; }));
    AppSnapshot const restarted = rig.App();
    EXPECT_EQ(restarted.Restarts, 1u);
    EXPECT_EQ(restarted.Crashes, 0u);
    EXPECT_TRUE(restarted.WantRunning);
    EXPECT_TRUE(rig.Said("is ready", OutputRun::Current));
    EXPECT_TRUE(rig.Said("exited with code 0 as asked", OutputRun::Previous));
}

TEST(SupervisorTest, AnAppEndedFromOutsideCountsOneCrashAndStartsAgain)
{
    Rig rig(ServerScript());
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    int64 const first = *rig.App().ProcessId;
    EndFromOutside(first);
    ASSERT_TRUE(rig.WaitFor([first](AppSnapshot const& app) { return app.State == AppState::Running && app.ProcessId && *app.ProcessId != first; }));
    AppSnapshot const restarted = rig.App();
    EXPECT_EQ(restarted.Crashes, 1u);
    EXPECT_EQ(restarted.Restarts, 1u);
    EXPECT_EQ(restarted.FailedStarts, 0u);
    ASSERT_GE(restarted.Exits.size(), 1u);
    AppExit const& crash = restarted.Exits.front();
    EXPECT_FALSE(crash.Requested);
    EXPECT_EQ(crash.During, AppState::Running);
    EXPECT_TRUE(rig.Said("without being asked", OutputRun::Previous));
}

TEST(SupervisorTest, AStartThatEndsBeforeItIsReadyIsRecordedAndNotStartedAgain)
{
    Rig rig({ "err", "nothing works", "exit", "3" });
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Crashed; }));
    AppSnapshot const crashed = rig.App();
    EXPECT_EQ(crashed.FailedStarts, 1u);
    EXPECT_EQ(crashed.Crashes, 0u);
    ASSERT_FALSE(crashed.Exits.empty());
    EXPECT_EQ(crashed.Exits.back().Code, std::optional<int64>(3));
    EXPECT_EQ(crashed.Exits.back().During, AppState::Starting);
    EXPECT_NE(crashed.Message.find("before it was ready"), std::string::npos) << crashed.Message;
    EXPECT_NE(crashed.Message.find("exit code 3"), std::string::npos) << crashed.Message;
    EXPECT_NE(crashed.Message.find("nothing works"), std::string::npos) << "the panel shows why it stopped, not only that it did: " << crashed.Message;
    EXPECT_TRUE(rig.Said("nothing works"));
    std::this_thread::sleep_for(2s);
    EXPECT_EQ(rig.App().State, AppState::Crashed);
    EXPECT_EQ(rig.App().Restarts, 0u);
}

TEST(SupervisorTest, AStartThatEndsBeforeItIsReadyNamesTheFirstErrorItLogged)
{
    Rig rig({ "echo", "2026-09-30_19:10:27.884 ERROR [server.loginserver] it has no type dump: TypeDumpPath is not set", "echo",
        "2026-09-30_19:10:27.886 ERROR [server.loginserver] loginserver failed to start", "exit", "1" });
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Crashed; }));
    std::string const message = rig.App().Message;
    EXPECT_NE(message.find("exit code 1: it has no type dump: TypeDumpPath is not set"), std::string::npos)
        << "the first error is the cause and the last only says it failed: " << message;
    EXPECT_EQ(message.find("[server.loginserver]"), std::string::npos) << "the time, level and category are left out: " << message;
}

TEST(SupervisorTest, AStartThatNeverReportsReadyIsEndedAtItsTimeout)
{
    Rig rig({ "sleep", "60000" }, "App.helper.StartTimeout = 1\n");
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Crashed; }));
    EXPECT_NE(rig.App().Message.find("did not become ready within 1"), std::string::npos) << rig.App().Message;
    EXPECT_EQ(rig.App().FailedStarts, 1u);
    EXPECT_TRUE(rig.Said("did not become ready within 1 s"));
}

namespace
{
    class StandInAdmin
    {
    public:
        explicit StandInAdmin(LogTestHarness& harness, LogTestDirectory& directory) : _server(harness.GetLog(), "helper", directory.Path() / "standin")
        {
            _server.SetHealthSource([this]
            {
                std::lock_guard const lock(_mutex);
                return _health;
            });
            ListenerSettings settings;
            settings.Enable = true;
            settings.BindIp = "127.0.0.1";
            settings.Port = 0;
            settings.Token = Token;
            std::string error;
            EXPECT_TRUE(_server.Start(settings, error)) << error;
        }

        ~StandInAdmin()
        {
            _server.Stop();
        }

        void Starting(std::string stage, std::chrono::seconds allowance)
        {
            std::lock_guard const lock(_mutex);
            _health.State = "starting";
            _health.StartStage = std::move(stage);
            _health.StartUntilEpochMs = std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() + allowance).time_since_epoch()).count();
        }

        void Running()
        {
            std::lock_guard const lock(_mutex);
            _health.State = "running";
            _health.StartStage.clear();
        }

        std::vector<std::string> Script() const
        {
            return { "Admin.Enable = 1", "Admin.BindIP = 127.0.0.1", fmt::format("Admin.Port = {}", _server.GetPort()), fmt::format("Admin.Token = {}", Token),
                "Helper.Script = sleep 60000" };
        }

    private:
        std::mutex _mutex;
        AdminHealth _health{ "helper", "", "rev", 0, "starting" };
        AdminServer _server;
    };
}

TEST(SupervisorTest, AStartStepTheAppReportsIsWaitedForUntilTheAppIsReady)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    StandInAdmin admin(harness, directory);
    admin.Starting("extracting zones", 60s);
    Rig rig(admin.Script(), "App.helper.StartTimeout = 1\n");
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.StartStage == "extracting zones"; }));
    std::this_thread::sleep_for(2500ms);
    AppSnapshot const waiting = rig.App();
    EXPECT_EQ(waiting.State, AppState::Starting) << "a one second start timeout is waited past while the app says what it is doing: " << waiting.Message;
    EXPECT_GT(waiting.StartUntilEpochMs, 0);

    admin.Running();
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; })) << rig.App().Message;
    EXPECT_TRUE(rig.App().StartStage.empty());
    EXPECT_EQ(rig.App().FailedStarts, 0u);
}

TEST(SupervisorTest, AStartStepThatRunsPastTheTimeItAskedForEndsTheStart)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    StandInAdmin admin(harness, directory);
    admin.Starting("extracting zones", 3s);
    Rig rig(admin.Script(), "App.helper.StartTimeout = 1\n");
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.StartStage == "extracting zones"; }));
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Crashed; }, 20s));
    EXPECT_NE(rig.App().Message.find("extracting zones ran past the time it asked for"), std::string::npos) << rig.App().Message;
    EXPECT_EQ(rig.App().FailedStarts, 1u);
}

TEST(SupervisorTest, ANewSupervisorTakesARunningAppBackAndRefusesAProcessIdThatIsNotItAnyMore)
{
    Rig rig(ServerScript());
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    int64 const first = *rig.App().ProcessId;
    rig.Close();

    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([first](AppSnapshot const& app) { return app.State == AppState::Running && app.ProcessId == first; }));
    EXPECT_TRUE(rig.App().Adopted);
    EXPECT_TRUE(rig.Said("Took back helper as process"));
    rig.Close();

    std::string state;
    {
        std::ifstream stream(rig.StateFile(), std::ios::binary);
        state.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
    nlohmann::json saved = nlohmann::json::parse(state, nullptr, false);
    ASSERT_TRUE(saved.is_object());
    saved["apps"]["helper"]["process"]["start_time"] = saved["apps"]["helper"]["process"]["start_time"].get<uint64>() + 1;
    std::ofstream(rig.StateFile(), std::ios::binary) << saved.dump();

    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([first](AppSnapshot const& app) { return app.State == AppState::Running && app.ProcessId && *app.ProcessId != first; }));
    EXPECT_FALSE(rig.App().Adopted);
    EXPECT_TRUE(rig.Said("Did not take back process"));
    EndFromOutside(first);
}

TEST(SupervisorTest, PowerRequestsAreRefusedWhenTheyCannotApply)
{
    Rig rig(ServerScript(), "App.helper.Autostart = 0\n");
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Offline; }));
    PowerResult const stop = rig.Instance().Power("helper", PowerAction::Stop, 0);
    EXPECT_FALSE(stop.Accepted);
    EXPECT_EQ(stop.Status, 409);
    EXPECT_EQ(stop.Code, "already_stopped");
    PowerResult const kill = rig.Instance().Power("helper", PowerAction::Kill, 0);
    EXPECT_FALSE(kill.Accepted);
    EXPECT_EQ(kill.Code, "not_running");
    PowerResult const unknown = rig.Instance().Power("nothing", PowerAction::Start, 0);
    EXPECT_FALSE(unknown.Accepted);
    EXPECT_EQ(unknown.Status, 404);

    EXPECT_TRUE(rig.Instance().Power("helper", PowerAction::Start, 0).Accepted);
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    PowerResult const again = rig.Instance().Power("helper", PowerAction::Start, 0);
    EXPECT_FALSE(again.Accepted);
    EXPECT_EQ(again.Code, "already_running");
}

TEST(SupervisorTest, TheRoutesListEveryAppPowerItAndSayWhyARelayCannotReachIt)
{
    Rig rig(ServerScript());
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    AdminStatusSnapshot self;
    self.App.Name = "supervisor";
    self.App.Role = "supervisor";
    self.App.Revision = "rev";
    self.State = "running";
    rig.Instance().Register(router, [self] { return self; });

    AdminResponse const apps = router.Dispatch(Request("GET", "/api/apps"));
    ASSERT_EQ(apps.Status, 200) << apps.Body;
    nlohmann::json const list = nlohmann::json::parse(apps.Body);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0]["name"], "supervisor");
    EXPECT_TRUE(list[0]["supervision"].is_null());
    EXPECT_EQ(list[1]["name"], "helper");
    EXPECT_EQ(list[1]["role"], "child_process_helper");
    EXPECT_EQ(list[1]["supervision"]["state"], "running");

    AdminResponse const supervision = router.Dispatch(Request("GET", "/api/supervisor"));
    ASSERT_EQ(supervision.Status, 200) << supervision.Body;
    nlohmann::json const state = nlohmann::json::parse(supervision.Body);
    EXPECT_EQ(state["schema"], Supervisor::SchemaVersion);
    ASSERT_EQ(state["apps"].size(), 1u);
    EXPECT_EQ(state["apps"][0]["desired"], "running");
    EXPECT_EQ(state["apps"][0]["admin"]["enabled"], false);

    AdminResponse const one = router.Dispatch(Request("GET", "/api/apps/helper"));
    ASSERT_EQ(one.Status, 200) << one.Body;
    EXPECT_EQ(nlohmann::json::parse(one.Body)["name"], "helper");
    EXPECT_EQ(router.Dispatch(Request("GET", "/api/apps/nothing")).Status, 404);
    EXPECT_EQ(router.Dispatch(Request("GET", "/api/apps/helper/nothing")).Status, 404);
    EXPECT_EQ(router.Dispatch(Request("POST", "/api/apps/helper", "{}")).Status, 405);

    AdminResponse const output = router.Dispatch(Request("GET", "/api/apps/helper/output/current"));
    ASSERT_EQ(output.Status, 200) << output.Body;
    nlohmann::json const lines = nlohmann::json::parse(output.Body);
    EXPECT_EQ(lines["run"], "current");
    EXPECT_FALSE(lines["lines"].empty());
    EXPECT_EQ(router.Dispatch(Request("GET", "/api/apps/helper/output/previous")).Status, 200);

    AdminResponse const relay = router.Dispatch(Request("GET", "/api/apps/helper/api/status"));
    EXPECT_EQ(relay.Status, 503);
    EXPECT_EQ(nlohmann::json::parse(relay.Body)["error"], "app_admin_off");
    EXPECT_EQ(router.Dispatch(Request("POST", "/api/apps/helper/api/session", "{}")).Status, 404);

    EXPECT_EQ(router.Dispatch(Request("POST", "/api/apps/helper/power", "[]")).Status, 422);
    AdminResponse const bad = router.Dispatch(Request("POST", "/api/apps/helper/power", "{\"action\":\"explode\",\"force\":1}"));
    ASSERT_EQ(bad.Status, 422) << bad.Body;
    nlohmann::json const fields = nlohmann::json::parse(bad.Body)["fields"];
    EXPECT_TRUE(fields.contains("action"));
    EXPECT_TRUE(fields.contains("force"));
    EXPECT_EQ(router.Dispatch(Request("POST", "/api/apps/helper/power", "{\"action\":\"start\",\"seconds\":5}")).Status, 422);
    EXPECT_EQ(router.Dispatch(Request("POST", "/api/apps/helper/power", "{\"action\":\"start\"}")).Status, 409);

    AdminResponse const stop = router.Dispatch(Request("POST", "/api/apps/helper/power", "{\"action\":\"stop\",\"seconds\":0}"));
    ASSERT_EQ(stop.Status, 202) << stop.Body;
    EXPECT_EQ(nlohmann::json::parse(stop.Body)["accepted"], true);
    EXPECT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Offline; }));
}

TEST(SupervisorTest, EachListenerJudgesTheRequestsThatCameInOnIt)
{
    Rig rig(ServerScript());
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter admin(auth);
    AdminRouter panel(auth);
    panel.SetPermissionCheck([](AdminRequest const&, std::string_view) { return PermissionVerdict::Forbidden; });
    AdminStatusSnapshot self;
    self.App.Name = "supervisor";
    self.App.Role = "supervisor";
    rig.Instance().Register(admin, [self] { return self; });
    rig.Instance().Register(panel, [self] { return self; });

    AdminResponse const relayed = admin.Dispatch(Request("GET", "/api/apps/helper/api/status"));
    EXPECT_EQ(relayed.Status, 503) << "the token on the supervisor's own listener reaches the relay, which then says why the app cannot answer: " << relayed.Body;
    EXPECT_EQ(admin.Dispatch(Request("POST", "/api/apps/helper/power", "{\"action\":\"start\"}")).Status, 409) << "and may ask for power";
    EXPECT_EQ(panel.Dispatch(Request("GET", "/api/apps/helper/api/status")).Status, 403) << "while the panel's own check still refuses its caller";
    EXPECT_EQ(panel.Dispatch(Request("POST", "/api/apps/helper/power", "{\"action\":\"start\"}")).Status, 403);
}

TEST(SupervisorTest, ARestartOnlyGrantCanRestartWithoutRequiringStatusRead)
{
    Rig rig(ServerScript());
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));

    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter panel(auth);
    panel.SetPermissionCheck([](AdminRequest const&, std::string_view permission)
    {
        return permission == "power.restart" ? PermissionVerdict::Allowed : PermissionVerdict::Forbidden;
    });
    rig.Instance().Register(panel, [] { return AdminStatusSnapshot{}; });

    AdminResponse const restarted = panel.Dispatch(Request("POST", "/api/apps/helper/power", "{\"action\":\"restart\"}"));
    ASSERT_EQ(restarted.Status, 202) << restarted.Body;
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.Restarts == 1 && app.State == AppState::Running; }));
    EXPECT_EQ(panel.Dispatch(Request("GET", "/api/apps/helper")).Status, 403);
}

TEST(SupervisorTest, TheRelayRemovesClientCredentialsAndCapsCommandLevels)
{
    std::string const body = Supervisor::PrepareCommandRelayBody(
        R"({"command":"account create player passphrase","level":4,"AdMiN_ToKeN":"app-secret","Authorization":"secret"})", 2);
    nlohmann::json const relayed = nlohmann::json::parse(body);
    EXPECT_EQ(relayed["command"], "account create player passphrase");
    EXPECT_EQ(relayed["level"], 2);
    EXPECT_FALSE(relayed.contains("AdMiN_ToKeN"));
    EXPECT_FALSE(relayed.contains("Authorization"));
    EXPECT_EQ(body.find("app-secret"), std::string::npos);

    nlohmann::json const lower = nlohmann::json::parse(Supervisor::PrepareCommandRelayBody(R"({"command":"ping","level":1})", 2));
    EXPECT_EQ(lower["level"], 1);
}

TEST(SupervisorTest, StoppedAppAndInvalidTokenRelayFailuresAreDistinctAndAudited)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    ConfigMgr panelConfig;
    ASSERT_TRUE(panelConfig.LoadInitial(directory.Write("panel.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
    Panel panel(harness.GetLog(), directory.Path() / "data", directory.Path());
    std::string error;
    ASSERT_TRUE(panel.Start(panelConfig, error)) << error;
    auto const recordThroughPanel = [&panel](Supervisor& supervisor)
    {
        supervisor.SetAuditRecorder([&panel](AdminRequest const& request, std::string_view app, std::string_view action, std::function<AdminResponse()> operation)
        {
            return panel.AuditRequest(request, app, action, std::move(operation));
        });
    };

    std::string const unusedAdmin = fmt::format(
        "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = 1\nAdmin.Token = {}\n", Token);
    Rig stopped(ServerScript(), {}, unusedAdmin);
    ASSERT_TRUE(stopped.Open());
    ASSERT_TRUE(stopped.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    recordThroughPanel(stopped.Instance());
    AdminAuth stoppedAuth(10, 1.0);
    stoppedAuth.SetToken(Token);
    AdminRouter stoppedRouter(stoppedAuth);
    stopped.Instance().Register(stoppedRouter, [] { return AdminStatusSnapshot{}; });
    AdminResponse const stop = stoppedRouter.Dispatch(Request("POST", "/api/apps/helper/power", "{\"action\":\"stop\"}"));
    ASSERT_EQ(stop.Status, 202) << stop.Body;
    ASSERT_TRUE(stopped.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Offline; }));
    AdminResponse const notRunning = stoppedRouter.Dispatch(Request("POST", "/api/apps/helper/api/command", R"({"command":"ping"})"));
    ASSERT_EQ(notRunning.Status, 503) << notRunning.Body;
    EXPECT_EQ(nlohmann::json::parse(notRunning.Body)["error"], "app_not_running");

    RepeatedResponseServer invalidToken(401);
    std::string const invalidAdmin = fmt::format(
        "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = {}\nAdmin.Token = {}\n", invalidToken.Port(), Token);
    Rig invalid(ServerScript(), {}, invalidAdmin);
    ASSERT_TRUE(invalid.Open());
    ASSERT_TRUE(invalid.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    ASSERT_TRUE(invalid.App().AdminEnabled);
    ASSERT_EQ(invalid.App().AdminPort, invalidToken.Port());
    recordThroughPanel(invalid.Instance());
    AdminAuth invalidAuth(10, 1.0);
    invalidAuth.SetToken(Token);
    AdminRouter invalidRouter(invalidAuth);
    invalid.Instance().Register(invalidRouter, [] { return AdminStatusSnapshot{}; });
    AdminResponse const tokenRefused = invalidRouter.Dispatch(Request("POST", "/api/apps/helper/api/command", R"({"command":"ping"})"));
    ASSERT_EQ(tokenRefused.Status, 502) << tokenRefused.Body;
    EXPECT_EQ(nlohmann::json::parse(tokenRefused.Body)["error"], "app_invalid_token");

    std::size_t relayedSettings = 0;
    invalid.Instance().SetRelayHooks({ [](AdminRequest const&) { return std::string("operator"); },
        [&relayedSettings](AdminRequest const&, RelayedAnswer const&) { ++relayedSettings; } });
    AdminResponse const settingRefused = invalidRouter.Dispatch(
        Request("PUT", "/api/apps/helper/api/settings/Database.Host", R"({"value":"localhost","reason":"test"})"));
    ASSERT_EQ(settingRefused.Status, 502) << settingRefused.Body;
    EXPECT_EQ(relayedSettings, 0u);

    std::optional<PanelStore::Statement> rows = panel.Store().Prepare(
        "SELECT result, error FROM audit_event WHERE name = 'app:console.command' ORDER BY id", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_EQ(rows->Text(0), "failed");
    EXPECT_EQ(nlohmann::json::parse(rows->Text(1))["error"], "app_not_running");
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_EQ(rows->Text(0), "failed");
    EXPECT_EQ(nlohmann::json::parse(rows->Text(1))["error"], "app_invalid_token");
    EXPECT_FALSE(rows->Step(error));
    EXPECT_TRUE(error.empty()) << error;
}
