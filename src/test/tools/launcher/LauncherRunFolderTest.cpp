/*
 * Project Ambrose by Imjustchico
 * Tests the folder the client runs from: the configuration is written from the install's own config.xml with only the window keys changed and SilentMetricsURL emptied, the install's VersionInfo and its other settings are kept, preferences carry the same window, a table or key the template lacks is added and listed, defaultconfig.xml in Root.wad stands in for a missing config.xml and a refusal names it when the archive has none, a template that is not a client configuration is refused, data.dat is copied only when the install has it, and a rebuild removes the stamp before writing and writes it last.
 */

#include "ClientRunFolder.h"
#include "FakeClientSystem.h"
#include "FakeLauncherFiles.h"
#include "LauncherHarness.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string>

namespace
{
    RunFolderOptions Options()
    {
        RunFolderOptions options;
        options.Folder = "C:/runs/wizard";
        options.Install = LauncherTestData::Install;
        options.Revision = LauncherTestData::Revision;
        options.Width = 1280;
        options.Height = 720;
        return options;
    }

    std::string const* Find(RunFolderPlan const& plan, std::string const& name)
    {
        auto const found = std::find_if(plan.Files.begin(), plan.Files.end(), [&name](RunFolderFile const& file) { return file.Name == name; });
        return found == plan.Files.end() ? nullptr : &found->Text;
    }
}

