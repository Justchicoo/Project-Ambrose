/*
 * Project Ambrose by Imjustchico
 * Checks the whole chain a reload of the configuration runs, the one an operator sees as editing a .conf file and typing reload: the file on disk is changed, the config target rebuilds from it, the subscriber that logging registered is told which keys changed, and log routing follows without the server being stopped. Checks too that a file that cannot be parsed leaves the routing that was serving in place, because a reload that fails must not take logging down with it.
 */

#include "Log.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "ReloadMgr.h"
#include "TestAppenderStore.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    std::string const Header = "# Project Ambrose by Imjustchico\n# Logging test configuration.\n";

    class ConfigReloadRoutesLoggingTest : public testing::Test
    {
    protected:
        void SetUp() override { sReloadMgr.Clear(); }
        void TearDown() override { sReloadMgr.Clear(); }
    };
}

TEST_F(ConfigReloadRoutesLoggingTest, EditingTheFileAndReloadingChangesRoutingWithoutARestart)
{
    LogTestDirectory directory;
    LogTestHarness harness;

    std::string const atInfo = Header + "Appender.Capture = 200,2,0\nLogger.root = 3,Capture\nLogger.network = 3,Capture\n";
    std::filesystem::path const file = directory.Write("app.conf", atInfo);

    ConfigMgr config;
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    ASSERT_TRUE(harness.GetLog().LoadFromConfig(config).Succeeded());

    std::vector<std::string> told;
    config.SubscribeToChanges([&harness, &config, &told](std::vector<std::string> const& changed)
    {
        told = changed;
        harness.GetLog().LoadFromConfig(config);
    });
    ASSERT_TRUE(sReloadMgr.Register("config", [&config](std::vector<std::string>& errors)
    {
        ConfigLoadResult const result = config.Reload();
        for (ConfigIssue const& issue : result.Errors)
            errors.push_back(issue.ToString());
        return result.Succeeded();
    }));

    EXPECT_FALSE(harness.GetLog().ShouldLog("network", LogLevel::Debug)) << "network starts at Info, so a Debug line is not routed";

    directory.Write("app.conf", Header + "Appender.Capture = 200,2,0\nLogger.root = 3,Capture\nLogger.network = 2,Capture\n");
    ReloadOutcome const outcome = sReloadMgr.Reload("config");
    ASSERT_TRUE(outcome.Ok) << (outcome.Errors.empty() ? std::string() : outcome.Errors.front());
    EXPECT_EQ(outcome.Generation, 1u);

    EXPECT_EQ(told, (std::vector<std::string>{ "Logger.network" })) << "logging is told which key changed, not merely that something did";
    EXPECT_TRUE(harness.GetLog().ShouldLog("network", LogLevel::Debug)) << "the edited level is in force without the server being stopped";
    EXPECT_FALSE(harness.GetLog().ShouldLog("network", LogLevel::Trace)) << "and only as far as the file asked";
}

TEST_F(ConfigReloadRoutesLoggingTest, AFileThatCannotBeParsedLeavesRoutingAsItWas)
{
    LogTestDirectory directory;
    LogTestHarness harness;

    std::filesystem::path const file = directory.Write("app.conf", Header + "Appender.Capture = 200,2,0\nLogger.root = 3,Capture\nLogger.network = 2,Capture\n");

    ConfigMgr config;
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    ASSERT_TRUE(harness.GetLog().LoadFromConfig(config).Succeeded());
    int told = 0;
    config.SubscribeToChanges([&harness, &config, &told](std::vector<std::string> const&)
    {
        ++told;
        harness.GetLog().LoadFromConfig(config);
    });
    ASSERT_TRUE(sReloadMgr.Register("config", [&config](std::vector<std::string>& errors)
    {
        ConfigLoadResult const result = config.Reload();
        for (ConfigIssue const& issue : result.Errors)
            errors.push_back(issue.ToString());
        return result.Succeeded();
    }));

    ASSERT_TRUE(harness.GetLog().ShouldLog("network", LogLevel::Debug));

    directory.Write("app.conf", Header + "Logger.network == 4,Capture\n");
    ReloadOutcome const refused = sReloadMgr.Reload("config");
    EXPECT_FALSE(refused.Ok) << "a file that cannot be parsed is not a reload that worked";
    EXPECT_FALSE(refused.Errors.empty()) << "and the operator is told what was wrong with it";
    EXPECT_EQ(refused.Generation, 0u) << "the generation that was serving goes on serving";
    EXPECT_EQ(told, 0) << "nothing is announced for a reload that never took";
    EXPECT_TRUE(harness.GetLog().ShouldLog("network", LogLevel::Debug)) << "routing is exactly what it was before the bad edit";
}
