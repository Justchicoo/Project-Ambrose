/*
 * Project Ambrose by Imjustchico
 * Tests the launcher on a described machine without starting anything: discovery finds the newest install of two and a named folder wins over it, a relative one is made absolute for the machine described, the command always carries -L, -P 0, -A, -D with a trailing separator and -G in the run folder, the automatic login and character options pass through with the client's own .. prefix, the run folder comes from the option or the Ambrose data folder and is named the same way on a machine that is not Windows, its configuration is written every run while the copies follow the stamp, nothing outside it is written, its own settings are read from the configuration file unless an option overrides it and a blank one means the default, and every refusal names its cause: no install found, a folder that holds none, a missing client program, patching asked for, no host or port, values that make no sense or begin with '-', a revision that cannot name a folder, a run folder inside the install or one that cannot be written, and a machine that cannot start a Windows program.
 */

#include "ConfigMgr.h"
#include "LauncherHarness.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace
{
    std::string Separator()
    {
        return std::string(1, static_cast<char>(std::filesystem::path::preferred_separator));
    }

    std::string Generic(std::filesystem::path const& path)
    {
        return path.generic_string();
    }
}

TEST(LauncherTest, FindsTheNewestInstallAndBuildsTheWholeCommand)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::optional<LauncherPlan> const plan = harness.Prepare(LauncherRequest{});
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(Generic(plan->Install.Root), LauncherTestData::Install);
    EXPECT_EQ(Generic(plan->Program), std::string(LauncherTestData::Install) + "/Bin/WizardGraphicalClient.exe");
    EXPECT_EQ(Generic(plan->RunFolder), LauncherTestData::RunFolder);
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-L"), "127.0.0.1");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-L", 2), "12000");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-P"), "0");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-A"), "en-US");
    std::string const dataRoot = LauncherHarness::Argument(*plan, "-D");
    EXPECT_NE(dataRoot.find("GameData"), std::string::npos);
    EXPECT_TRUE(dataRoot.ends_with(Separator())) << dataRoot;
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-G"), ConfigMgr::PathToUtf8(plan->RunFolder / "WizardClient.log"));
    EXPECT_NE(plan->Command().find("WizardGraphicalClient.exe"), std::string::npos);
    EXPECT_NE(plan->Command().find("-P 0"), std::string::npos);
    EXPECT_EQ(std::count(plan->Arguments.begin(), plan->Arguments.end(), "-U"), 0);
}

TEST(LauncherTest, EveryOptionReachesTheCommandAndTheRunFolder)
{
    LauncherHarness harness;
    harness.AddInstall();
    LauncherRequest request;
    request.ClientDir = LauncherTestData::Install;
    request.Host = "10.0.0.5";
    request.Port = "12100";
    request.Locale = "de-DE";
    request.Window = "800x600";
    request.Fullscreen = "2";
    request.WindowX = "40";
    request.WindowY = "50";
    request.RunDir = "C:/runs/wizard";
    request.User = ClientLogin{ "17", "session-key", "Iridian Nightbreeze" };
    request.Character = "Iridian Nightbreeze";
    std::optional<LauncherPlan> const plan = harness.Prepare(request);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-L"), "10.0.0.5");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-L", 2), "12100");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-A"), "de-DE");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-U"), "..17");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-U", 2), "session-key");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-U", 3), "Iridian Nightbreeze");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-C"), "Iridian Nightbreeze");
    EXPECT_EQ(Generic(plan->RunFolder), "C:/runs/wizard");
    std::string const config = plan->Folder.Files.front().Text;
    EXPECT_EQ(plan->Folder.Files.front().Name, "config.xml");
    EXPECT_NE(config.find("<IsFullscreen TYPE=\"INT\">2</IsFullscreen>"), std::string::npos) << config;
    EXPECT_NE(config.find("<Resolution TYPE=\"STR\">800x600</Resolution>"), std::string::npos) << config;
    EXPECT_NE(config.find("<WindowedX TYPE=\"INT\">40</WindowedX>"), std::string::npos) << config;
    EXPECT_NE(config.find("<WindowedY TYPE=\"INT\">50</WindowedY>"), std::string::npos) << config;
}

