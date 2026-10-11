/*
 * Project Ambrose by Imjustchico
 * Runs the supervisor over the helper program as its app and proves the crash behaviour end to end: four kills from outside inside the crash loop window stop the restarts, badge the app as crash looping and keep four crash records with their log tails, a clean exit nobody asked for is clean but unexpected rather than a crash, an app past its memory limit is classified out of memory, a crash while a restore holds the app's lock is recorded but not restarted until the restore releases it, and the crashes route and the published conditions carry what the alert rules need.
 */

#include "AdminAuth.h"
#include "AdminRouter.h"
#include "AdminStatus.h"
#include "ChildProcess.h"
#include "ConfigMgr.h"
#include "CrashPolicy.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "ManagedApp.h"
#include "Supervisor.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
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
}

TEST(SupervisorCrashTest, FourKillsFromOutsideTripTheCrashLoopAndKeepFourRecords)
{
    Rig rig({ "echo", ReadyLine, "wait-for-stop" }, "Supervisor.BackoffBaseSeconds = 0\n");
    ASSERT_TRUE(rig.Open());
    for (uint32 crashes = 1; crashes <= 4; ++crashes)
    {
        ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; })) << "before kill " << crashes;
        EndFromOutside(*rig.App().ProcessId);
        ASSERT_TRUE(rig.WaitFor([crashes](AppSnapshot const& app) { return app.Crashes == crashes; })) << "after kill " << crashes;
    }
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::CrashLoop; }));
    AppSnapshot const looped = rig.App();
    EXPECT_TRUE(looped.CrashLoop);
    EXPECT_EQ(looped.Crashes, 4u);
    ASSERT_EQ(looped.CrashHistory.size(), 4u);
    for (CrashRecord const& record : looped.CrashHistory)
    {
        EXPECT_EQ(record.Classification, ExitClassification::Crash);
#ifdef __linux__
        EXPECT_EQ(record.Signal, std::optional<int>(9)) << "an ended tree is SIGKILL";
#else
        EXPECT_TRUE(record.Code.has_value());
#endif
        EXPECT_FALSE(record.LogTail.empty());
        EXPECT_FALSE(record.DumpWritten);
    }
    bool tailHasWaiting = false;
    for (std::string const& line : looped.CrashHistory.back().LogTail)
        if (line.find("waiting") != std::string::npos)
            tailHasWaiting = true;
    EXPECT_TRUE(tailHasWaiting) << "the last crash keeps the log tail of its run";
    nlohmann::json const status = nlohmann::json::parse(Supervisor::StatusData(looped));
    EXPECT_EQ(status["state"], "crash_loop");
    EXPECT_TRUE(status["crash_loop"]);
    EXPECT_EQ(status["classification"], "crash");

    std::this_thread::sleep_for(1500ms);
    AppSnapshot const still = rig.App();
    EXPECT_EQ(still.State, AppState::CrashLoop) << "no restart while crash looping";
    EXPECT_FALSE(still.ProcessId.has_value());
    EXPECT_EQ(still.CrashHistory.size(), 4u);

    PowerResult const start = rig.Instance().Power("helper", PowerAction::Start, 0);
    EXPECT_TRUE(start.Accepted) << start.Message;
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    AppSnapshot const again = rig.App();
    EXPECT_FALSE(again.CrashLoop) << "a manual start clears the loop";
    EXPECT_EQ(again.CrashHistory.size(), 4u) << "the history survives the loop";
}

TEST(SupervisorCrashTest, ACleanExitNobodyAskedForIsCleanButUnexpected)
{
    Rig rig({ "echo", ReadyLine, "sleep", "1500", "exit", "0" });
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Crashed; }));
    AppSnapshot const exited = rig.App();
    ASSERT_FALSE(exited.Exits.empty());
    EXPECT_EQ(exited.Exits.back().Classification, ExitClassification::CleanUnexpected);
    EXPECT_EQ(exited.Exits.back().Code, std::optional<int64>(0));
    EXPECT_EQ(exited.Crashes, 0u) << "a clean exit is not a crash";
    EXPECT_TRUE(exited.CrashHistory.empty());
    std::this_thread::sleep_for(1500ms);
    AppSnapshot const later = rig.App();
    EXPECT_EQ(later.State, AppState::Crashed);
    EXPECT_EQ(later.Exits.size(), 1u) << "a clean exit nobody asked for is not restarted";
}