TEST(LauncherRunFolderTest, KeepsTheInstallsOwnSettingsAndChangesOnlyTheWindowAndTheMetricsUrl)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::string error;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(Options(), harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    EXPECT_EQ(plan->ConfigSource, "Bin/config.xml");
    std::string const* const config = Find(*plan, "config.xml");
    ASSERT_TRUE(config);
    EXPECT_NE(config->find("<IsFullscreen TYPE=\"INT\">0</IsFullscreen>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<SilentMetricsURL TYPE=\"STR\">"), std::string::npos) << *config;
    EXPECT_EQ(config->find("example.invalid"), std::string::npos) << *config;
    EXPECT_NE(config->find("<VersionNumber TYPE=\"UINT\">128</VersionNumber>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<PrefVersionNumber TYPE=\"UINT\">32</PrefVersionNumber>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<QuestHelperEnabled TYPE=\"INT\">1</QuestHelperEnabled>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<UIScale TYPE=\"FLT\">10.000000</UIScale>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<WindowedX TYPE=\"INT\">8</WindowedX>"), std::string::npos) << *config;

    std::string const* const preferences = Find(*plan, "preferences.xml");
    ASSERT_TRUE(preferences);
    EXPECT_NE(preferences->find("<IsFullscreen TYPE=\"INT\">0</IsFullscreen>"), std::string::npos) << *preferences;
    EXPECT_NE(preferences->find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << *preferences;
    EXPECT_NE(preferences->find("<EnableSound TYPE=\"INT\">1</EnableSound>"), std::string::npos) << *preferences;
    EXPECT_EQ(preferences->find("SilentMetricsURL"), std::string::npos) << *preferences;
}

TEST(LauncherRunFolderTest, AddsTheTablesAndKeysATemplateLacks)
{
    LauncherHarness harness;
    harness.AddInstall(true, false);
    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml", "<?xml version=\"1.0\" ?>\n<config>\n</config>\n");
    std::string error;
    RunFolderOptions options = Options();
    options.WindowX = 12;
    options.WindowY = 34;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(options, harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    std::string const* const config = Find(*plan, "config.xml");
    ASSERT_TRUE(config);
    EXPECT_NE(config->find("<Name TYPE=\"STR\">VideoSettings</Name>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<Name TYPE=\"STR\">GameSettings</Name>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<WindowedX TYPE=\"INT\">12</WindowedX>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<WindowedY TYPE=\"INT\">34</WindowedY>"), std::string::npos) << *config;
    std::string const* const preferences = Find(*plan, "preferences.xml");
    ASSERT_TRUE(preferences);
    EXPECT_NE(preferences->find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << *preferences;
}

TEST(LauncherRunFolderTest, ReadsDefaultConfigFromTheArchiveWhenTheInstallHasNoConfiguration)
{
    LauncherHarness harness;
    harness.AddInstall(true, false);
    harness.Files.AddEntry(std::string(LauncherTestData::Install) + "/Data/GameData/Root.wad", "defaultconfig.xml", LauncherTestData::ConfigTemplate);
    std::string error;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(Options(), harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    EXPECT_EQ(plan->ConfigSource, "defaultconfig.xml in Root.wad");
    std::string const* const config = Find(*plan, "config.xml");
    ASSERT_TRUE(config);
    EXPECT_NE(config->find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << *config;
}

TEST(LauncherRunFolderTest, RefusesWhenNeitherTheInstallNorItsArchiveHasAConfiguration)
{
    LauncherHarness harness;
    harness.AddInstall(true, false);
    std::string error;
    EXPECT_FALSE(ClientRunFolder::Build(Options(), harness.System, harness.Files, error));
    EXPECT_NE(error.find("defaultconfig.xml"), std::string::npos) << error;
    EXPECT_NE(error.find("Root.wad"), std::string::npos) << error;
}

TEST(LauncherRunFolderTest, RefusesATemplateThatIsNotAClientConfiguration)
{
    LauncherHarness harness;
    harness.AddInstall(true, false);
    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml", "<?xml version=\"1.0\" ?>\n<settings>\n</settings>\n");
    std::string error;
    EXPECT_FALSE(ClientRunFolder::Build(Options(), harness.System, harness.Files, error));
    EXPECT_NE(error.find("root element is not <config>"), std::string::npos) << error;

    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml", "<config><VideoSettings>");
    EXPECT_FALSE(ClientRunFolder::Build(Options(), harness.System, harness.Files, error));
    EXPECT_NE(error.find("client configuration cannot be built"), std::string::npos) << error;
}

TEST(LauncherRunFolderTest, CopiesTheRevisionAndDataFilesTheClientOpensByName)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::string error;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(Options(), harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    std::string const* const revision = Find(*plan, "revision.dat");
    ASSERT_TRUE(revision);
    EXPECT_EQ(*revision, std::string(LauncherTestData::Revision) + "\n");
    ASSERT_TRUE(Find(*plan, "data.dat"));

    FakeClientSystem bare;
    bare.Environment["ProgramData"] = "C:/ProgramData";
    bare.AddInstall(LauncherTestData::Install, LauncherTestData::Revision);
    bare.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml", LauncherTestData::ConfigTemplate);
    std::optional<RunFolderPlan> const without = ClientRunFolder::Build(Options(), bare, harness.Files, error);
    ASSERT_TRUE(without) << error;
    EXPECT_FALSE(Find(*without, "data.dat"));
}

TEST(LauncherRunFolderTest, ARebuildRemovesTheStampFirstAndWritesItLast)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::string error;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(Options(), harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    ASSERT_TRUE(ClientRunFolder::Write(*plan, harness.Files, error)) << error;
    ASSERT_FALSE(harness.Files.Removed.empty());
    ASSERT_FALSE(harness.Files.Order.empty());
    EXPECT_EQ(harness.Files.Removed.front(), "C:/runs/wizard/launcher.stamp");
    EXPECT_EQ(harness.Files.Order.back(), "C:/runs/wizard/launcher.stamp");
    EXPECT_EQ(harness.Files.Order.front(), "C:/runs/wizard/config.xml");
    EXPECT_EQ(harness.Files.Folders.front(), "C:/runs/wizard");
    EXPECT_NE(harness.Files.Text("C:/runs/wizard/launcher.stamp").find(LauncherTestData::Revision), std::string::npos);
    EXPECT_NE(harness.Files.Text("C:/runs/wizard/launcher.stamp").find("Resolution = 1280x720"), std::string::npos);

    harness.Files.WriteError = "no room is left";
    EXPECT_FALSE(ClientRunFolder::Write(*plan, harness.Files, error));
    EXPECT_EQ(error, "no room is left");
}

TEST(LauncherRunFolderTest, TheStampChangesWithEveryOptionItRecords)
{
    RunFolderOptions const options = Options();
    RunFolderOptions other = options;
    other.Fullscreen = 1;
    EXPECT_NE(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(other));
    other = options;
    other.Height = 900;
    EXPECT_NE(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(other));
    other = options;
    other.Revision = "r900000.Wizard_1_700";
    EXPECT_NE(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(other));
    other = options;
    other.WindowX = 0;
    EXPECT_NE(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(other));
    EXPECT_EQ(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(Options()));
}
