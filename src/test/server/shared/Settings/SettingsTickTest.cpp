/*
 * Project Ambrose by Imjustchico
 * Checks a live setting end to end in a running app: an app that reads World.UpdateInterval as its tick is started over an in-memory settings store, the settings set command typed on its console answers that the value changed, and the tick measured after it is the new interval within two ticks, with nothing restarted.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "ScopeExit.h"
#include "ServerApp.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    class MemoryStore : public SettingStore
    {
    public:
        bool Load(std::map<std::string, std::string, std::less<>>& values, std::string&) override
        {
            values = Values;
            return true;
        }

        bool Write(SettingWrite const& write, std::string&) override
        {
            if (write.Persisted)
                Values[write.Key] = *write.Persisted;
            else
                Values.erase(write.Key);
            return true;
        }

        bool History(std::string const&, std::size_t, std::vector<SettingAuditEntry>&, std::string&) override
        {
            return true;
        }

        std::map<std::string, std::string, std::less<>> Values;
    };

    class TickingApp : public ServerApp
    {
    public:
        using ServerApp::ServerApp;

        std::shared_ptr<SettingStore> Store;

        std::vector<std::chrono::steady_clock::time_point> Ticks() const
        {
            std::lock_guard const lock(_mutex);
            return _ticks;
        }

    protected:
        bool OnStart() override { return StartSettings(Store); }
        uint8 GetSettingApps() const override { return SettingApps::Game; }
        std::chrono::milliseconds GetUpdateInterval() const override { return std::chrono::milliseconds(sSettings.Get<uint32>("World.UpdateInterval")); }
        std::unique_ptr<ConsoleInput> CreateConsoleInput() override { return nullptr; }

        void OnUpdate(std::chrono::milliseconds) override
        {
            std::lock_guard const lock(_mutex);
            _ticks.push_back(std::chrono::steady_clock::now());
        }

    private:
        mutable std::mutex _mutex;
        std::vector<std::chrono::steady_clock::time_point> _ticks;
    };

    class SettingsTickTest : public testing::Test
    {
    protected:
        void SetUp() override { sSettings.Clear(); }
        void TearDown() override { sSettings.Clear(); }

        bool WaitFor(std::function<bool()> const& condition, std::chrono::seconds limit = std::chrono::seconds(20))
        {
            auto const deadline = std::chrono::steady_clock::now() + limit;
            while (!condition())
            {
                if (std::chrono::steady_clock::now() > deadline)
                    return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
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

TEST_F(SettingsTickTest, SettingTheUpdateIntervalOnTheConsoleChangesTheMeasuredTickWithinTwoTicks)
{
    std::filesystem::path const file = _directory.Write("tickserver.conf", "LogsDir = logs\nAppender.Console = 1,3,0\nLogger.root = 3,Console\nWorld.UpdateInterval = 20\n");
    TickingApp app({ "tickserver", "tickserver.conf" }, _config, _harness.GetLog(), _out, _err);
    app.Store = std::make_shared<MemoryStore>();
    int exitCode = -1;
    std::thread runner([&] { exitCode = app.Run({ "tickserver", "-c", ConfigMgr::PathToUtf8(file) }); });
    ScopeExit const joinOnExit([&] { if (runner.joinable()) { app.RequestStop(); runner.join(); } });
    ASSERT_TRUE(WaitFor([&] { return app.IsReady(); }));
    ASSERT_TRUE(WaitFor([&] { return app.Ticks().size() >= 6; }));

    std::vector<std::string> replies;
    ConsoleCommandTable::Result const ran = app.Commands().Execute("settings set World.UpdateInterval 100 slower ticks", [&replies](std::string_view line) { replies.emplace_back(line); });
    EXPECT_EQ(ran, ConsoleCommandTable::Result::Ran);
    ASSERT_FALSE(replies.empty());
    EXPECT_NE(replies.front().find("World.UpdateInterval is now 100"), std::string::npos) << replies.front();
    std::size_t const before = app.Ticks().size();

    ASSERT_TRUE(WaitFor([&] { return app.Ticks().size() >= before + 3; }));
    std::vector<std::chrono::steady_clock::time_point> const ticks = app.Ticks();
    auto const gap = std::chrono::duration_cast<std::chrono::milliseconds>(ticks[before + 2] - ticks[before + 1]);
    EXPECT_GE(gap.count(), 90) << "the second tick after the set already waits the new interval";

    app.RequestStop();
    runner.join();
    EXPECT_EQ(exitCode, EXIT_SUCCESS);
}