TEST(SupervisorCrashTest, AnAppOverItsMemoryLimitIsClassifiedOutOfMemory)
{
    Rig rig({ "echo", ReadyLine, "alloc", "64", "wait-for-stop" }, "App.helper.MemoryLimitMB = 16\n");
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app)
    {
        return !app.Exits.empty() && app.Exits.back().Classification == ExitClassification::OutOfMemory;
    }, 60s));
    AppSnapshot const killed = rig.App();
    EXPECT_GE(killed.Crashes, 1u);
    ASSERT_FALSE(killed.CrashHistory.empty());
    EXPECT_EQ(killed.CrashHistory.back().Classification, ExitClassification::OutOfMemory);
    EXPECT_TRUE(rig.Said("past its limit of 16 MB"));
}

TEST(SupervisorCrashTest, ACrashWhileARestoreHoldsTheLockIsRecordedAndWaitsForRelease)
{
    Rig rig({ "echo", ReadyLine, "wait-for-stop" });
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    LockResult const locked = rig.Instance().AcquireAppLock("helper", "restore", "backup restore");
    EXPECT_TRUE(locked.Acquired) << locked.Refusal;
    PowerResult const refused = rig.Instance().Power("helper", PowerAction::Stop, 0);
    EXPECT_FALSE(refused.Accepted);
    EXPECT_EQ(refused.Status, 409);
    EXPECT_NE(refused.Message.find("restore"), std::string::npos) << refused.Message;
    EXPECT_NE(refused.Message.find("backup restore"), std::string::npos) << refused.Message;

    EndFromOutside(*rig.App().ProcessId);
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.CrashHistory.size() == 1; }));
    AppSnapshot const crashed = rig.App();
    EXPECT_EQ(crashed.CrashHistory.back().Classification, ExitClassification::Crash);
    EXPECT_TRUE(crashed.Lock.has_value());
    EXPECT_EQ(crashed.Lock->Holder, "restore");
    std::this_thread::sleep_for(2500ms);
    AppSnapshot const waiting = rig.App();
    EXPECT_FALSE(waiting.ProcessId.has_value()) << "no restart while the restore holds the lock";
    EXPECT_EQ(waiting.State, AppState::Backoff);

    EXPECT_TRUE(rig.Instance().ReleaseAppLock("helper", "restore"));
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    EXPECT_EQ(rig.App().Restarts, 1u) << "the restart held back by the lock runs once it is released";
}

TEST(SupervisorCrashTest, TheCrashesRouteCarriesEachRecordWithItsLogTail)
{
    Rig rig({ "echo", ReadyLine, "sleep", "1500", "exit", "3" });
    ASSERT_TRUE(rig.Open());
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return app.State == AppState::Running; }));
    ASSERT_TRUE(rig.WaitFor([](AppSnapshot const& app) { return !app.CrashHistory.empty(); }));
    AdminAuth auth(10, 1.0);
    auth.SetToken(Token);
    AdminRouter router(auth);
    AdminStatusSnapshot self;
    self.App.Name = "supervisor";
    self.App.Role = "supervisor";
    self.App.Revision = "rev";
    self.State = "running";
    rig.Instance().Register(router, [self] { return self; });

    AdminResponse const answer = router.Dispatch(Request("GET", "/api/apps/helper/crashes"));
    ASSERT_EQ(answer.Status, 200) << answer.Body;
    nlohmann::json const records = nlohmann::json::parse(answer.Body);
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0]["classification"], "crash");
    EXPECT_EQ(records[0]["code"], 3);
    EXPECT_FALSE(records[0]["log_tail"].empty());
    EXPECT_FALSE(records[0]["dump_written"]);
    EXPECT_EQ(records[0]["dump_note"], "no dump was written");
}

TEST(SupervisorCrashTest, ThePublishedConditionsCarryWhatAlertRulesNeed)
{
    CrashEvent event;
    event.Record.EpochMs = 1728646400000;
    event.Record.Code = 1;
    event.Record.Classification = ExitClassification::Crash;
    event.Record.UptimeMs = 5000;
    event.CrashesInWindow = 2;
    nlohmann::json const crashed = nlohmann::json::parse(Supervisor::CrashEventJson("helper", event));
    EXPECT_EQ(crashed["app"], "helper");
    EXPECT_EQ(crashed["time"], 1728646400000);
    EXPECT_EQ(crashed["classification"], "crash");
    EXPECT_EQ(crashed["exit_code"], 1);
    EXPECT_EQ(crashed["uptime_ms"], 5000);
    EXPECT_EQ(crashed["crashes_in_window"], 2);
    EXPECT_EQ(crashed["dump_note"], "no dump was written");

    event.CrashLoop = true;
    event.CrashesInWindow = 4;
    nlohmann::json const loop = nlohmann::json::parse(Supervisor::CrashLoopEventJson("helper", event));
    EXPECT_EQ(loop["app"], "helper");
    EXPECT_EQ(loop["crashes_in_window"], 4);
    EXPECT_EQ(loop["last_classification"], "crash");
}
