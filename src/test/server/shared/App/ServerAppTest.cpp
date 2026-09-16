/*
 * Project Ambrose by Imjustchico
 * Tests the shared app lifecycle in process: options, version, missing or broken config, ready and stop logging, --check, console commands and input, live update intervals, repeated runs, and shutdown on signals.
 */

#include "AppOptions.h"
#include "ConfigMgr.h"
#include "ConsoleInput.h"
#include "GitRevision.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "ScopeExit.h"
#include "ServerApp.h"

#include <gtest/gtest.h>

#include <atomic>
#include <csignal>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <thread>

namespace
{
    class ScriptedConsoleInput : public ConsoleInput
    {
    public:
        ScriptedConsoleInput(std::vector<std::string> lines, bool closeAtEnd, std::atomic<int>& interrupts)
            : _lines(std::move(lines)), _closeAtEnd(closeAtEnd), _interrupts(interrupts)
        {
        }

        ReadResult ReadLine(std::string& line, std::chrono::milliseconds timeout) override
        {
            std::unique_lock lock(_mutex);
            if (_interrupted)
                return ReadResult::Closed;
            if (_next < _lines.size())
            {
                line = _lines[_next++];
                return ReadResult::Line;
            }
            if (_closeAtEnd)
                return ReadResult::Closed;
            _wake.wait_for(lock, timeout, [this] { return _interrupted; });
            return _interrupted ? ReadResult::Closed : ReadResult::Timeout;
        }

        void Interrupt() override
        {
            std::lock_guard const lock(_mutex);
            _interrupted = true;
            _interrupts.fetch_add(1);
            _wake.notify_all();
        }

    private:
        std::vector<std::string> _lines;
        std::size_t _next = 0;
        bool _closeAtEnd;
        bool _interrupted = false;
        std::atomic<int>& _interrupts;
        std::mutex _mutex;
        std::condition_variable _wake;
    };

    class TickApp : public ServerApp
    {
    public:
        using ServerApp::ServerApp;

        std::atomic<int64> IntervalMs{ 0 };
        std::atomic<int> Updates{ 0 };
        std::atomic<bool> Started{ false };
        std::atomic<bool> Stopped{ false };
        bool FailStart = false;
        bool SignalDuringStop = false;
        std::function<std::unique_ptr<ConsoleInput>()> ConsoleFactory;

    protected:
        bool OnStart() override
        {
            Started = true;
            return !FailStart;
        }

        std::unique_ptr<ConsoleInput> CreateConsoleInput() override
        {
            return ConsoleFactory ? ConsoleFactory() : nullptr;
        }

        std::chrono::milliseconds GetUpdateInterval() const override { return std::chrono::milliseconds(IntervalMs.load()); }
        void OnUpdate(std::chrono::milliseconds) override { Updates.fetch_add(1); }
        void OnStop() override
        {
            if (SignalDuringStop)
            {
                std::raise(SIGINT);
                GetIoContext().Restart();
                GetIoContext().Poll();
            }
            Stopped = true;
        }
    };

    class ServerAppTest : public testing::Test
    {
    protected:
        std::filesystem::path WriteConfig(std::string_view extra = "")
        {
            std::filesystem::path const file = _directory.Path() / "testserver.conf";
            std::ofstream stream(file);
            stream << "LogsDir = logs\nAppender.Console = 1,3,0\nLogger.root = 3,Console\n" << extra;
            return file;
        }

        bool WaitFor(std::function<bool()> const& condition)
        {
            auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!condition())
            {
                if (std::chrono::steady_clock::now() > deadline)
                    return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return true;
        }

        LogTestDirectory _directory;
        LogTestHarness _harness;
        ConfigMgr _config;
        std::ostringstream _out;
        std::ostringstream _err;
    };
}