TEST(LauncherTest, TheUserIdKeepsOneClientPrefixAndTheNameIsOptional)
{
    LauncherHarness harness;
    harness.AddInstall();
    LauncherRequest request;
    request.User = ClientLogin{ "..17", "session-key", "" };
    std::optional<LauncherPlan> const plan = harness.Prepare(request);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-U"), "..17");
    EXPECT_EQ(plan->Arguments.back(), "session-key");
}

TEST(LauncherTest, WritesOnlyTheRunFolderAndRewritesItsConfigurationEveryRun)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::optional<LauncherPlan> const plan = harness.Prepare(LauncherRequest{});
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_TRUE(plan->Folder.Rebuild);
    ASSERT_TRUE(harness.Write(*plan)) << harness.Error;
    std::string const folder = std::string(LauncherTestData::RunFolder) + "/";
    ASSERT_FALSE(harness.Files.Written.empty());
    for (auto const& [file, text] : harness.Files.Written)
        EXPECT_TRUE(file.starts_with(folder)) << file;
    EXPECT_TRUE(harness.Files.Has(folder + "config.xml"));
    EXPECT_TRUE(harness.Files.Has(folder + "preferences.xml"));
    EXPECT_TRUE(harness.Files.Has(folder + "revision.dat"));
    EXPECT_TRUE(harness.Files.Has(folder + "data.dat"));
    EXPECT_TRUE(harness.Files.Has(folder + "launcher.stamp"));
    EXPECT_EQ(harness.Files.Text(folder + "revision.dat"), std::string(LauncherTestData::Revision) + "\n");

    harness.System.AddFile(folder + "launcher.stamp", harness.Files.Text(folder + "launcher.stamp"));
    harness.System.AddFile(folder + "config.xml", "<?xml version=\"1.0\" ?>\n<config>\n<VideoSettings>\n  <RECORD>\n    <IsFullscreen TYPE=\"INT\">1</IsFullscreen>\n  </RECORD>\n</VideoSettings>\n</config>\n");
    std::optional<LauncherPlan> const again = harness.Prepare(LauncherRequest{});
    ASSERT_TRUE(again) << harness.Error;
    EXPECT_FALSE(again->Folder.Rebuild);
    harness.Files.Written.clear();
    ASSERT_TRUE(harness.Write(*again)) << harness.Error;
    EXPECT_TRUE(harness.Files.Has(folder + "config.xml"));
    EXPECT_TRUE(harness.Files.Has(folder + "preferences.xml"));
    EXPECT_FALSE(harness.Files.Has(folder + "revision.dat"));
    EXPECT_FALSE(harness.Files.Has(folder + "launcher.stamp"));
    EXPECT_NE(harness.Files.Text(folder + "config.xml").find("<IsFullscreen TYPE=\"INT\">0</IsFullscreen>"), std::string::npos) << harness.Files.Text(folder + "config.xml");
    EXPECT_NE(harness.Files.Text(folder + "config.xml").find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos) << harness.Files.Text(folder + "config.xml");

    LauncherRequest wider;
    wider.Window = "1600x900";
    std::optional<LauncherPlan> const changed = harness.Prepare(wider);
    ASSERT_TRUE(changed) << harness.Error;
    EXPECT_FALSE(changed->Folder.Rebuild);
    harness.Files.Written.clear();
    ASSERT_TRUE(harness.Write(*changed)) << harness.Error;
    EXPECT_NE(harness.Files.Text(folder + "config.xml").find("<Resolution TYPE=\"STR\">1600x900</Resolution>"), std::string::npos) << harness.Files.Text(folder + "config.xml");

    LauncherRequest newer;
    newer.RunDir = LauncherTestData::RunFolder;
    harness.System.AddFile(std::string(LauncherTestData::Install) + "/Bin/revision.dat", "r900000.Wizard_1_700\n");
    std::optional<LauncherPlan> const rebuilt = harness.Prepare(newer);
    ASSERT_TRUE(rebuilt) << harness.Error;
    EXPECT_TRUE(rebuilt->Folder.Rebuild);
}

