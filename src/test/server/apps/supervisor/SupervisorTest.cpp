/*
 * Project Ambrose by Imjustchico
 * Runs the supervisor over the helper program as its app: it starts it and calls it ready on its ready line, stops it with a shutdown line on its input, restarts it, counts one crash and starts it again when something else ends it, leaves a start that exits before it is ready alone, ends a start that never reports ready, takes a running app back after the supervisor is replaced and refuses the same process id once its start time no longer matches, and answers its routes: the app list carrying the supervisor and every app, the supervisor's own state, power requests refused field by field and by state, the captured output, and a relay that says why an app with its admin API off cannot be reached.
 */

#include "AdminAuth.h"
#include "AdminRouter.h"
#include "AdminStatus.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Supervisor.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <thread>
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
        explicit Rig(std::vector<std::string> const& script, std::string const& extra = {})
        {
            std::string lines;
            for (std::string const& line : script)
                lines += line + "\n";
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

        bool Open()
        {
            _instance = std::make_unique<Supervisor>(_harness.GetLog(), HelperBreak());
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
    EXPECT_TRUE(rig.Said("nothing works"));
    std::this_thread::sleep_for(2s);
    EXPECT_EQ(rig.App().State, AppState::Crashed);
    EXPECT_EQ(rig.App().Restarts, 0u);
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
