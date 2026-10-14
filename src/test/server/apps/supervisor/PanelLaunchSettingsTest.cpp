/*
 * Project Ambrose by Imjustchico
 * Tests one app's launch settings over the panel store: a validated change lands with its secrets sealed and masked everywhere they are shown, a build that does not exist is refused naming its path and keeps the previous setting, a working folder outside the install and an out-of-bounds timeout are refused, the preview shows the exact command line with --set overrides and masks secret environment values, and a closed store refuses plainly.
 */

#include "LogTestDirectory.h"
#include "PanelKeyring.h"
#include "PanelLaunchSettings.h"
#include "PanelStore.h"
#include "Settings.h"
#include "SourceFolder.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <mutex>
#include <string>

namespace
{
    SettingAuthor const Merle{ "Merle", 1, "panel" };

    class PanelLaunchSettingsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _file = _directory.Path() / "panel.sqlite3";
            _keyringFile = _directory.Path() / "keyring";
            Reopen();
            std::vector<std::string> notes;
            std::vector<std::string> warnings;
            std::string error;
            ASSERT_TRUE(_keyring.Load(_keyringFile, notes, warnings, error)) << error;
            _launch = std::make_unique<PanelLaunchSettings>(_store, _keyring, _mutex, _directory.Path());
        }

        void Reopen()
        {
            _store.Close();
            std::vector<std::string> warnings;
            std::string error;
            ASSERT_TRUE(_store.Open(_file, Ambrose::FindSourceFolder(), warnings, error)) << error;
        }

        LaunchSettings StopTimeoutSettings(uint32 seconds)
        {
            LaunchSettings settings;
            settings.StopTimeoutSeconds = seconds;
            return settings;
        }

        LogTestDirectory _directory;
        std::filesystem::path _file;
        std::filesystem::path _keyringFile;
        PanelStore _store;
        PanelKeyring _keyring;
        std::mutex _mutex;
        std::unique_ptr<PanelLaunchSettings> _launch;
    };
}

TEST_F(PanelLaunchSettingsTest, AValidatedChangeLandsAndReloads)
{
    LaunchSettings settings = StopTimeoutSettings(90);
    settings.RestartPolicy = LaunchRestartPolicy::Always;
    std::string audit;
    std::string error;
    ASSERT_TRUE(_launch->Write("gameserver", settings, Merle, error)) << error;
    audit = PanelLaunchSettings::AuditProperties("gameserver", LaunchSettings(), settings, "longer stop");
    LaunchSettings loaded;
    ASSERT_TRUE(_launch->Load("gameserver", loaded, error)) << error;
    EXPECT_EQ(loaded.StopTimeoutSeconds, 90u);
    EXPECT_EQ(loaded.RestartPolicy, LaunchRestartPolicy::Always);
    EXPECT_TRUE(audit.find("\"stop_timeout\":90") != std::string::npos);
}

TEST_F(PanelLaunchSettingsTest, SecretsAreSealedAndMaskedEverywhere)
{
    LaunchSettings settings;
    settings.Environment["DB_PASSWORD"] = "s3cret-value";
    settings.SecretEnvironment.insert("DB_PASSWORD");
    settings.Environment["PLAIN"] = "visible";
    std::string audit;
    std::string error;
    ASSERT_TRUE(_launch->Write("gameserver", settings, Merle, error)) << error;
    audit = PanelLaunchSettings::AuditProperties("gameserver", LaunchSettings(), settings, "secrets");
    EXPECT_EQ(audit.find("s3cret-value"), std::string::npos) << "the audit properties must never carry the secret";
    EXPECT_NE(audit.find("********"), std::string::npos);
    LaunchSettings loaded;
    ASSERT_TRUE(_launch->Load("gameserver", loaded, error)) << error;
    EXPECT_EQ(loaded.Environment["DB_PASSWORD"], "s3cret-value");
    EXPECT_TRUE(loaded.SecretEnvironment.contains("DB_PASSWORD"));
    nlohmann::json const answer = _launch->Answer("gameserver", "gameserver", { "--config", "gameserver.conf" }, error);
    ASSERT_TRUE(error.empty()) << error;
    EXPECT_EQ(answer["environment"]["DB_PASSWORD"], "********");
    EXPECT_EQ(answer["environment"]["PLAIN"], "visible");
    EXPECT_EQ(answer["preview_environment"][0]["value"], "********");
}

TEST_F(PanelLaunchSettingsTest, AMissingBuildIsRefusedNamingItsPathAndKeepsThePreviousSetting)
{
    std::string setupAudit;
    std::string setupError;
    ASSERT_TRUE(_launch->Write("gameserver", StopTimeoutSettings(60), Merle, setupError)) << setupError;
    LaunchSettings bad;
    bad.Build = "/nonexistent/build/gameserver";
    std::string audit;
    std::string error;
    EXPECT_FALSE(_launch->Write("gameserver", bad, Merle, error));
    EXPECT_NE(error.find("/nonexistent/build/gameserver"), std::string::npos) << error;
    LaunchSettings loaded;
    ASSERT_TRUE(_launch->Load("gameserver", loaded, error)) << error;
    EXPECT_EQ(loaded.StopTimeoutSeconds, 60u);
    EXPECT_TRUE(loaded.Build.empty());
}

TEST_F(PanelLaunchSettingsTest, AWorkingFolderOutsideTheInstallIsRefused)
{
    LaunchSettings settings;
    settings.WorkingDirectory = "/etc";
    std::string audit;
    std::string error;
    EXPECT_FALSE(_launch->Write("gameserver", settings, Merle, error));
    EXPECT_NE(error.find("outside the install"), std::string::npos) << error;
}