TEST(LauncherTest, RefusesARunFolderInsideTheInstall)
{
    for (char const* inside : { LauncherTestData::Install, "C:/ProgramData/KingsIsle Entertainment/Wizard101/Bin", "C:/ProgramData/KingsIsle Entertainment/Wizard101/Bin/../Bin/runs" })
    {
        LauncherHarness harness;
        harness.AddInstall();
        LauncherRequest request;
        request.RunDir = inside;
        EXPECT_FALSE(harness.Prepare(request)) << inside;
        EXPECT_NE(harness.Error.find("cannot be inside the install"), std::string::npos) << harness.Error;
        EXPECT_TRUE(harness.Files.Written.empty());
        EXPECT_TRUE(harness.Files.Removed.empty());
        EXPECT_TRUE(harness.Files.Folders.empty());
    }
}

TEST(LauncherTest, RefusesARunFolderThatOnlyReachesTheInstallThroughALink)
{
    LauncherHarness harness;
    harness.AddInstall();
    harness.System.AddLink("C:/runs/wizard", std::string(LauncherTestData::Install) + "/Bin");
    LauncherRequest request;
    request.RunDir = "C:/runs/wizard";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("cannot be inside the install"), std::string::npos) << harness.Error;
    EXPECT_TRUE(harness.Files.Written.empty());
}

TEST(LauncherTest, RefusesAValueThatBeginsWithADashSoNoClientOptionCanBeSmuggledIn)
{
    LauncherHarness harness;
    harness.AddInstall();
    LauncherRequest request;
    request.Character = "-ST";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("character name"), std::string::npos) << harness.Error;
    EXPECT_NE(harness.Error.find("begins with '-'"), std::string::npos) << harness.Error;
    request.Character.reset();
    request.Host = "-ST";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("login server host"), std::string::npos) << harness.Error;
    request.Host.reset();
    request.Locale = "-ST";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("locale"), std::string::npos) << harness.Error;
    request.Locale.reset();
    request.User = ClientLogin{ "17", "-ST", "" };
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("user key"), std::string::npos) << harness.Error;
    request.User = ClientLogin{ "17", "session-key", "-ST" };
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("user name"), std::string::npos) << harness.Error;
    request.User = ClientLogin{ "-ST", "session-key", "" };
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("user id"), std::string::npos) << harness.Error;

    LauncherRequest good;
    good.Character = "Iridian Nightbreeze";
    std::optional<LauncherPlan> const plan = harness.Prepare(good);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(std::count(plan->Arguments.begin(), plan->Arguments.end(), "-ST"), 0);
}

TEST(LauncherTest, ARelativeInstallFolderBecomesAbsoluteBeforeItReachesTheClient)
{
    LauncherHarness harness;
    for (char const* spelling : { "Wizard101", "C:/work/Wizard101" })
    {
        harness.System.AddInstall(spelling, LauncherTestData::Revision);
        harness.System.AddFile(std::string(spelling) + "/Bin/config.xml", LauncherTestData::ConfigTemplate);
    }
    LauncherRequest request;
    request.ClientDir = "Wizard101";
    std::optional<LauncherPlan> const plan = harness.Prepare(request);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(Generic(plan->Install.Root), "C:/work/Wizard101");
    EXPECT_EQ(Generic(plan->Program), "C:/work/Wizard101/Bin/WizardGraphicalClient.exe");
    EXPECT_TRUE(LauncherHarness::Argument(*plan, "-D").starts_with("C:")) << LauncherHarness::Argument(*plan, "-D");
    EXPECT_NE(LauncherHarness::Argument(*plan, "-D").find("work"), std::string::npos) << LauncherHarness::Argument(*plan, "-D");
}

