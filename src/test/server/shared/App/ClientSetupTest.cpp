/*
 * Project Ambrose by Imjustchico
 * Tests guided setup on a described machine with scripted answers and a real configuration folder: a server with no install or type dump chooses the ones found, saves them to conf.d/client-data.conf, which merges with what the file held and quotes paths safely, and reads them back after the reload; a typed path is checked before it is taken; keys set by the environment or the command line and Setup.Discover = 0 leave everything alone; without a terminal the finds are reported with the setting to add; and tools fill their --client and --type-dump values the same way or print what they found.
 */

#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "FakeClientSystem.h"
#include "LogTestDirectory.h"
#include "ScriptedPromptInput.h"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr char const* Header = "# Project Ambrose by Imjustchico\n# Settings for a guided setup test.\n";
    constexpr char const* Install = "C:/ProgramData/KingsIsle Entertainment/Wizard101";
    constexpr char const* Dump = "C:/ProgramData/KingsIsle Entertainment/Wizard101/r806919.Wizard_1_610.json";

    struct SetupHarness
    {
        LogTestDirectory Directory;
        std::filesystem::path File;
        std::map<std::string, std::string> Environment;
        std::unique_ptr<ConfigMgr> Config;
        FakeClientSystem System;
        std::shared_ptr<ScriptedPromptInput::Counters> Counters = std::make_shared<ScriptedPromptInput::Counters>();
        std::ostringstream Out;
        std::vector<std::pair<bool, std::string>> Reports;

        explicit SetupHarness(std::string const& body, std::vector<std::pair<std::string, std::string>> overrides = {})
        {
            File = Directory.Path() / "gameserver.conf";
            std::ofstream(File, std::ios::binary) << Header << body;
            Config = std::make_unique<ConfigMgr>([this](std::string const& name) -> std::optional<std::string>
            {
                auto const found = Environment.find(name);
                return found == Environment.end() ? std::nullopt : std::optional<std::string>(found->second);
            });
            EXPECT_TRUE(Config->LoadInitial(File, {}, std::move(overrides)).Succeeded());
            System.Environment["ProgramData"] = "C:/ProgramData";
            System.AddInstall(Install, "r806919.Wizard_1_610");
            System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
        }

        ClientSetupResult Run(std::vector<std::string> answers, bool interactive = true)
        {
            SetupPrompt prompt(std::make_unique<ScriptedPromptInput>(std::move(answers), true, Counters), Out, interactive, std::chrono::seconds(30));
            return ClientSetup::ForServer(*Config, prompt, System, { "gameserver", true, true }, [this](bool warning, std::string const& text) { Reports.emplace_back(warning, text); });
        }

        std::string Saved() const
        {
            std::ifstream stream(Directory.Path() / "conf.d" / "client-data.conf", std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), {});
        }
    };
}

TEST(ClientSetupTest, AServerChoosesFoundClientDataAndSavesIt)
{
    SetupHarness setup("ClientDir =\nTypeDumpPath =\n");
    ClientSetupResult const result = setup.Run({ "", "" });
    ASSERT_EQ(result.Saved.size(), 2u);
    EXPECT_EQ(result.Saved[0], (std::pair<std::string, std::string>{ "ClientDir", Install }));
    EXPECT_EQ(result.Saved[1], (std::pair<std::string, std::string>{ "TypeDumpPath", Dump }));
    EXPECT_EQ(setup.Config->GetOption<std::string>("ClientDir", "", true), Install);
    EXPECT_EQ(setup.Config->GetOption<std::string>("TypeDumpPath", "", true), Dump);
    EXPECT_EQ(setup.Config->Resolve("ClientDir")->Kind, ConfigSourceKind::ModuleConfig);
    EXPECT_EQ(setup.Saved(), std::string("# Project Ambrose by Imjustchico\n# The client data paths guided setup found on this machine and you chose; edit or delete this file to choose again.\n")
        + "ClientDir = \"" + Install + "\"\nTypeDumpPath = \"" + Dump + "\"\n");
    EXPECT_NE(setup.Out.str().find("gameserver needs your own Wizard101 install, and ClientDir is not set. Found on this machine:\n  1) C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), found through KingsIsle's default folder\n"), std::string::npos);
    ASSERT_EQ(setup.Reports.size(), 2u);
    EXPECT_FALSE(setup.Reports[0].first);
    EXPECT_NE(setup.Reports[0].second.find("Saved ClientDir = C:/ProgramData/KingsIsle Entertainment/Wizard101 to "), std::string::npos);

    SetupHarness again("ClientDir = \"" + std::string(Install) + "\"\nTypeDumpPath = \"" + Dump + "\"\n");
    EXPECT_TRUE(again.Run({}).Saved.empty());
    EXPECT_EQ(again.Counters->Reads.load(), 0);
    EXPECT_EQ(again.Out.str(), "");
}