TEST(AppOptionsTest, ParsesEveryOptionForm)
{
    AppOptions const defaults = AppOptions::Parse({ "app" }, "app.conf");
    EXPECT_EQ(defaults.ConfigFile, "app.conf");
    EXPECT_FALSE(defaults.ShowVersion);
    EXPECT_FALSE(defaults.CheckOnly);
    EXPECT_TRUE(defaults.Error.empty());

    AppOptions const full = AppOptions::Parse({ "app", "-c", "a.conf", "--set", "Network.Threads=4", "--set=Log.Utc = 1", "-v", "--help", "--check" }, "app.conf");
    EXPECT_TRUE(full.Error.empty()) << full.Error;
    EXPECT_EQ(full.ConfigFile, "a.conf");
    EXPECT_TRUE(full.ShowVersion);
    EXPECT_TRUE(full.ShowHelp);
    EXPECT_TRUE(full.CheckOnly);
    ASSERT_EQ(full.Overrides.size(), 2u);
    EXPECT_EQ(full.Overrides[0], (std::pair<std::string, std::string>{ "Network.Threads", "4" }));
    EXPECT_EQ(full.Overrides[1], (std::pair<std::string, std::string>{ "Log.Utc", "1" }));
    EXPECT_EQ(AppOptions::Parse({ "app", "--config=b.conf" }, "app.conf").ConfigFile, "b.conf");
    AppOptions const quoted = AppOptions::Parse({ "app", "--set", "Motd = \" hello \" " }, "app.conf");
    ASSERT_EQ(quoted.Overrides.size(), 1u);
    EXPECT_EQ(quoted.Overrides[0], (std::pair<std::string, std::string>{ "Motd", " hello " }));

    EXPECT_EQ(AppOptions::Parse({ "app", "--config" }, "app.conf").Error, "option '--config' needs a value");
    EXPECT_EQ(AppOptions::Parse({ "app", "--config=" }, "app.conf").Error, "option '--config' needs a file name");
    EXPECT_EQ(AppOptions::Parse({ "app", "--set", "novalue" }, "app.conf").Error, "option '--set' needs Key=Value, got 'novalue'");
    EXPECT_EQ(AppOptions::Parse({ "app", "--bogus" }, "app.conf").Error, "unknown argument '--bogus'");
    EXPECT_EQ(AppOptions::Parse({ "app", "stray" }, "app.conf").Error, "unknown argument 'stray'");
    EXPECT_NE(AppOptions::Usage("app", "app.conf").find("--config <file>"), std::string::npos);
}

TEST_F(ServerAppTest, VersionHelpAndBadArgumentsExitWithoutConfig)
{
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    EXPECT_EQ(app.Run({ "testserver", "--version" }), EXIT_SUCCESS);
    EXPECT_EQ(_out.str(), GitRevision::GetFullVersion() + "\n");
    _out.str("");
    EXPECT_EQ(app.Run({ "testserver", "-h" }), EXIT_SUCCESS);
    EXPECT_NE(_out.str().find("Usage: testserver"), std::string::npos);
    EXPECT_EQ(app.Run({ "testserver", "--nope" }), EXIT_FAILURE);
    EXPECT_NE(_err.str().find("unknown argument '--nope'"), std::string::npos);
    EXPECT_FALSE(app.Started.load());
}

TEST_F(ServerAppTest, MissingConfigNamesThePathAndFails)
{
    std::filesystem::path const missing = _directory.Path() / "absent.conf";
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    EXPECT_EQ(app.Run({ "testserver", "--config", ConfigMgr::PathToUtf8(missing) }), EXIT_FAILURE);
    EXPECT_NE(_err.str().find(ConfigMgr::PathToUtf8(std::filesystem::absolute(missing))), std::string::npos) << _err.str();
    EXPECT_NE(_err.str().find("testserver.conf.dist"), std::string::npos) << _err.str();
    EXPECT_FALSE(app.Started.load());
}

TEST_F(ServerAppTest, BrokenConfigReportsTheErrorWithoutTheCopyHint)
{
    std::filesystem::path const file = WriteConfig("this line has no equals sign\n");
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    EXPECT_EQ(app.Run({ "testserver", "--config", ConfigMgr::PathToUtf8(file) }), EXIT_FAILURE);
    EXPECT_NE(_err.str().find(ConfigMgr::PathToUtf8(std::filesystem::absolute(file))), std::string::npos) << _err.str();
    EXPECT_EQ(_err.str().find(".dist"), std::string::npos) << _err.str();
    EXPECT_FALSE(app.Started.load());
}