TEST(LauncherTest, AMachineThatIsNotWindowsNamesItsOwnFoldersAndCannotStartTheClient)
{
    LauncherHarness harness;
    harness.System.Windows = false;
    harness.System.Environment.clear();
    harness.System.Environment["HOME"] = "/home/wiz";
    harness.System.Working = "/work";
    harness.System.Executable = "/opt/ambrose/bin";
    harness.System.AddInstall("/home/wiz/games/Wizard101", LauncherTestData::Revision);
    harness.System.AddFile("/home/wiz/games/Wizard101/Bin/config.xml", LauncherTestData::ConfigTemplate);
    LauncherRequest request;
    request.ClientDir = "/home/wiz/games/Wizard101";
    std::optional<LauncherPlan> const plan = harness.Prepare(request);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(Generic(plan->RunFolder), std::string("/home/wiz/.local/share/project-ambrose/client/") + LauncherTestData::Revision);
    request.RunDir = "runs/wizard";
    std::optional<LauncherPlan> const relative = harness.Prepare(request);
    ASSERT_TRUE(relative) << harness.Error;
    EXPECT_EQ(Generic(relative->RunFolder), "/work/runs/wizard");
    request.RunDir = "/runs/wizard";
    std::optional<LauncherPlan> const named = harness.Prepare(request);
    ASSERT_TRUE(named) << harness.Error;
    EXPECT_EQ(Generic(named->RunFolder), "/runs/wizard");

    Launcher const launcher(harness.System, harness.Files, harness.Err);
    std::string error;
    EXPECT_FALSE(launcher.CanStart(error));
    EXPECT_NE(error.find("not Windows"), std::string::npos) << error;
    EXPECT_TRUE(harness.Files.Written.empty());
}

TEST(LauncherTest, AWindowsMachineCanStartTheClient)
{
    LauncherHarness harness;
    Launcher const launcher(harness.System, harness.Files, harness.Err);
    std::string error;
    EXPECT_TRUE(launcher.CanStart(error));
    EXPECT_TRUE(error.empty());
}

TEST(LauncherTest, DiscoveryTakesTheNewestInstallAndANamedFolderWins)
{
    LauncherHarness harness;
    harness.AddInstall();
    harness.System.AddInstall("D:/Games/Wizard101", "r999999.Wizard_1_700");
    harness.System.AddFile("D:/Games/Wizard101/Bin/config.xml", LauncherTestData::ConfigTemplate);
    harness.System.Uninstall.push_back({ "Wizard101", "D:/Games/Wizard101" });
    std::optional<LauncherPlan> const newest = harness.Prepare(LauncherRequest{});
    ASSERT_TRUE(newest) << harness.Error;
    EXPECT_EQ(Generic(newest->Install.Root), "D:/Games/Wizard101");
    EXPECT_EQ(newest->Install.Revision, "r999999.Wizard_1_700");

    LauncherRequest request;
    request.ClientDir = LauncherTestData::Install;
    std::optional<LauncherPlan> const named = harness.Prepare(request);
    ASSERT_TRUE(named) << harness.Error;
    EXPECT_EQ(Generic(named->Install.Root), LauncherTestData::Install);
    EXPECT_EQ(named->Install.Revision, LauncherTestData::Revision);
}

TEST(LauncherTest, TheClientsOwnLogFromAnEarlierRunIsRemoved)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::optional<LauncherPlan> const plan = harness.Prepare(LauncherRequest{});
    ASSERT_TRUE(plan) << harness.Error;
    ASSERT_TRUE(harness.Write(*plan)) << harness.Error;
    EXPECT_NE(std::find(harness.Files.Removed.begin(), harness.Files.Removed.end(), std::string(LauncherTestData::RunFolder) + "/WizardClient.log"), harness.Files.Removed.end());
}

TEST(LauncherTest, RefusesWithoutAnInstall)
{
    LauncherHarness harness;
    EXPECT_FALSE(harness.Prepare(LauncherRequest{}));
    EXPECT_NE(harness.Error.find("no Wizard101 install was found"), std::string::npos) << harness.Error;
}