TEST_F(PanelLaunchSettingsTest, OutOfBoundsValuesAreRefused)
{
    LaunchSettings settings = StopTimeoutSettings(99999);
    std::string audit;
    std::string error;
    EXPECT_FALSE(_launch->Write("gameserver", settings, Merle, error));
    EXPECT_NE(error.find("stop timeout"), std::string::npos) << error;
    LaunchSettings cpu;
    cpu.CpuLimitPercent = 101;
    EXPECT_FALSE(_launch->Write("gameserver", cpu, Merle, error));
}

TEST_F(PanelLaunchSettingsTest, ThePreviewShowsTheExactCommandLineWithOverrides)
{
    LaunchSettings settings;
    settings.Overrides["Rate.Drop.Item"] = "1.5";
    settings.Environment["DB_PASSWORD"] = "s3cret-value";
    settings.SecretEnvironment.insert("DB_PASSWORD");
    std::string audit;
    std::string error;
    ASSERT_TRUE(_launch->Write("gameserver", settings, Merle, error)) << error;
    nlohmann::json const answer = _launch->Answer("gameserver", "gameserver", { "--config", "gameserver.conf" }, error);
    ASSERT_TRUE(error.empty()) << error;
    std::string const preview = answer["preview_command_line"].get<std::string>();
    EXPECT_NE(preview.find("--set Rate.Drop.Item=1.5"), std::string::npos) << preview;
    EXPECT_NE(preview.find("--config"), std::string::npos) << preview;
    EXPECT_EQ(preview.find("s3cret-value"), std::string::npos);
    EXPECT_EQ(answer["overrides"]["Rate.Drop.Item"], "1.5");
    ASSERT_EQ(answer["locked_keys"].size(), 1u);
    EXPECT_EQ(answer["locked_keys"][0]["key"], "Rate.Drop.Item");
    EXPECT_TRUE(answer["locked_keys"][0]["locked"].get<bool>());
    EXPECT_EQ(answer["locked_keys"][0]["layer"], "command-line override");
}

TEST_F(PanelLaunchSettingsTest, ALaunchedOverrideIsReportedLockedAndALiveEditIsRefusedNamingTheCommandLineOverrideLayer)
{
    LaunchSettings settings;
    settings.Overrides["Rate.Drop.Item"] = "1.5";
    std::string error;
    ASSERT_TRUE(_launch->Write("gameserver", settings, Merle, error)) << error;
    nlohmann::json const answer = _launch->Answer("gameserver", "gameserver", { "--config", "gameserver.conf" }, error);
    ASSERT_TRUE(error.empty()) << error;
    ASSERT_EQ(answer["locked_keys"].size(), 1u);
    EXPECT_EQ(answer["locked_keys"][0]["key"], "Rate.Drop.Item");
    EXPECT_TRUE(answer["locked_keys"][0]["locked"].get<bool>());
    EXPECT_EQ(answer["locked_keys"][0]["layer"], "command-line override");
    EXPECT_NE(answer["preview_command_line"].get<std::string>().find("--set Rate.Drop.Item=1.5"), std::string::npos);
    LaunchSettings changed = settings;
    changed.Overrides["Rate.Drop.Item"] = "2.0";
    EXPECT_FALSE(_launch->Write("gameserver", changed, Merle, error));
    EXPECT_NE(error.find("command-line override"), std::string::npos) << error;
    EXPECT_NE(error.find("Rate.Drop.Item"), std::string::npos) << error;
    LaunchSettings removed = settings;
    removed.Overrides.erase("Rate.Drop.Item");
    EXPECT_FALSE(_launch->Write("gameserver", removed, Merle, error));
    EXPECT_NE(error.find("command-line override"), std::string::npos) << error;
    LaunchSettings reloaded;
    ASSERT_TRUE(_launch->Load("gameserver", reloaded, error)) << error;
    EXPECT_EQ(reloaded.Overrides["Rate.Drop.Item"], "1.5");
}

TEST_F(PanelLaunchSettingsTest, TheAuditRowShowsOldAndNewValuesWithSecretsMasked)
{
    LaunchSettings old;
    old.StopTimeoutSeconds = 30;
    old.Environment["DB_PASSWORD"] = "old-secret";
    old.SecretEnvironment.insert("DB_PASSWORD");
    LaunchSettings changed = old;
    changed.StopTimeoutSeconds = 60;
    changed.Environment["DB_PASSWORD"] = "new-secret";
    nlohmann::json const properties = nlohmann::json::parse(
        PanelLaunchSettings::AuditProperties("gameserver", old, changed, "raising the stop timeout"));
    EXPECT_EQ(properties["old"]["stop_timeout"], 30);
    EXPECT_EQ(properties["new"]["stop_timeout"], 60);
    EXPECT_EQ(properties["old"]["environment"]["DB_PASSWORD"], "********");
    EXPECT_EQ(properties["new"]["environment"]["DB_PASSWORD"], "********");
    EXPECT_EQ(properties["reason"], "raising the stop timeout");
}

TEST_F(PanelLaunchSettingsTest, AClosedStoreRefusesPlainly)
{
    _store.Close();
    LaunchSettings loaded;
    std::string error;
    EXPECT_FALSE(_launch->Load("gameserver", loaded, error));
    EXPECT_NE(error.find("not open"), std::string::npos) << error;
}
