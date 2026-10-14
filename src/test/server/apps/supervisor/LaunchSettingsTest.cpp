/*
 * Project Ambrose by Imjustchico
 * Tests the launch settings one app resolves: unset fields inherit the app definition the config file gave, a set stop timeout overrides it and is read fresh for every stop, command-line overrides become --set arguments, and the resolved environment carries the unsealed secret values for the child to inherit.
 */

#include "AppDefinition.h"
#include "LaunchSettings.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "ManagedApp.h"
#include "SupervisorState.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

namespace
{
    AppDefinition Definition(std::filesystem::path const& folder)
    {
        AppDefinition definition;
        definition.Name = "gameserver";
        definition.Program = folder / "gameserver";
        definition.ProgramName = "gameserver";
        definition.Config = folder / "gameserver.conf";
        definition.WorkingDirectory = folder;
        definition.Autostart = true;
        definition.StartTimeout = std::chrono::seconds(120);
        definition.StopTimeout = std::chrono::seconds(60);
        return definition;
    }

    class LaunchSettingsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _state = std::make_unique<SupervisorState>(_directory.Path() / "state.json");
        }

        ManagedApp MakeApp(AppDefinition definition)
        {
            return ManagedApp(std::move(definition), *_state, _directory.Path() / "output", 1024 * 1024,
                _directory.Path(), [](int64, std::string&) { return false; }, _harness.GetLog());
        }

        LogTestDirectory _directory;
        LogTestHarness _harness;
        std::unique_ptr<SupervisorState> _state;
    };
}

TEST_F(LaunchSettingsTest, UnsetFieldsInheritTheAppDefinition)
{
    ManagedApp app = MakeApp(Definition(_directory.Path()));
    ResolvedLaunch const resolved = app.ResolveLaunch();
    EXPECT_EQ(resolved.StopTimeout, std::chrono::seconds(60));
    EXPECT_EQ(resolved.StartTimeout, std::chrono::seconds(120));
    EXPECT_TRUE(resolved.Autostart);
    EXPECT_EQ(resolved.RestartPolicy, LaunchRestartPolicy::OnCrash);
    EXPECT_TRUE(resolved.ExtraArguments.empty());
    EXPECT_TRUE(resolved.Environment.empty());
    EXPECT_EQ(resolved.MemoryLimitBytes, 0u);
}

TEST_F(LaunchSettingsTest, AChangedStopTimeoutAppliesAtTheNextStop)
{
    ManagedApp app = MakeApp(Definition(_directory.Path()));
    LaunchSettings settings;
    settings.StopTimeoutSeconds = 90;
    app.SetLaunchSettingsProvider([settings](std::string_view) { return settings; });
    EXPECT_EQ(app.CurrentStopTimeout(), std::chrono::seconds(90));
    ResolvedLaunch const resolved = app.ResolveLaunch();
    EXPECT_EQ(resolved.StopTimeout, std::chrono::seconds(90));
    EXPECT_EQ(resolved.StartTimeout, std::chrono::seconds(120));
}

TEST_F(LaunchSettingsTest, OverridesBecomeSetArgumentsAndSecretsReachTheEnvironment)
{
    ManagedApp app = MakeApp(Definition(_directory.Path()));
    LaunchSettings settings;
    settings.Overrides["Rate.Drop.Item"] = "1.5";
    settings.Environment["DB_PASSWORD"] = "s3cret-value";
    settings.SecretEnvironment.insert("DB_PASSWORD");
    app.SetLaunchSettingsProvider([settings](std::string_view) { return settings; });
    ResolvedLaunch const resolved = app.ResolveLaunch();
    ASSERT_EQ(resolved.ExtraArguments.size(), 2u);
    EXPECT_EQ(resolved.ExtraArguments[0], "--set");
    EXPECT_EQ(resolved.ExtraArguments[1], "Rate.Drop.Item=1.5");
    ASSERT_EQ(resolved.Environment.size(), 1u);
    EXPECT_EQ(resolved.Environment[0].first, "DB_PASSWORD");
    EXPECT_EQ(resolved.Environment[0].second, "s3cret-value");
}

TEST_F(LaunchSettingsTest, LimitsPriorityAndRestartPolicyResolve)
{
    ManagedApp app = MakeApp(Definition(_directory.Path()));
    LaunchSettings settings;
    settings.RestartPolicy = LaunchRestartPolicy::Never;
    settings.BackoffSeconds = 5;
    settings.MaxRestarts = 3;
    settings.Priority = -1;
    settings.MemoryLimitBytes = 512ull * 1024ull * 1024ull;
    settings.CpuLimitPercent = 50;
    app.SetLaunchSettingsProvider([settings](std::string_view) { return settings; });
    ResolvedLaunch const resolved = app.ResolveLaunch();
    EXPECT_EQ(resolved.RestartPolicy, LaunchRestartPolicy::Never);
    EXPECT_EQ(resolved.Backoff, std::chrono::seconds(5));
    EXPECT_EQ(resolved.MaxRestarts, 3u);
    EXPECT_EQ(resolved.Priority, -1);
    EXPECT_EQ(resolved.MemoryLimitBytes, 512ull * 1024ull * 1024ull);
    EXPECT_EQ(resolved.CpuLimitPercent, 50u);
}

TEST_F(LaunchSettingsTest, AnAppPastItsMemoryLimitIsRecordedAsOutOfMemory)
{
    ManagedApp app = MakeApp(Definition(_directory.Path()));
    LaunchSettings settings;
    settings.MemoryLimitBytes = 256ull * 1024ull * 1024ull;
    app.SetLaunchSettingsProvider([settings](std::string_view) { return settings; });
    ResolvedLaunch const resolved = app.ResolveLaunch();
    EXPECT_EQ(resolved.MemoryLimitBytes, 256ull * 1024ull * 1024ull);
    AppExit exit;
    exit.Code = 0;
    exit.OutOfMemory = true;
    exit.UptimeMs = 1234;
    std::string const description = ManagedApp::DescribeExit("gameserver", exit);
    EXPECT_NE(description.find("out of memory past its limit"), std::string::npos) << description;
    EXPECT_NE(description.find("gameserver"), std::string::npos) << description;
    AppExit normalExit;
    normalExit.Code = 0;
    normalExit.UptimeMs = 1234;
    std::string const normalDescription = ManagedApp::DescribeExit("gameserver", normalExit);
    EXPECT_EQ(normalDescription.find("out of memory"), std::string::npos) << normalDescription;
}