TEST(LauncherTest, RefusesAFolderThatHoldsNoInstall)
{
    LauncherHarness harness;
    harness.AddInstall();
    LauncherRequest request;
    request.ClientDir = "C:/Games/Empty";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("holds no Wizard101 install"), std::string::npos) << harness.Error;
}

TEST(LauncherTest, RefusesAnInstallWithoutTheClientProgram)
{
    LauncherHarness harness;
    harness.AddInstall(false);
    EXPECT_FALSE(harness.Prepare(LauncherRequest{}));
    EXPECT_NE(harness.Error.find("WizardGraphicalClient.exe"), std::string::npos) << harness.Error;
    EXPECT_NE(harness.Error.find("missing"), std::string::npos) << harness.Error;
}

TEST(LauncherTest, RefusesPatching)
{
    LauncherHarness harness;
    harness.AddInstall();
    LauncherRequest request;
    request.Patch = "1";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("patching is asked for"), std::string::npos) << harness.Error;
    request.Patch = "sometimes";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("is not 0 or 1"), std::string::npos) << harness.Error;
}

TEST(LauncherTest, RefusesAMissingHostOrPort)
{
    LauncherHarness harness;
    harness.AddInstall();
    LauncherRequest request;
    request.Host = "  ";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("no login server host"), std::string::npos) << harness.Error;
    request.Host.reset();
    for (char const* port : { "", "0", "70000", "twelve thousand" })
    {
        request.Port = port;
        EXPECT_FALSE(harness.Prepare(request)) << port;
        EXPECT_NE(harness.Error.find("login port"), std::string::npos) << harness.Error;
    }
}

TEST(LauncherTest, RefusesValuesThatMakeNoSense)
{
    LauncherHarness harness;
    harness.AddInstall();
    LauncherRequest request;
    request.Window = "1280";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("window size"), std::string::npos) << harness.Error;
    request.Window = "40x30";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("window size"), std::string::npos) << harness.Error;
    request.Window.reset();
    request.Fullscreen = "3";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("window mode"), std::string::npos) << harness.Error;
    request.Fullscreen.reset();
    request.WindowX = "over there";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("window position across"), std::string::npos) << harness.Error;
    request.WindowX.reset();
    request.Locale = "en US";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("locale cannot hold a space"), std::string::npos) << harness.Error;
    request.Locale.reset();
    request.User = ClientLogin{ "17", "", "" };
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("user key"), std::string::npos) << harness.Error;
}

TEST(LauncherTest, RefusesWhenTheRunFolderCannotBeNamedOrWritten)
{
    LauncherHarness harness;
    harness.AddInstall();
    harness.System.Environment.erase("LOCALAPPDATA");
    EXPECT_FALSE(harness.Prepare(LauncherRequest{}));
    EXPECT_NE(harness.Error.find("--run-dir"), std::string::npos) << harness.Error;

    harness.System.Environment["LOCALAPPDATA"] = "C:/Users/wiz/AppData/Local";
    std::optional<LauncherPlan> const plan = harness.Prepare(LauncherRequest{});
    ASSERT_TRUE(plan) << harness.Error;
    harness.Files.FolderError = "the disk is read-only";
    EXPECT_FALSE(harness.Write(*plan));
    EXPECT_NE(harness.Error.find("cannot be written"), std::string::npos) << harness.Error;
    EXPECT_NE(harness.Error.find("read-only"), std::string::npos) << harness.Error;
    harness.Files.FolderError.clear();
    harness.Files.WriteError = "no room is left";
    EXPECT_FALSE(harness.Write(*plan));
    EXPECT_NE(harness.Error.find("no room is left"), std::string::npos) << harness.Error;
}