TEST_F(ServerAppTest, RunsUntilStoppedAndLogsTheLifecycle)
{
    std::filesystem::path const file = WriteConfig();
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    int exitCode = -1;
    std::thread runner([&] { exitCode = app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file), "--set", "World.UpdateInterval=5" }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    ASSERT_TRUE(WaitFor([&] { return app.IsReady(); }));
    EXPECT_EQ(_config.GetOption<uint32>("World.UpdateInterval", 0, true), 5u);
    auto const stopAt = std::chrono::steady_clock::now();
    app.RequestStop();
    runner.join();
    EXPECT_LT(std::chrono::steady_clock::now() - stopAt, std::chrono::seconds(2));
    EXPECT_EQ(exitCode, EXIT_SUCCESS);
    EXPECT_TRUE(app.Stopped.load());
    std::string const output = _harness.Device().Output();
    EXPECT_NE(output.find("testserver ready"), std::string::npos) << output;
    EXPECT_NE(output.find("testserver shutting down after a stop request"), std::string::npos) << output;
    EXPECT_NE(output.find("testserver stopped"), std::string::npos) << output;
}

TEST_F(ServerAppTest, CheckStartsReportsReadyAndStopsCleanly)
{
    std::filesystem::path const file = WriteConfig();
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    EXPECT_EQ(app.Run({ "testserver", "--check", "-c", ConfigMgr::PathToUtf8(file) }), EXIT_SUCCESS);
    EXPECT_TRUE(app.Started.load());
    EXPECT_TRUE(app.Stopped.load());
    EXPECT_FALSE(app.IsReady());
    std::string const output = _harness.Device().Output();
    EXPECT_NE(output.find("testserver ready"), std::string::npos) << output;
    EXPECT_NE(output.find("testserver shutting down after --check"), std::string::npos) << output;
    EXPECT_NE(output.find("testserver stopped"), std::string::npos) << output;
}