TEST(ClientSetupTest, TypedPathsAreCheckedAndSavedFilesMergeAndQuote)
{
    SetupHarness setup("ClientDir = C:/Nowhere\nTypeDumpPath =\n");
    std::filesystem::create_directories(setup.Directory.Path() / "conf.d");
    std::ofstream(setup.Directory.Path() / "conf.d" / "client-data.conf", std::ios::binary) << Header << "Keep.Me = 7\nTypeDumpPath = C:/Stale\n";
    ASSERT_TRUE(setup.Config->Reload().Succeeded());
    setup.System.AddInstall("D:/Quoted \"Wiz 101\"", "r900000.Wizard_1_700");
    ClientSetupResult const result = setup.Run({ "E:/Empty", "D:/Quoted \"Wiz 101\"", "s" });
    ASSERT_EQ(result.Saved.size(), 1u);
    EXPECT_NE(setup.Out.str().find("E:/Empty holds no Wizard101 install: there is no Data/GameData/Root.wad in it."), std::string::npos);
    EXPECT_NE(setup.Out.str().find("not the pinned r806919, so its messages and types may not match"), std::string::npos);
    EXPECT_NE(setup.Out.str().find("ClientDir C:/Nowhere holds no Wizard101 install"), std::string::npos);
    EXPECT_NE(setup.Out.str().find("TypeDumpPath C:/Stale is not a type dump"), std::string::npos);
    EXPECT_EQ(setup.Config->GetOption<std::string>("ClientDir", "", true), "D:/Quoted \"Wiz 101\"");
    EXPECT_EQ(setup.Config->GetOption<uint32>("Keep.Me", 0, true), 7u);
    EXPECT_NE(setup.Saved().find("ClientDir = \"D:/Quoted \\\"Wiz 101\\\"\"\nKeep.Me = \"7\"\nTypeDumpPath = \"C:/Stale\"\n"), std::string::npos);

    std::filesystem::path savedTo;
    std::string error;
    std::string const awkward = "C:\\Games\\\"W\"\\101";
    ASSERT_TRUE(ClientSetup::Save(setup.File, { { "ClientDir", awkward } }, savedTo, error)) << error;
    ASSERT_TRUE(setup.Config->Reload().Succeeded());
    EXPECT_EQ(setup.Config->GetOption<std::string>("ClientDir", "", true), awkward);
    EXPECT_FALSE(std::filesystem::exists(savedTo.string() + ".partial"));
}

TEST(ClientSetupTest, LockedKeysDisabledDiscoveryAndNoTerminalLeaveTheConfigurationAlone)
{
    SetupHarness locked("ClientDir =\nTypeDumpPath =\n", { { "ClientDir", "" } });
    locked.Environment["AMBROSE_TYPE_DUMP_PATH"] = "C:/Missing.json";
    EXPECT_TRUE(locked.Run({ "", "" }).Saved.empty());
    EXPECT_EQ(locked.Counters->Reads.load(), 0);
    EXPECT_FALSE(std::filesystem::exists(locked.Directory.Path() / "conf.d"));

    SetupHarness disabled("ClientDir =\nTypeDumpPath =\nSetup.Discover = 0\n");
    EXPECT_TRUE(disabled.Run({ "", "" }).Saved.empty());
    EXPECT_EQ(disabled.Counters->Reads.load(), 0);
    EXPECT_TRUE(disabled.Reports.empty());

    SetupHarness quiet("ClientDir =\nTypeDumpPath =\n");
    ClientSetupResult const result = quiet.Run({ "", "" }, false);
    EXPECT_TRUE(result.Saved.empty());
    EXPECT_EQ(result.Installs.size(), 1u);
    EXPECT_EQ(quiet.Counters->Reads.load(), 0);
    ASSERT_EQ(quiet.Reports.size(), 2u);
    EXPECT_TRUE(quiet.Reports[0].first);
    EXPECT_EQ(quiet.Reports[0].second, "ClientDir is not set, and Wizard101 was found on this machine: C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), found through KingsIsle's default folder. Start gameserver in a terminal to choose one, or set ClientDir in conf.d/client-data.conf");
    EXPECT_NE(quiet.Reports[1].second.find("TypeDumpPath is not set, and a type dump was found on this machine: " + std::string(Dump)), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(quiet.Directory.Path() / "conf.d"));
}

TEST(ClientSetupTest, ToolsFillTheirValuesOrPrintWhatTheyFound)
{
    FakeClientSystem system;
    system.Environment["ProgramData"] = "C:/ProgramData";
    system.AddInstall(Install, "r806919.Wizard_1_610");
    system.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    auto const counters = std::make_shared<ScriptedPromptInput::Counters>();

    std::ostringstream out;
    std::ostringstream err;
    SetupPrompt interactive(std::make_unique<ScriptedPromptInput>(std::vector<std::string>{ "1", "" }, true, counters), out, true, std::chrono::seconds(30));
    std::optional<std::string> client;
    std::optional<std::string> dump;
    ClientSetup::ForTool(client, &dump, interactive, system, "extractor", err);
    EXPECT_EQ(client, Install);
    EXPECT_EQ(dump, Dump);
    EXPECT_EQ(err.str(), "");

    SetupPrompt quiet(nullptr, out, false, std::chrono::seconds(30));
    std::optional<std::string> noClient;
    std::optional<std::string> noDump;
    ClientSetup::ForTool(noClient, &noDump, quiet, system, "extractor", err);
    EXPECT_FALSE(noClient);
    EXPECT_FALSE(noDump);
    EXPECT_NE(err.str().find("extractor: Wizard101 was found on this machine: C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), found through KingsIsle's default folder. Pass --client with one of them"), std::string::npos);
    EXPECT_NE(err.str().find("extractor: a type dump was found on this machine: " + std::string(Dump)), std::string::npos);

    std::ostringstream off;
    system.Environment["AMBROSE_SETUP_DISCOVER"] = "0";
    ClientSetup::ForTool(noClient, nullptr, quiet, system, "localetool", off);
    EXPECT_EQ(off.str(), "");
    EXPECT_EQ(ClientSetup::ConfigPath("C:/a/./b/../c"), "C:/a/c");
}