TEST(LauncherTest, RefusesToNameTheRunFolderAfterARevisionThatIsNotAFolderName)
{
    LauncherHarness harness;
    harness.System.AddInstall("C:/Games/Wizard101", "../../elsewhere");
    harness.System.AddFile("C:/Games/Wizard101/Bin/config.xml", LauncherTestData::ConfigTemplate);
    LauncherRequest request;
    request.ClientDir = "C:/Games/Wizard101";
    EXPECT_FALSE(harness.Prepare(request));
    EXPECT_NE(harness.Error.find("cannot name a folder"), std::string::npos) << harness.Error;
    request.RunDir = "C:/runs/wizard";
    std::optional<LauncherPlan> const plan = harness.Prepare(request);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(Generic(plan->RunFolder), "C:/runs/wizard");
}

TEST(LauncherTest, ARelativeRunFolderSitsUnderTheWorkingFolder)
{
    LauncherHarness harness;
    harness.AddInstall();
    LauncherRequest request;
    request.RunDir = "runs/wizard";
    std::optional<LauncherPlan> const plan = harness.Prepare(request);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(Generic(plan->RunFolder), "C:/work/runs/wizard");
}

TEST(LauncherTest, SettingsComeFromTheConfigurationUnlessAnOptionOverridesThem)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "launcher.conf";
    std::ofstream(file, std::ios::binary) << "# Project Ambrose by Imjustchico\n# Settings for a launcher test.\n"
        "ClientDir = C:/Games/Wizard101\nLoginHost = 192.168.1.9\nLoginPort = 12200\nLocale = fr-FR\nWindow = 1024x768\nFullscreen = 1\nPatch = 0\n";
    ConfigMgr config([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    LauncherRequest request;
    request.Host = "127.0.0.2";
    Launcher::FromConfig(config, request);
    EXPECT_EQ(request.Host, "127.0.0.2");
    EXPECT_EQ(request.ClientDir, "C:/Games/Wizard101");
    EXPECT_EQ(request.Port, "12200");
    EXPECT_EQ(request.Locale, "fr-FR");
    EXPECT_EQ(request.Window, "1024x768");
    EXPECT_EQ(request.Fullscreen, "1");
    EXPECT_EQ(request.Patch, "0");
    EXPECT_FALSE(request.RunDir.has_value());
}

TEST(LauncherTest, ASettingLeftBlankInTheFileMeansItsDefault)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "launcher.conf";
    std::ofstream(file, std::ios::binary) << "# Project Ambrose by Imjustchico\n# Settings for a launcher test.\n"
        "ClientDir =\nLoginHost =\nLoginPort =\nLocale =\nWindow =\nFullscreen =\nWindowX =\nWindowY =\nRunDir =\nPatch =\n";
    ConfigMgr config([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    LauncherRequest request;
    Launcher::FromConfig(config, request);
    EXPECT_FALSE(request.Host.has_value());
    EXPECT_FALSE(request.Port.has_value());
    EXPECT_FALSE(request.Locale.has_value());
    EXPECT_FALSE(request.Window.has_value());
    EXPECT_FALSE(request.Fullscreen.has_value());
    EXPECT_FALSE(request.Patch.has_value());

    LauncherHarness harness;
    harness.AddInstall();
    std::optional<LauncherPlan> const plan = harness.Prepare(request);
    ASSERT_TRUE(plan) << harness.Error;
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-L"), "127.0.0.1");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-L", 2), "12000");
    EXPECT_EQ(LauncherHarness::Argument(*plan, "-A"), "en-US");
    EXPECT_EQ(Generic(plan->RunFolder), LauncherTestData::RunFolder);
    EXPECT_NE(plan->Folder.Files.front().Text.find("<Resolution TYPE=\"STR\">1280x720</Resolution>"), std::string::npos);
}

TEST(LauncherTest, AQuotedCommandKeepsAPathWithSpacesInOnePiece)
{
    LauncherHarness harness;
    harness.AddInstall();
    std::optional<LauncherPlan> const plan = harness.Prepare(LauncherRequest{});
    ASSERT_TRUE(plan) << harness.Error;
    std::string const command = plan->Command();
    EXPECT_NE(command.find("KingsIsle Entertainment"), std::string::npos) << command;
    EXPECT_TRUE(command.find('"') != std::string::npos || command.find('\'') != std::string::npos) << command;
}