TEST_F(ServerAppTest, ConsoleLinesRunCommandsUntilShutdown)
{
    std::filesystem::path const file = WriteConfig();
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    std::atomic<int> interrupts{ 0 };
    app.ConsoleFactory = [&interrupts]
    {
        return std::make_unique<ScriptedConsoleInput>(std::vector<std::string>{ "help", "   ", "bogus words here", "shutdown now", "SHUTDOWN" }, false, interrupts);
    };
    int exitCode = -1;
    std::thread runner([&] { exitCode = app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    ASSERT_TRUE(WaitFor([&] { return app.Stopped.load(); }));
    runner.join();
    EXPECT_EQ(exitCode, EXIT_SUCCESS);
    EXPECT_GE(interrupts.load(), 1);

    std::string const out = _out.str();
    EXPECT_NE(out.find("help [command] - list commands, or the commands starting with the given words\n"), std::string::npos) << out;
    EXPECT_NE(out.find("shutdown - stop the server gracefully\n"), std::string::npos) << out;
    EXPECT_NE(out.find("Unknown command 'bogus'. Type 'help' to list commands.\n"), std::string::npos) << out;
    EXPECT_NE(out.find("Usage: shutdown\n"), std::string::npos) << out;
    EXPECT_NE(out.find("testserver is shutting down\n"), std::string::npos) << out;

    std::string const log = _harness.Device().Output();
    EXPECT_NE(log.find("Console: help"), std::string::npos) << log;
    EXPECT_NE(log.find("Console: unknown command 'bogus'"), std::string::npos) << log;
    EXPECT_EQ(log.find("words here"), std::string::npos) << log;
    EXPECT_NE(log.find("testserver shutting down after the shutdown command"), std::string::npos) << log;
    EXPECT_NE(log.find("testserver stopped"), std::string::npos) << log;
}

TEST_F(ServerAppTest, ClosedConsoleKeepsRunningAndStopInterruptsAWaitingReader)
{
    std::filesystem::path const file = WriteConfig();
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    std::atomic<int> interrupts{ 0 };
    app.ConsoleFactory = [&interrupts] { return std::make_unique<ScriptedConsoleInput>(std::vector<std::string>{}, true, interrupts); };
    std::thread runner([&] { app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    ASSERT_TRUE(WaitFor([&] { return app.IsReady(); }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(app.IsReady());
    EXPECT_FALSE(app.Stopped.load());
    app.RequestStop();
    runner.join();

    TickApp waiting({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    std::atomic<int> waitingInterrupts{ 0 };
    waiting.ConsoleFactory = [&waitingInterrupts] { return std::make_unique<ScriptedConsoleInput>(std::vector<std::string>{}, false, waitingInterrupts); };
    std::thread second([&] { waiting.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }); });
    ScopeExit const joinSecond([&] { if (second.joinable()) { waiting.RequestStop(); second.join(); } });
    ASSERT_TRUE(WaitFor([&] { return waiting.IsReady(); }));
    auto const stopAt = std::chrono::steady_clock::now();
    waiting.RequestStop();
    second.join();
    EXPECT_LT(std::chrono::steady_clock::now() - stopAt, std::chrono::seconds(5));
    EXPECT_EQ(waitingInterrupts.load(), 1);
}

TEST_F(ServerAppTest, ConsoleEnableZeroStartsNoReader)
{
    std::filesystem::path const file = WriteConfig("Console.Enable = 0\n");
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    std::atomic<bool> created{ false };
    app.ConsoleFactory = [&created] { created = true; return std::unique_ptr<ConsoleInput>(); };
    std::thread runner([&] { app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    ASSERT_TRUE(WaitFor([&] { return app.IsReady(); }));
    app.RequestStop();
    runner.join();
    EXPECT_FALSE(created.load());
}

TEST_F(ServerAppTest, LifecycleLinesFollowTheAppCategory)
{
    std::filesystem::path const file = WriteConfig("Logger.server.testserver = 4,Console\n");
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    std::thread runner([&] { app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    ASSERT_TRUE(WaitFor([&] { return app.IsReady(); }));
    app.RequestStop();
    runner.join();
    EXPECT_TRUE(app.Stopped.load());
    std::string const output = _harness.Device().Output();
    EXPECT_EQ(output.find("testserver ready"), std::string::npos) << output;
    EXPECT_EQ(output.find("testserver stopped"), std::string::npos) << output;
}

TEST_F(ServerAppTest, AStopFromAnEarlierRunDoesNotStopTheNextOne)
{
    std::filesystem::path const file = WriteConfig();
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    for (int run = 0; run < 2; ++run)
    {
        std::atomic<int> exitCode{ -1 };
        std::thread runner([&] { exitCode = app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }); });
        ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
        ASSERT_TRUE(WaitFor([&] { return app.IsReady(); }));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_TRUE(app.IsReady());
        EXPECT_EQ(exitCode.load(), -1);
        app.RequestStop();
        runner.join();
        EXPECT_EQ(exitCode.load(), EXIT_SUCCESS);
    }
}

TEST_F(ServerAppTest, UpdateIntervalChangesApplyFromTheNextTick)
{
    std::filesystem::path const file = WriteConfig();
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    app.IntervalMs = 1000;
    std::thread runner([&] { app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    ASSERT_TRUE(WaitFor([&] { return app.IsReady(); }));
    app.IntervalMs = 1;
    EXPECT_TRUE(WaitFor([&] { return app.Updates.load() >= 20; }));
    app.IntervalMs = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int const paused = app.Updates.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_LE(app.Updates.load(), paused + 1);
    app.RequestStop();
    runner.join();
}

TEST_F(ServerAppTest, FailedStartExitsWithFailure)
{
    std::filesystem::path const file = WriteConfig();
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    app.FailStart = true;
    EXPECT_EQ(app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }), EXIT_FAILURE);
    EXPECT_TRUE(app.Started.load());
    EXPECT_NE(_harness.Device().Output().find("testserver failed to start"), std::string::npos);
}

TEST_F(ServerAppTest, InterruptSignalShutsDownGracefully)
{
    std::filesystem::path const file = WriteConfig();
    TickApp app({ "testserver", "testserver.conf" }, _config, _harness.GetLog(), _out, _err);
    app.IntervalMs = 10;
    int exitCode = -1;
    app.SignalDuringStop = true;
    std::thread runner([&] { exitCode = app.Run({ "testserver", "-c", ConfigMgr::PathToUtf8(file) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    ASSERT_TRUE(WaitFor([&] { return app.IsReady() && app.Updates.load() > 0; }));
    auto const signalAt = std::chrono::steady_clock::now();
    std::raise(SIGINT);
    runner.join();
    EXPECT_LT(std::chrono::steady_clock::now() - signalAt, std::chrono::seconds(2));
    EXPECT_EQ(exitCode, EXIT_SUCCESS);
    EXPECT_TRUE(app.Stopped.load());
    EXPECT_NE(_harness.Device().Output().find("testserver shutting down after signal"), std::string::npos) << _harness.Device().Output();
}
