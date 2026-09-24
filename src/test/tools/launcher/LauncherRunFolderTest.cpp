/*
 * Project Ambrose by Imjustchico
 * Tests the folder the client runs from: the configuration is written from the install's own config.xml with only the window keys changed and SilentMetricsURL emptied in both files, the install's VersionInfo and its other settings are kept, preferences carry the same window, a table or key the template lacks is added and listed only when the template lists its tables, defaultconfig.xml in Root.wad stands in for a missing config.xml with its root element renamed and a refusal names it when the archive has none, a template that is not a client configuration is refused, data.dat is copied only when the install has it, the stamp follows the install and its revision and not the window, the configuration is written every run from what the folder already holds and again from the install when the folder lost it, and a rebuild removes the stamp before writing and writes it last.
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

    std::string const* FindCopy(RunFolderPlan const& plan, std::string const& name)
    {
        auto const found = std::find_if(plan.Copies.begin(), plan.Copies.end(), [&name](RunFolderFile const& file) { return file.Name == name; });
        return found == plan.Copies.end() ? nullptr : &found->Text;
    }

    constexpr char const* DefaultConfigTemplate = R"(<?xml version="1.0" ?>
<defaultconfig>
  <VersionInfo>
    <RECORD>
      <VersionNumber TYPE="UINT">128</VersionNumber>
      <PrefVersionNumber TYPE="UINT">32</PrefVersionNumber>
    </RECORD>
  </VersionInfo>
  <VideoSettings>
    <RECORD>
      <IsFullscreen TYPE="INT" UITYPE="LIST" UIDATA="1:Off,On,On Borderless">1</IsFullscreen>
      <Resolution TYPE="STR" UITYPE="LIST" UIDATA="1:autodetect">autodetect</Resolution>
    </RECORD>
  </VideoSettings>
  <GameSettings>
    <RECORD>
      <SilentMetricsURL TYPE="STR">https://www.example.invalid/static/noop.html</SilentMetricsURL>
    </RECORD>
  </GameSettings>
  <DebugSettings>
    <RECORD>
      <TraceAnim TYPE="INT">0</TraceAnim>
    </RECORD>
  </DebugSettings>
</defaultconfig>
)";
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
    EXPECT_NE(config->find("<VideoSettings>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<WindowedX TYPE=\"INT\">12</WindowedX>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<WindowedY TYPE=\"INT\">34</WindowedY>"), std::string::npos) << *config;
    EXPECT_EQ(config->find("_TableList"), std::string::npos) << *config;
    std::string const* const preferences = Find(*plan, "preferences.xml");
    ASSERT_TRUE(preferences);
    EXPECT_NE(preferences->find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << *preferences;
}

TEST(LauncherRunFolderTest, ATemplateThatListsItsTablesAlsoListsTheOnesAdded)
{
    LauncherHarness harness;
    harness.AddInstall(true, false);
    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml",
        "<?xml version=\"1.0\" ?>\n<config>\n<_TableList>\n  <RECORD>\n    <Name TYPE=\"STR\">VersionInfo</Name>\n  </RECORD>\n</_TableList>\n"
        "<VersionInfo>\n  <RECORD>\n    <VersionNumber TYPE=\"UINT\">128</VersionNumber>\n  </RECORD>\n</VersionInfo>\n</config>\n");
    std::string error;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(Options(), harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    std::string const* const config = Find(*plan, "config.xml");
    ASSERT_TRUE(config);
    EXPECT_NE(config->find("<Name TYPE=\"STR\">VideoSettings</Name>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<Name TYPE=\"STR\">GameSettings</Name>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<Name TYPE=\"STR\">VersionInfo</Name>"), std::string::npos) << *config;
}

TEST(LauncherRunFolderTest, ReadsDefaultConfigFromTheArchiveWhenTheInstallHasNoConfiguration)
{
    LauncherHarness harness;
    harness.AddInstall(true, false);
    harness.Files.AddEntry(std::string(LauncherTestData::Install) + "/Data/GameData/Root.wad", "defaultconfig.xml", DefaultConfigTemplate);
    std::string error;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(Options(), harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    EXPECT_EQ(plan->ConfigSource, "defaultconfig.xml in Root.wad");
    std::string const* const config = Find(*plan, "config.xml");
    ASSERT_TRUE(config);
    EXPECT_NE(config->find("<config>"), std::string::npos) << *config;
    EXPECT_EQ(config->find("<defaultconfig>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<Resolution TYPE=\"STR\""), std::string::npos) << *config;
    EXPECT_NE(config->find(">1280x720</Resolution>"), std::string::npos) << *config;
    EXPECT_NE(config->find(">0</IsFullscreen>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>"), std::string::npos) << *config;
    EXPECT_EQ(config->find("noop.html"), std::string::npos) << *config;
    EXPECT_NE(config->find("<TraceAnim TYPE=\"INT\">0</TraceAnim>"), std::string::npos) << *config;
    EXPECT_EQ(config->find("_TableList"), std::string::npos) << *config;
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
    std::string const* const revision = FindCopy(*plan, "revision.dat");
    ASSERT_TRUE(revision);
    EXPECT_EQ(*revision, std::string(LauncherTestData::Revision) + "\n");
    ASSERT_TRUE(FindCopy(*plan, "data.dat"));

    FakeClientSystem bare;
    bare.Environment["ProgramData"] = "C:/ProgramData";
    bare.AddInstall(LauncherTestData::Install, LauncherTestData::Revision);
    bare.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml", LauncherTestData::ConfigTemplate);
    std::optional<RunFolderPlan> const without = ClientRunFolder::Build(Options(), bare, harness.Files, error);
    ASSERT_TRUE(without) << error;
    EXPECT_FALSE(FindCopy(*without, "data.dat"));
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
    EXPECT_NE(harness.Files.Text("C:/runs/wizard/launcher.stamp").find(LauncherTestData::Install), std::string::npos);

    harness.Files.WriteError = "no room is left";
    EXPECT_FALSE(ClientRunFolder::Write(*plan, harness.Files, error));
    EXPECT_EQ(error, "no room is left");
}

TEST(LauncherRunFolderTest, TheStampChangesWithTheInstallAndItsRevisionOnly)
{
    RunFolderOptions const options = Options();
    RunFolderOptions other = options;
    other.Revision = "r900000.Wizard_1_700";
    EXPECT_NE(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(other));
    other = options;
    other.Install = "C:/Games/Wizard101";
    EXPECT_NE(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(other));
    other = options;
    other.Fullscreen = 1;
    other.Height = 900;
    other.WindowX = 0;
    EXPECT_EQ(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(other));
    EXPECT_EQ(ClientRunFolder::Stamp(options), ClientRunFolder::Stamp(Options()));
}

TEST(LauncherRunFolderTest, TheConfigurationIsWrittenEveryRunFromWhatTheFolderHoldsAndKeepsTheWindowAndTheEmptyMetricsUrl)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::string error;
    RunFolderOptions const options = Options();
    harness.System.AddFile("C:/runs/wizard/launcher.stamp", ClientRunFolder::Stamp(options));
    harness.System.AddFile("C:/runs/wizard/config.xml", R"(<?xml version="1.0" ?>
<config>
<GameSettings>
  <RECORD>
    <QuestHelperEnabled TYPE="INT">0</QuestHelperEnabled>
    <SilentMetricsURL TYPE="STR">https://www.example.invalid/static/noop.html</SilentMetricsURL>
  </RECORD>
</GameSettings>
<VideoSettings>
  <RECORD>
    <IsFullscreen TYPE="INT">1</IsFullscreen>
    <Resolution TYPE="STR">1920x1080</Resolution>
  </RECORD>
</VideoSettings>
</config>
)");
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(options, harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    EXPECT_FALSE(plan->Rebuild);
    EXPECT_EQ(plan->ConfigSource, "the config.xml the folder already holds");
    std::string const* const config = Find(*plan, "config.xml");
    ASSERT_TRUE(config);
    EXPECT_NE(config->find("<IsFullscreen TYPE=\"INT\">0</IsFullscreen>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << *config;
    EXPECT_NE(config->find("<SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>"), std::string::npos) << *config;
    EXPECT_EQ(config->find("noop.html"), std::string::npos) << *config;
    EXPECT_NE(config->find("<QuestHelperEnabled TYPE=\"INT\">0</QuestHelperEnabled>"), std::string::npos) << *config;

    ASSERT_TRUE(ClientRunFolder::Write(*plan, harness.Files, error)) << error;
    EXPECT_TRUE(harness.Files.Has("C:/runs/wizard/config.xml"));
    EXPECT_TRUE(harness.Files.Has("C:/runs/wizard/preferences.xml"));
    EXPECT_FALSE(harness.Files.Has("C:/runs/wizard/revision.dat"));
    EXPECT_FALSE(harness.Files.Has("C:/runs/wizard/launcher.stamp"));
    EXPECT_TRUE(harness.Files.Removed.empty());
}

TEST(LauncherRunFolderTest, AStampWithoutItsFilesStillWritesTheConfigurationFromTheInstall)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::string error;
    RunFolderOptions const options = Options();
    harness.System.AddFile("C:/runs/wizard/launcher.stamp", ClientRunFolder::Stamp(options));
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(options, harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    EXPECT_FALSE(plan->Rebuild);
    EXPECT_EQ(plan->ConfigSource, "Bin/config.xml");
    ASSERT_TRUE(ClientRunFolder::Write(*plan, harness.Files, error)) << error;
    EXPECT_NE(harness.Files.Text("C:/runs/wizard/config.xml").find("<SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>"), std::string::npos);
    EXPECT_NE(harness.Files.Text("C:/runs/wizard/config.xml").find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos);
    EXPECT_TRUE(harness.Files.Has("C:/runs/wizard/preferences.xml"));
}

TEST(LauncherRunFolderTest, AFileTheClientLeftUnreadableCostsTheNextTemplateAndNotTheRun)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::string error;
    RunFolderOptions const options = Options();
    harness.System.AddFile("C:/runs/wizard/launcher.stamp", ClientRunFolder::Stamp(options));
    harness.System.AddFile("C:/runs/wizard/config.xml", "<config><VideoSettings>");
    harness.System.AddFile("C:/runs/wizard/preferences.xml", "<preferences><VideoSettings>");
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(options, harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    EXPECT_EQ(plan->ConfigSource, "Bin/config.xml");
    EXPECT_EQ(plan->PreferencesSource, "Bin/preferences.xml");
    std::string const* const config = Find(*plan, "config.xml");
    ASSERT_TRUE(config);
    EXPECT_NE(config->find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << *config;
    ASSERT_TRUE(Find(*plan, "preferences.xml"));
}

TEST(LauncherRunFolderTest, AnInstallWhosePreferencesCannotBeReadFallsBackToAnEmptyOne)
{
    LauncherHarness harness;
    harness.AddInstall(true, false);
    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml", LauncherTestData::ConfigTemplate);
    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/preferences.xml", "<preferences><VideoSettings>");
    std::string error;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(Options(), harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    EXPECT_EQ(plan->PreferencesSource, "an empty one");
    std::string const* const preferences = Find(*plan, "preferences.xml");
    ASSERT_TRUE(preferences);
    EXPECT_NE(preferences->find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << *preferences;
}

TEST(LauncherRunFolderTest, AMetricsUrlInTheInstallsPreferencesComesOutEmpty)
{
    LauncherHarness harness;
    harness.AddInstall(true, false);
    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/config.xml", LauncherTestData::ConfigTemplate);
    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/preferences.xml", R"(<?xml version="1.0" ?>
<preferences>
<GameSettings>
  <RECORD>
    <SilentMetricsURL TYPE="STR">https://www.example.invalid/static/noop.html</SilentMetricsURL>
  </RECORD>
</GameSettings>
</preferences>
)");
    std::string error;
    std::optional<RunFolderPlan> const plan = ClientRunFolder::Build(Options(), harness.System, harness.Files, error);
    ASSERT_TRUE(plan) << error;
    std::string const* const preferences = Find(*plan, "preferences.xml");
    ASSERT_TRUE(preferences);
    EXPECT_NE(preferences->find("<SilentMetricsURL TYPE=\"STR\"></SilentMetricsURL>"), std::string::npos) << *preferences;
    EXPECT_EQ(preferences->find("noop.html"), std::string::npos) << *preferences;
}
