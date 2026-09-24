/*
 * Project Ambrose by Imjustchico
 * Tests setup on a described machine with a real configuration folder and a fake type dump builder: auto picks the newest install, saves only ClientDir and uses the built dump, keeps usable and locked values, replaces an unusable dump, reports builder failures and machines without an install, and uses the newest install for this run only when ClientDir names a folder without one; no server saves an install without a usable dump, so a game server's failed build cannot stop the login server sharing its conf.d, and the login server does not use an install setup picked without one either; a TypeDumpPath set empty on the command line is left alone; ask asks the 3.20 questions, uses a current built dump without asking, offers a build when no dump found is named for the install and the finds only when there is something to offer, with advice by cause otherwise; off searches, saves and builds nothing; typed paths lose quotes and expand ~; on Windows, found paths that are not valid Unicode are skipped instead of throwing; a saved value another conf.d file overrides, or one that overrides the app's own file, is reported; saving merges line by line, escapes, refuses files it cannot rewrite safely and rolls back when the reload fails; tools follow AMBROSE_SETUP_MODE; and the built dump provider looks up a current dump without building.
 */

#include "ClientSetup.h"
#include "ConfigMgr.h"
#include "FakeClientSystem.h"
#include "LogTestDirectory.h"
#include "ScriptedPromptInput.h"
#include "TypeDumpCache.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr char const* Header = "# Project Ambrose by Imjustchico\n# Settings for a setup test.\n";
    constexpr char const* SavedHeader = "# Project Ambrose by Imjustchico\n# The client data paths setup chose on this machine; edit or delete this file to choose again.\n";
    constexpr char const* Install = "C:/ProgramData/KingsIsle Entertainment/Wizard101";
    constexpr char const* Newer = "D:/Games/Wizard101";
    constexpr char const* Dump = "C:/ProgramData/KingsIsle Entertainment/Wizard101/r806919.Wizard_1_610.json";
    constexpr char const* Built = "C:/Users/wiz/AppData/Local/ProjectAmbrose/types/r900000.Wizard_1_700.json";

    struct FakeBuilder
    {
        std::vector<std::string> Calls;
        std::vector<std::string> Lookups;
        std::optional<std::filesystem::path> Result = std::filesystem::path(Built);
        std::optional<std::filesystem::path> Current;
        std::string Error = "typeextract exited with 1";

        ClientSetup::TypeDumpProvider Provider()
        {
            return [this](ClientInstall const& install, TypeDumpBuild build, std::string& error) -> std::optional<std::filesystem::path>
            {
                if (build == TypeDumpBuild::Never)
                {
                    Lookups.push_back(ClientLocator::PathText(install.Root));
                    if (!Current)
                        error = "no current type dump";
                    return Current;
                }
                Calls.push_back(ClientLocator::PathText(install.Root));
                if (!Result)
                    error = Error;
                return Result;
            };
        }
    };

    struct SetupHarness
    {
        LogTestDirectory Directory;
        std::filesystem::path File;
        std::map<std::string, std::string> Environment;
        std::unique_ptr<ConfigMgr> Config;
        FakeClientSystem System;
        ClientSystem const* Machine = nullptr;
        FakeBuilder Builder;
        std::shared_ptr<ScriptedPromptInput::Counters> Counters = std::make_shared<ScriptedPromptInput::Counters>();
        std::ostringstream Out;
        std::vector<std::pair<bool, std::string>> Reports;

        explicit SetupHarness(std::string const& body, std::vector<std::pair<std::string, std::string>> overrides = {}, bool withInstall = true)
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
            if (withInstall)
                System.AddInstall(Install, "r806919.Wizard_1_610");
        }

        void AddNewer()
        {
            System.AddInstall(Newer, "r900000.Wizard_1_700");
            System.Uninstall.push_back({ "Wizard101", Newer });
        }

        ClientSetupResult Run(std::vector<std::string> answers, bool interactive = true, ClientSetupRequest const& request = { "gameserver", true, true, false }, bool closeAtEnd = true)
        {
            SetupPrompt prompt(std::make_unique<ScriptedPromptInput>(std::move(answers), closeAtEnd, Counters), Out, interactive, std::chrono::seconds(30));
            return ClientSetup::ForServer(*Config, prompt, Machine ? *Machine : System, Builder.Provider(), request,[this](bool warning, std::string const& text) { Reports.emplace_back(warning, text); });
        }

        std::filesystem::path SavedPath() const
        {
            return Directory.Path() / "conf.d" / "client-data.conf";
        }

        std::string Saved() const
        {
            std::ifstream stream(SavedPath(), std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), {});
        }

        bool Reported(bool warning, std::string const& text) const
        {
            return std::any_of(Reports.begin(), Reports.end(), [&](auto const& report) { return report.first == warning && report.second.find(text) != std::string::npos; });
        }

        std::string AllReports() const
        {
            std::string text;
            for (auto const& [warning, line] : Reports)
                text += (warning ? "warning: " : "info: ") + line + "\n";
            return text;
        }

        std::size_t Warnings() const
        {
            return static_cast<std::size_t>(std::count_if(Reports.begin(), Reports.end(), [](auto const& report) { return report.first; }));
        }
    };

    ClientCandidate Candidate(std::string const& root, std::string const& revision, bool program)
    {
        ClientCandidate candidate;
        candidate.Install.Root = root;
        candidate.Install.Revision = revision;
        candidate.Install.HasProgram = program;
        candidate.Source = "a test";
        return candidate;
    }

#ifdef _WIN32
    class UnpairedSurrogateSystem final : public ClientSystem
    {
    public:
        FakeClientSystem Base;
        std::filesystem::path Parent = std::filesystem::path(L"D:/Games");
        std::filesystem::path Odd = std::filesystem::path(std::wstring(L"D:/Games/W") + static_cast<wchar_t>(0xD800));
        std::filesystem::path OddDump = std::filesystem::path(std::wstring(L"C:/Users/wiz/AppData/Local/ProjectAmbrose/x") + static_cast<wchar_t>(0xD800) + L".json");

        bool IsWindows() const override { return true; }
        std::optional<std::string> GetEnv(std::string const& name) const override { return Base.GetEnv(name); }

        bool IsFile(std::filesystem::path const& path) const override
        {
            if (!HasSurrogate(path))
                return Base.IsFile(path);
            std::optional<std::wstring> const relative = Inside(path, Odd);
            return Generic(path) == Generic(OddDump) || (relative && (*relative == L"Data/GameData/Root.wad" || *relative == L"Bin/revision.dat" || *relative == L"Bin/WizardGraphicalClient.exe"));
        }

        bool IsDirectory(std::filesystem::path const& path) const override
        {
            if (!HasSurrogate(path))
                return Base.IsDirectory(path);
            std::optional<std::wstring> const relative = Inside(path, Odd);
            return Generic(path) == Generic(Odd) || (relative && (*relative == L"Data" || *relative == L"Data/GameData" || *relative == L"Bin"));
        }

        std::vector<std::filesystem::path> ListDirectories(std::filesystem::path const& path, std::size_t limit) const override
        {
            if (HasSurrogate(path))
                return {};
            std::vector<std::filesystem::path> found = Base.ListDirectories(path, limit);
            if (Generic(path) == Generic(Parent) && found.size() < limit)
                found.push_back(Odd);
            return found;
        }

        std::vector<std::filesystem::path> ListFiles(std::filesystem::path const& path, std::size_t limit) const override
        {
            if (HasSurrogate(path))
                return {};
            std::vector<std::filesystem::path> found = Base.ListFiles(path, limit);
            if (Generic(path) == Generic(OddDump.parent_path()) && found.size() < limit)
                found.push_back(OddDump);
            return found;
        }

        std::optional<std::string> ReadText(std::filesystem::path const& path, std::size_t maxBytes) const override
        {
            if (!HasSurrogate(path))
                return Base.ReadText(path, maxBytes);
            if (Generic(path) == Generic(OddDump))
                return std::string("{\"version\": 2, \"classes\": {}}").substr(0, maxBytes);
            std::optional<std::wstring> const relative = Inside(path, Odd);
            if (relative && *relative == L"Bin/revision.dat")
                return std::string("r900000.Wizard_1_700\n").substr(0, maxBytes);
            return std::nullopt;
        }

        std::filesystem::path Canonical(std::filesystem::path const& path) const override
        {
            return HasSurrogate(path) ? path : Base.Canonical(path);
        }

        std::vector<UninstallEntry> GetUninstallEntries() const override { return Base.GetUninstallEntries(); }
        std::optional<std::string> GetSteamPath() const override { return Base.GetSteamPath(); }
        std::filesystem::path GetWorkingDirectory() const override { return Base.GetWorkingDirectory(); }
        std::filesystem::path GetExecutableDirectory() const override { return Base.GetExecutableDirectory(); }

    private:
        static bool HasSurrogate(std::filesystem::path const& path)
        {
            std::wstring const& text = path.native();
            return std::any_of(text.begin(), text.end(), [](wchar_t c) { return c >= 0xD800 && c <= 0xDFFF; });
        }

        static std::wstring Generic(std::filesystem::path const& path)
        {
            return path.lexically_normal().generic_wstring();
        }

        static std::optional<std::wstring> Inside(std::filesystem::path const& path, std::filesystem::path const& root)
        {
            std::wstring const text = Generic(path);
            std::wstring const base = Generic(root) + L"/";
            if (!text.starts_with(base))
                return std::nullopt;
            return text.substr(base.size());
        }
    };
#endif
}

TEST(ClientSetupTest, AutoPicksTheNewestInstallSavesOnlyClientDirAndUsesTheBuiltDump)
{
    SetupHarness setup("ClientDir =\nTypeDumpPath =\n");
    setup.AddNewer();
    ClientSetupResult const result = setup.Run({});
    EXPECT_EQ(result.Mode, SetupMode::Auto);
    EXPECT_EQ(setup.Counters->Reads.load(), 0);
    EXPECT_EQ(setup.Out.str(), "");
    ASSERT_TRUE(result.Install);
    EXPECT_EQ(ClientLocator::PathText(result.Install->Root), Newer);
    ASSERT_TRUE(result.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*result.TypeDump), Built);
    EXPECT_TRUE(result.TypeDumpBuilt);
    EXPECT_EQ(setup.Builder.Calls, std::vector<std::string>{ Newer });
    ASSERT_EQ(result.Saved.size(), 1u);
    EXPECT_EQ(result.Saved[0], (std::pair<std::string, std::string>{ "ClientDir", Newer }));
    EXPECT_TRUE(std::filesystem::equivalent(result.SavedTo, setup.SavedPath()));
    EXPECT_EQ(setup.Config->GetOption<std::string>("ClientDir", "", true), Newer);
    EXPECT_EQ(setup.Config->GetOption<std::string>("TypeDumpPath", "x", true), "");
    EXPECT_EQ(setup.Saved(), std::string(SavedHeader) + "ClientDir = \"" + Newer + "\"\n");
    EXPECT_TRUE(setup.Reported(false, "ClientDir is not set, so gameserver uses the newest Wizard101 install on this machine: D:/Games/Wizard101 (r900000.Wizard_1_700), found through ")) << setup.AllReports();
    EXPECT_TRUE(setup.Reported(false, std::string("Using the type dump ") + Built + " built from D:/Games/Wizard101 (r900000.Wizard_1_700)")) << setup.AllReports();
    EXPECT_TRUE(setup.Reported(false, "Saved ClientDir = \"D:/Games/Wizard101\" to ")) << setup.AllReports();
    EXPECT_EQ(std::count_if(setup.Reports.begin(), setup.Reports.end(), [](auto const& report) { return report.first; }), 0) << setup.AllReports();

    setup.Reports.clear();
    ClientSetupResult const again = setup.Run({});
    EXPECT_TRUE(again.Saved.empty());
    EXPECT_TRUE(again.Installs.empty());
    ASSERT_TRUE(again.Install);
    EXPECT_EQ(ClientLocator::PathText(again.Install->Root), Newer);
    EXPECT_EQ(setup.Builder.Calls.size(), 2u);
}

TEST(ClientSetupTest, NewestPrefersHigherRevisionsThenTheProgramThenDiscoveryOrder)
{
    EXPECT_FALSE(ClientSetup::Newest({}));
    std::optional<ClientCandidate> const newest = ClientSetup::Newest({ Candidate("A", "r806919.Wizard_1_610", true), Candidate("B", "r900000.Wizard_1_700", false), Candidate("C", "r900000.Wizard_1_700", true), Candidate("D", "r900000", true), Candidate("E", "", true) });
    ASSERT_TRUE(newest);
    EXPECT_EQ(ClientLocator::PathText(newest->Install.Root), "C");
    std::optional<ClientCandidate> const known = ClientSetup::Newest({ Candidate("A", "", true), Candidate("B", "r1", false) });
    ASSERT_TRUE(known);
    EXPECT_EQ(ClientLocator::PathText(known->Install.Root), "B");
    std::optional<ClientCandidate> const unknown = ClientSetup::Newest({ Candidate("A", "", false), Candidate("B", "", false) });
    ASSERT_TRUE(unknown);
    EXPECT_EQ(ClientLocator::PathText(unknown->Install.Root), "A");
}

TEST(ClientSetupTest, AutoKeepsUsableAndLockedValuesAndReplacesAnUnusableDump)
{
    SetupHarness usable("ClientDir = \"" + std::string(Install) + "\"\nTypeDumpPath = \"" + Dump + "\"\n");
    usable.System.AddFile(Dump, "{\"classes\": {}, \"version\": 2}");
    ClientSetupResult const kept = usable.Run({});
    ASSERT_TRUE(kept.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*kept.TypeDump), Dump);
    EXPECT_FALSE(kept.TypeDumpBuilt);
    EXPECT_TRUE(usable.Builder.Calls.empty());
    EXPECT_TRUE(kept.Saved.empty());
    EXPECT_TRUE(usable.Reports.empty()) << usable.AllReports();
    EXPECT_FALSE(std::filesystem::exists(usable.Directory.Path() / "conf.d"));

    SetupHarness stale("ClientDir = \"" + std::string(Install) + "\"\nTypeDumpPath = C:/Stale.json\n");
    stale.System.AddFile("C:/Stale.json", "not a dump");
    ClientSetupResult const replaced = stale.Run({});
    ASSERT_TRUE(replaced.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*replaced.TypeDump), Built);
    EXPECT_EQ(stale.Builder.Calls, std::vector<std::string>{ Install });
    EXPECT_TRUE(replaced.Saved.empty());
    EXPECT_EQ(stale.Config->GetOption<std::string>("TypeDumpPath", "", true), "C:/Stale.json");
    EXPECT_TRUE(stale.Reported(true, "TypeDumpPath C:/Stale.json is not a type dump, so gameserver uses the type dump built from its install instead")) << stale.AllReports();

    SetupHarness locked("ClientDir =\nTypeDumpPath =\n", { { "ClientDir", "" } });
    locked.Environment["AMBROSE_TYPE_DUMP_PATH"] = "C:/Missing.json";
    ClientSetupResult const untouched = locked.Run({});
    EXPECT_TRUE(untouched.Installs.empty());
    EXPECT_EQ(locked.System.DirectoryQueries, 0u);
    EXPECT_FALSE(untouched.Install);
    ASSERT_TRUE(untouched.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*untouched.TypeDump), "C:/Missing.json");
    EXPECT_TRUE(locked.Builder.Calls.empty());
    EXPECT_TRUE(locked.Reported(true, "TypeDumpPath C:/Missing.json is not a type dump, and the environment variable AMBROSE_TYPE_DUMP_PATH sets it, so setup leaves it as it is")) << locked.AllReports();
    EXPECT_FALSE(std::filesystem::exists(locked.Directory.Path() / "conf.d"));

    SetupHarness wrong("ClientDir =\nTypeDumpPath =\n", { { "ClientDir", "E:/Nothing" } });
    ClientSetupResult const asIs = wrong.Run({});
    ASSERT_TRUE(asIs.Install);
    EXPECT_EQ(ClientLocator::PathText(asIs.Install->Root), "E:/Nothing");
    EXPECT_FALSE(asIs.TypeDump);
    EXPECT_TRUE(wrong.Builder.Calls.empty());
    EXPECT_NE(asIs.TypeDumpError.find("ClientDir E:/Nothing holds no Wizard101 install, so no type dump can be built from it"), std::string::npos) << asIs.TypeDumpError;
    EXPECT_TRUE(wrong.Reported(true, "ClientDir E:/Nothing holds no Wizard101 install, and the command line sets it, so setup leaves it as it is")) << wrong.AllReports();
    EXPECT_TRUE(asIs.Saved.empty());
}

TEST(ClientSetupTest, AutoSavesNoInstallWithoutADumpAndReportsBuilderFailuresAndMachinesWithoutAnInstall)
{
    SetupHarness failing("ClientDir =\nTypeDumpPath =\n");
    failing.Builder.Result.reset();
    ClientSetupResult const failed = failing.Run({});
    EXPECT_FALSE(failed.TypeDump);
    EXPECT_EQ(failed.TypeDumpError, "the type dump for C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) could not be built: typeextract exited with 1");
    EXPECT_TRUE(failing.Reported(true, "The type dump for C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) could not be built: typeextract exited with 1")) << failing.AllReports();
    ASSERT_TRUE(failed.Install);
    EXPECT_EQ(ClientLocator::PathText(failed.Install->Root), Install);
    EXPECT_TRUE(failed.Saved.empty());
    EXPECT_FALSE(std::filesystem::exists(failing.SavedPath()));
    EXPECT_EQ(failing.Config->GetOption<std::string>("ClientDir", "x", true), "");
    EXPECT_TRUE(failing.Reported(true, "ClientDir \"C:/ProgramData/KingsIsle Entertainment/Wizard101\" was not saved, because no type dump is in use for it: the type dump for C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) could not be built: typeextract exited with 1; gameserver uses it for this run only and tries again on its next start")) << failing.AllReports();

    failing.Reports.clear();
    ClientSetupResult const login = failing.Run({}, true, { "loginserver", true, true, true });
    EXPECT_FALSE(login.Install);
    EXPECT_FALSE(login.TypeDump);
    EXPECT_TRUE(login.Saved.empty());
    EXPECT_EQ(failing.Builder.Calls.size(), 2u);
    EXPECT_TRUE(failing.Reported(true, "loginserver starts without client data and tries again on its next start")) << failing.AllReports();

    SetupHarness empty("ClientDir =\nTypeDumpPath =\n", {}, false);
    ClientSetupResult const nothing = empty.Run({});
    EXPECT_FALSE(nothing.Install);
    EXPECT_FALSE(nothing.TypeDump);
    EXPECT_TRUE(empty.Builder.Calls.empty());
    EXPECT_EQ(nothing.TypeDumpError, "no Wizard101 install is configured or found, so no type dump can be built");
    EXPECT_TRUE(empty.Reported(true, "ClientDir is not set, and no Wizard101 install was found on this machine, so gameserver runs without client data; install Wizard101, or set ClientDir in conf.d/client-data.conf to the folder that holds Data and Bin")) << empty.AllReports();
    EXPECT_FALSE(std::filesystem::exists(empty.Directory.Path() / "conf.d"));
}

TEST(ClientSetupTest, TheLoginServerNeverSavesAnInstallWithoutATypeDump)
{
    ClientSetupRequest const login{ "loginserver", true, true, true };
    SetupHarness failing("ClientDir =\nTypeDumpPath =\n");
    failing.Builder.Result.reset();
    ClientSetupResult const refused = failing.Run({}, true, login);
    EXPECT_TRUE(refused.Saved.empty());
    EXPECT_FALSE(refused.Install);
    EXPECT_FALSE(refused.TypeDump);
    EXPECT_FALSE(std::filesystem::exists(failing.SavedPath()));
    EXPECT_EQ(failing.Config->GetOption<std::string>("ClientDir", "x", true), "");
    EXPECT_TRUE(failing.Reported(true, "ClientDir \"C:/ProgramData/KingsIsle Entertainment/Wizard101\" was not saved, because loginserver needs a type dump with its install and the type dump for C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) could not be built: typeextract exited with 1; loginserver starts without client data and tries again on its next start")) << failing.AllReports();

    SetupHarness building("ClientDir =\nTypeDumpPath =\n");
    ClientSetupResult const built = building.Run({}, true, login);
    ASSERT_EQ(built.Saved.size(), 1u);
    ASSERT_TRUE(built.Install);
    ASSERT_TRUE(built.TypeDump);
    EXPECT_EQ(building.Config->GetOption<std::string>("ClientDir", "", true), Install);

    SetupHarness configured("ClientDir = \"" + std::string(Install) + "\"\nTypeDumpPath =\n");
    configured.Builder.Result.reset();
    ClientSetupResult const kept = configured.Run({}, true, login);
    ASSERT_TRUE(kept.Install);
    EXPECT_FALSE(kept.TypeDump);
    EXPECT_NE(kept.TypeDumpError.find("could not be built: typeextract exited with 1"), std::string::npos);

    SetupHarness lockedDump("ClientDir =\nTypeDumpPath =\n");
    lockedDump.Environment["AMBROSE_TYPE_DUMP_PATH"] = "C:/Missing.json";
    ClientSetupResult const unusable = lockedDump.Run({}, true, login);
    EXPECT_TRUE(unusable.Saved.empty());
    EXPECT_FALSE(unusable.Install);
    ASSERT_TRUE(unusable.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*unusable.TypeDump), "C:/Missing.json");
    EXPECT_TRUE(lockedDump.Builder.Calls.empty());
    EXPECT_FALSE(std::filesystem::exists(lockedDump.SavedPath()));
    EXPECT_TRUE(lockedDump.Reported(true, "ClientDir \"C:/ProgramData/KingsIsle Entertainment/Wizard101\" was not saved, because loginserver needs a type dump with its install and TypeDumpPath C:/Missing.json is not a type dump; loginserver starts without client data and tries again on its next start")) << lockedDump.AllReports();

    lockedDump.Reports.clear();
    ClientSetupResult const game = lockedDump.Run({});
    EXPECT_TRUE(game.Saved.empty());
    ASSERT_TRUE(game.Install);
    EXPECT_FALSE(std::filesystem::exists(lockedDump.SavedPath()));
    EXPECT_TRUE(lockedDump.Reported(true, "ClientDir \"C:/ProgramData/KingsIsle Entertainment/Wizard101\" was not saved, because no type dump is in use for it: TypeDumpPath C:/Missing.json is not a type dump; gameserver uses it for this run only")) << lockedDump.AllReports();

    SetupHarness unplugged("ClientDir = E:/Unplugged\nTypeDumpPath =\n");
    unplugged.Builder.Result.reset();
    ClientSetupResult const skipped = unplugged.Run({}, true, login);
    EXPECT_FALSE(skipped.Install);
    EXPECT_TRUE(skipped.Saved.empty());
    EXPECT_TRUE(unplugged.Reported(true, "The install C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) is not used, because loginserver needs a type dump with its install and the type dump for C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) could not be built: typeextract exited with 1; loginserver starts without client data and tries again on its next start")) << unplugged.AllReports();
}

TEST(ClientSetupTest, AutoUsesTheNewestInstallForThisRunOnlyWhenClientDirHoldsNoInstall)
{
    SetupHarness own("ClientDir = E:/Unplugged/Wizard101\nTypeDumpPath =\n");
    ClientSetupResult const result = own.Run({});
    ASSERT_TRUE(result.Install);
    EXPECT_EQ(ClientLocator::PathText(result.Install->Root), Install);
    ASSERT_TRUE(result.TypeDump);
    EXPECT_EQ(own.Builder.Calls, std::vector<std::string>{ Install });
    EXPECT_TRUE(result.Saved.empty());
    EXPECT_FALSE(std::filesystem::exists(own.Directory.Path() / "conf.d"));
    EXPECT_EQ(own.Config->GetOption<std::string>("ClientDir", "", true), "E:/Unplugged/Wizard101");
    EXPECT_TRUE(own.Reported(true, "ClientDir E:/Unplugged/Wizard101 holds no Wizard101 install, so gameserver uses the newest Wizard101 install on this machine for this run only: C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), found through ")) << own.AllReports();
    EXPECT_TRUE(own.Reported(true, ". ClientDir is not replaced in case its folder is only unavailable for now; point ClientDir in " + ClientLocator::PathText(own.File) + " at the install to keep, or empty it to save the newest")) << own.AllReports();

    SetupHarness saved("ClientDir =\nTypeDumpPath =\n");
    std::filesystem::create_directories(saved.Directory.Path() / "conf.d");
    std::ofstream(saved.SavedPath(), std::ios::binary) << SavedHeader << "ClientDir = \"E:/Unplugged/Wizard101\"\n";
    ASSERT_TRUE(saved.Config->Reload().Succeeded());
    std::string const before = saved.Saved();
    ClientSetupResult const replaced = saved.Run({});
    ASSERT_TRUE(replaced.Install);
    EXPECT_EQ(ClientLocator::PathText(replaced.Install->Root), Install);
    EXPECT_TRUE(replaced.Saved.empty());
    EXPECT_EQ(saved.Saved(), before);
    EXPECT_TRUE(saved.Reported(true, "at the install to keep, or empty it to save the newest")) << saved.AllReports();

    saved.System.AddInstall("E:/Unplugged/Wizard101", "r806919.Wizard_1_610");
    saved.Reports.clear();
    ClientSetupResult const back = saved.Run({});
    ASSERT_TRUE(back.Install);
    EXPECT_EQ(ClientLocator::PathText(back.Install->Root), "E:/Unplugged/Wizard101");
    EXPECT_TRUE(back.Saved.empty());
    EXPECT_EQ(saved.Saved(), before);
    EXPECT_EQ(saved.Warnings(), 0u) << saved.AllReports();
}

TEST(ClientSetupTest, ATypeDumpPathSetEmptyOnTheCommandLineIsLeftAlone)
{
    SetupHarness automatic("ClientDir = \"" + std::string(Install) + "\"\nTypeDumpPath =\n", { { "TypeDumpPath", "" } });
    ClientSetupResult const result = automatic.Run({});
    ASSERT_TRUE(result.Install);
    EXPECT_FALSE(result.TypeDump);
    EXPECT_EQ(result.TypeDumpError, "TypeDumpPath is set empty by the command line, so setup leaves it as it is");
    EXPECT_TRUE(automatic.Builder.Calls.empty());
    EXPECT_TRUE(automatic.Builder.Lookups.empty());
    EXPECT_EQ(automatic.System.DirectoryQueries, 0u);
    EXPECT_TRUE(result.Saved.empty());

    SetupHarness asked("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n", { { "TypeDumpPath", "" } });
    asked.System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const answered = asked.Run({ "", "" });
    EXPECT_EQ(asked.Counters->Reads.load(), 1);
    EXPECT_EQ(asked.Out.str().find("needs the type dump"), std::string::npos) << asked.Out.str();
    ASSERT_TRUE(answered.Install);
    EXPECT_FALSE(answered.TypeDump);
    EXPECT_EQ(answered.TypeDumpError, "TypeDumpPath is set empty by the command line, so setup leaves it as it is");
    EXPECT_TRUE(asked.Builder.Calls.empty());
    EXPECT_TRUE(asked.Builder.Lookups.empty());
    EXPECT_TRUE(answered.Saved.empty());
    EXPECT_FALSE(std::filesystem::exists(asked.SavedPath()));
    EXPECT_TRUE(asked.Reported(true, "ClientDir \"C:/ProgramData/KingsIsle Entertainment/Wizard101\" was not saved, because no type dump is in use for it: TypeDumpPath is set empty by the command line, so setup leaves it as it is; gameserver uses it for this run only and asks again on its next start")) << asked.AllReports();
}

TEST(ClientSetupTest, AskOffersTheFindsAndABuildOnlyWhenThereIsSomethingToOffer)
{
    SetupHarness chosen("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n");
    chosen.System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const result = chosen.Run({ "", "" });
    EXPECT_EQ(result.Mode, SetupMode::Ask);
    ASSERT_EQ(result.Saved.size(), 2u);
    EXPECT_EQ(result.Saved[0], (std::pair<std::string, std::string>{ "ClientDir", Install }));
    EXPECT_EQ(result.Saved[1], (std::pair<std::string, std::string>{ "TypeDumpPath", Dump }));
    EXPECT_TRUE(chosen.Builder.Calls.empty());
    EXPECT_EQ(chosen.Saved(), std::string(SavedHeader) + "ClientDir = \"" + Install + "\"\nTypeDumpPath = \"" + Dump + "\"\n");
    EXPECT_NE(chosen.Out.str().find("gameserver needs your own Wizard101 install, and ClientDir is not set. Found on this machine:\n  1) C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), found through "), std::string::npos) << chosen.Out.str();
    EXPECT_NE(chosen.Out.str().find("gameserver needs the type dump made from your install, and TypeDumpPath is not set. Found on this machine:\n"), std::string::npos) << chosen.Out.str();
    EXPECT_EQ(chosen.Out.str().find("r806919, so"), std::string::npos);

    SetupHarness build("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n");
    ClientSetupResult const yes = build.Run({ "", "y" });
    ASSERT_TRUE(yes.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*yes.TypeDump), Built);
    EXPECT_EQ(build.Builder.Calls, std::vector<std::string>{ Install });
    ASSERT_EQ(yes.Saved.size(), 1u);
    EXPECT_EQ(yes.Saved[0].first, "ClientDir");
    EXPECT_NE(build.Out.str().find("gameserver needs the type dump made from your install, and TypeDumpPath is not set. Build it from C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) now? [Y/n]: "), std::string::npos) << build.Out.str();

    SetupHarness declined("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n");
    ClientSetupResult const no = declined.Run({ "", "n" }, true, { "loginserver", true, true, true });
    EXPECT_FALSE(no.TypeDump);
    EXPECT_FALSE(no.Install);
    EXPECT_TRUE(no.Saved.empty());
    EXPECT_TRUE(declined.Builder.Calls.empty());
    EXPECT_EQ(no.TypeDumpError, "TypeDumpPath is not set, and the type dump was not built");

    SetupHarness nothing("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n", {}, false);
    ClientSetupResult const none = nothing.Run({ "", "" });
    EXPECT_GT(nothing.Counters->Reads.load(), 0) << "a machine with no install is asked to get one, which is the one thing left to offer";
    EXPECT_NE(nothing.Out.str().find("none was found on this machine"), std::string::npos) << nothing.Out.str();
    EXPECT_NE(nothing.Out.str().find("Ambrose never downloads it for you"), std::string::npos) << nothing.Out.str();
    EXPECT_NE(nothing.Out.str().find("Look again, now that Wizard101 is installed"), std::string::npos) << nothing.Out.str();
    EXPECT_NE(nothing.Out.str().find("Still no Wizard101 install on this machine"), std::string::npos)
        << "looking again and finding nothing says so and offers to look once more, rather than giving up on the first try: " << nothing.Out.str();
    EXPECT_FALSE(none.Install) << "no amount of looking invents an install that is not there";
    EXPECT_TRUE(nothing.Reported(true, "ClientDir is not set, and no Wizard101 install was found on this machine; set ClientDir in conf.d/client-data.conf to the folder that holds Data and Bin")) << nothing.AllReports();

    SetupHarness quiet("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n");
    quiet.System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const piped = quiet.Run({ "", "" }, false);
    EXPECT_TRUE(piped.Saved.empty());
    EXPECT_EQ(quiet.Counters->Reads.load(), 0);
    EXPECT_TRUE(quiet.Builder.Calls.empty());
    EXPECT_TRUE(quiet.Reported(true, "ClientDir is not set, and Wizard101 was found on this machine: C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), found through ")) << quiet.AllReports();
    EXPECT_TRUE(quiet.Reported(true, ". Start gameserver in a terminal whose input and output are not redirected to choose one, set Setup.Mode = auto to use the newest, or set ClientDir in conf.d/client-data.conf")) << quiet.AllReports();
    EXPECT_TRUE(quiet.Reported(true, std::string("TypeDumpPath is not set, and a type dump was found on this machine: ") + Dump)) << quiet.AllReports();
    EXPECT_TRUE(quiet.Reported(true, ". Start gameserver in a terminal whose input and output are not redirected to choose one, set Setup.Mode = auto to use the type dump built from the install, or set TypeDumpPath in conf.d/client-data.conf")) << quiet.AllReports();
    EXPECT_FALSE(std::filesystem::exists(quiet.Directory.Path() / "conf.d"));

    SetupHarness closed("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n");
    ClientSetupResult const skipped = closed.Run({});
    EXPECT_FALSE(skipped.Install);
    EXPECT_NE(closed.Out.str().find("Input closed; skipping setup questions."), std::string::npos);
    EXPECT_TRUE(closed.Reported(true, ". The setup questions were skipped, so set ClientDir in conf.d/client-data.conf, or set Setup.Mode = auto to use the newest")) << closed.AllReports();
}

TEST(ClientSetupTest, AskUsesACurrentBuiltDumpWithoutAskingSoABuildAfterAYesLasts)
{
    SetupHarness setup("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n");
    ClientSetupResult const yes = setup.Run({ "", "y" });
    ASSERT_TRUE(yes.TypeDump);
    ASSERT_EQ(yes.Saved.size(), 1u);
    EXPECT_EQ(setup.Builder.Lookups, std::vector<std::string>{ Install });
    EXPECT_EQ(setup.Builder.Calls, std::vector<std::string>{ Install });

    setup.Builder.Current = std::filesystem::path(Built);
    setup.Reports.clear();
    int const reads = setup.Counters->Reads.load();
    ClientSetupResult const later = setup.Run({}, false, { "loginserver", true, true, true });
    ASSERT_TRUE(later.Install);
    EXPECT_EQ(ClientLocator::PathText(later.Install->Root), Install);
    ASSERT_TRUE(later.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*later.TypeDump), Built);
    EXPECT_TRUE(later.TypeDumpBuilt);
    EXPECT_TRUE(later.TypeDumpError.empty()) << later.TypeDumpError;
    EXPECT_TRUE(later.Saved.empty());
    EXPECT_EQ(setup.Builder.Calls.size(), 1u);
    EXPECT_EQ(setup.Builder.Lookups.size(), 2u);
    EXPECT_EQ(setup.Counters->Reads.load(), reads);
    EXPECT_TRUE(setup.Reported(false, std::string("Using the type dump ") + Built + " built from C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610)")) << setup.AllReports();
    EXPECT_EQ(setup.Warnings(), 0u) << setup.AllReports();

    SetupHarness interactive("ClientDir = \"" + std::string(Install) + "\"\nTypeDumpPath =\nSetup.Mode = ask\n");
    interactive.System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    interactive.Builder.Current = std::filesystem::path(Built);
    ClientSetupResult const current = interactive.Run({ "", "" });
    ASSERT_TRUE(current.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*current.TypeDump), Built);
    EXPECT_EQ(interactive.Counters->Reads.load(), 0);
    EXPECT_EQ(interactive.Out.str(), "");
    EXPECT_TRUE(current.Saved.empty());
}

TEST(ClientSetupTest, AskOffersABuildWhenNoDumpFoundIsNamedForTheInstall)
{
    std::string const body = "ClientDir = \"" + std::string(Newer) + "\"\nTypeDumpPath =\nSetup.Mode = ask\n";
    std::string const question = "gameserver needs the type dump made from your install, and TypeDumpPath is not set. Build it from D:/Games/Wizard101 (r900000.Wizard_1_700) now? [Y/n]: ";
    std::string const others = "No type dump found is known to fit D:/Games/Wizard101 (r900000.Wizard_1_700), so choose one only if it was made from that install. Found on this machine:\n  1) " + std::string(Dump);

    SetupHarness declined(body);
    declined.AddNewer();
    declined.System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const no = declined.Run({ "n", "s" });
    EXPECT_NE(declined.Out.str().find(question), std::string::npos) << declined.Out.str();
    EXPECT_NE(declined.Out.str().find(others), std::string::npos) << declined.Out.str();
    EXPECT_LT(declined.Out.str().find(question), declined.Out.str().find(others));
    EXPECT_FALSE(no.TypeDump);
    EXPECT_EQ(no.TypeDumpError, "TypeDumpPath is not set, and the type dump was not built");
    EXPECT_TRUE(declined.Builder.Calls.empty());
    EXPECT_TRUE(no.Saved.empty());

    SetupHarness accepted(body);
    accepted.AddNewer();
    accepted.System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const yes = accepted.Run({ "y" });
    ASSERT_TRUE(yes.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*yes.TypeDump), Built);
    EXPECT_EQ(accepted.Builder.Calls, std::vector<std::string>{ Newer });
    EXPECT_EQ(accepted.Out.str().find("Found on this machine"), std::string::npos) << accepted.Out.str();

    SetupHarness picked(body);
    picked.AddNewer();
    picked.System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const other = picked.Run({ "n", "1" });
    ASSERT_TRUE(other.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*other.TypeDump), Dump);
    ASSERT_EQ(other.Saved.size(), 1u);
    EXPECT_EQ(other.Saved[0], (std::pair<std::string, std::string>{ "TypeDumpPath", Dump }));
    EXPECT_TRUE(other.TypeDumpError.empty()) << other.TypeDumpError;

    SetupHarness piped(body);
    piped.AddNewer();
    piped.System.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const quiet = piped.Run({ "y" }, false);
    EXPECT_FALSE(quiet.TypeDump);
    EXPECT_EQ(piped.Counters->Reads.load(), 0);
    EXPECT_TRUE(piped.Reported(true, "TypeDumpPath is not set, and no type dump was found for D:/Games/Wizard101 (r900000.Wizard_1_700). Start gameserver in a terminal whose input and output are not redirected to choose one")) << piped.AllReports();

    SetupHarness stale("ClientDir = \"" + std::string(Install) + "\"\nTypeDumpPath =\nSetup.Mode = ask\n");
    stale.System.Environment["LOCALAPPDATA"] = "C:/Users/wiz/AppData/Local";
    stale.System.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/types/r806919.Wizard_1_610.json", "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const rebuilt = stale.Run({ "" });
    EXPECT_NE(stale.Out.str().find("Build it from C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) now? [Y/n]: "), std::string::npos) << stale.Out.str();
    ASSERT_TRUE(rebuilt.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*rebuilt.TypeDump), Built);
    EXPECT_EQ(stale.Builder.Calls, std::vector<std::string>{ Install });
}

#ifdef _WIN32
TEST(ClientSetupTest, PathsThatAreNotValidUnicodeAreSkippedInsteadOfThrowing)
{
    UnpairedSurrogateSystem machine;
    machine.Base.Environment["ProgramData"] = "C:/ProgramData";
    machine.Base.Environment["LOCALAPPDATA"] = "C:/Users/wiz/AppData/Local";
    machine.Base.AddInstall(Install, "r806919.Wizard_1_610");
    machine.Base.AddFolder("D:/Games");
    machine.Base.Uninstall.push_back({ "Wizard101", "D:/Games" });
    std::string const odd = "D:/Games/W\xEF\xBF\xBD (r900000.Wizard_1_700), found through ";
    std::string const oddDump = "C:/Users/wiz/AppData/Local/ProjectAmbrose/x\xEF\xBF\xBD.json, found in the Ambrose data folder: its path is not valid Unicode, so it cannot be saved or passed on; rename the file to use it";
    ASSERT_EQ(ClientLocator::FindInstalls(machine).size(), 2u);

    std::string text;
    EXPECT_NO_THROW(text = ClientSetup::ConfigPath(machine.Odd));
    EXPECT_EQ(text, "D:/Games/W\xEF\xBF\xBD");

    SetupHarness automatic("ClientDir =\nTypeDumpPath =\n", {}, false);
    automatic.Machine = &machine;
    ClientSetupResult const result = automatic.Run({});
    ASSERT_EQ(result.Installs.size(), 1u);
    ASSERT_TRUE(result.Install);
    EXPECT_EQ(ClientLocator::PathText(result.Install->Root), Install);
    ASSERT_EQ(result.Saved.size(), 1u);
    EXPECT_EQ(result.Saved[0].second, Install);
    EXPECT_TRUE(automatic.Reported(true, "Setup skips the Wizard101 install " + odd)) << automatic.AllReports();
    EXPECT_TRUE(automatic.Reported(true, ": its path is not valid Unicode, so it cannot be saved or passed on; rename its folder to use it")) << automatic.AllReports();

    machine.Base.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    SetupHarness asked("ClientDir = \"" + std::string(Install) + "\"\nTypeDumpPath =\nSetup.Mode = ask\n", {}, false);
    asked.Machine = &machine;
    ClientSetupResult const chosen = asked.Run({ "" });
    ASSERT_TRUE(chosen.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*chosen.TypeDump), Dump);
    EXPECT_EQ(asked.Out.str().find("x\xEF\xBF\xBD"), std::string::npos) << asked.Out.str();
    EXPECT_EQ(asked.Out.str().find("  2) "), std::string::npos) << asked.Out.str();
    EXPECT_TRUE(asked.Reported(true, "Setup skips the type dump " + oddDump)) << asked.AllReports();

    FakeBuilder builder;
    std::ostringstream out;
    std::ostringstream err;
    SetupPrompt disabled(nullptr, out, false, std::chrono::seconds(30));
    std::optional<std::string> client;
    std::optional<std::string> dump;
    ClientSetup::ForTool(SetupMode::Auto, client, &dump, disabled, machine, builder.Provider(), "extractor", err);
    EXPECT_EQ(client, Install);
    EXPECT_NE(err.str().find("extractor: Setup skips the Wizard101 install " + odd), std::string::npos) << err.str();

    std::ostringstream offErr;
    std::optional<std::string> offClient = std::string(Install);
    std::optional<std::string> offDump;
    ClientSetup::ForTool(SetupMode::Off, offClient, &offDump, disabled, machine, builder.Provider(), "extractor", offErr);
    EXPECT_FALSE(offDump);
    EXPECT_NE(offErr.str().find("extractor: Setup skips the type dump " + oddDump), std::string::npos) << offErr.str();
    EXPECT_EQ(offErr.str().find("a type dump was found on this machine: C:/Users"), std::string::npos) << offErr.str();
}
#endif

TEST(ClientSetupTest, TypedPathsLoseQuotesExpandHomeAndAreSavedAbsolute)
{
    FakeClientSystem windows;
    EXPECT_EQ(ClientLocator::PathText(ClientSetup::TypedPath(windows, "  \"C:/Games/Wiz 101\"  ")), "C:/Games/Wiz 101");
    EXPECT_EQ(ClientLocator::PathText(ClientSetup::TypedPath(windows, "'C:/Games'")), "C:/Games");
    EXPECT_EQ(ClientLocator::PathText(ClientSetup::TypedPath(windows, "\"C:/Games'")), "\"C:/Games'");
    EXPECT_EQ(ClientLocator::PathText(ClientSetup::TypedPath(windows, "D:/Quoted \"Wiz 101\"")), "D:/Quoted \"Wiz 101\"");
    EXPECT_EQ(ClientLocator::PathText(ClientSetup::TypedPath(windows, "~/Games")), "~/Games");
    FakeClientSystem posix;
    posix.Windows = false;
    posix.Environment["HOME"] = "/home/wiz";
    EXPECT_EQ(ClientLocator::PathText(ClientSetup::TypedPath(posix, "~/Games/Wizard101")), "/home/wiz/Games/Wizard101");
    EXPECT_EQ(ClientLocator::PathText(ClientSetup::TypedPath(posix, "'~'")), "/home/wiz");
    EXPECT_EQ(ClientLocator::PathText(ClientSetup::TypedPath(posix, "~other/Games")), "~other/Games");

    SetupHarness typed("ClientDir = C:/Nowhere\nTypeDumpPath =\nSetup.Mode = ask\n");
    typed.System.AddInstall("D:/Wiz 101", "r900000.Wizard_1_700");
    typed.System.AddFile("D:/Wiz 101/r900000.Wizard_1_700.json", "{\"version\": 2, \"classes\": {}}");
    ClientSetupResult const result = typed.Run({ "E:/Empty", "\"D:/Wiz 101\"", "'D:/Wiz 101/r900000.Wizard_1_700.json'" });
    ASSERT_EQ(result.Saved.size(), 2u) << typed.Out.str() << typed.AllReports();
    EXPECT_EQ(result.Saved[0], (std::pair<std::string, std::string>{ "ClientDir", "D:/Wiz 101" }));
    EXPECT_EQ(result.Saved[1], (std::pair<std::string, std::string>{ "TypeDumpPath", "D:/Wiz 101/r900000.Wizard_1_700.json" }));
    EXPECT_NE(typed.Out.str().find("E:/Empty holds no Wizard101 install: there is no Data/GameData/Root.wad in it."), std::string::npos);
    EXPECT_NE(typed.Out.str().find("ClientDir C:/Nowhere holds no Wizard101 install"), std::string::npos);

    SetupHarness relative("ClientDir =\nTypeDumpPath =\n", {}, false);
    relative.System.Working = "C:/Work";
    relative.System.Environment["AMBROSE_CLIENT_DIR"] = "games/Wizard101";
    relative.System.AddInstall("games/Wizard101", "r900001.Wizard_1_700");
    ClientSetupResult const absolute = relative.Run({});
    ASSERT_EQ(absolute.Saved.size(), 1u) << relative.AllReports();
    EXPECT_EQ(absolute.Saved[0].second, "C:/Work/games/Wizard101");
}

TEST(ClientSetupTest, OffSearchesSavesAndBuildsNothingAndModesParse)
{
    SetupHarness off("ClientDir =\nTypeDumpPath =\nSetup.Mode = off\n");
    ClientSetupResult const result = off.Run({ "", "" });
    EXPECT_EQ(result.Mode, SetupMode::Off);
    EXPECT_EQ(off.System.DirectoryQueries, 0u);
    EXPECT_TRUE(result.Installs.empty());
    EXPECT_FALSE(result.Install);
    EXPECT_FALSE(result.TypeDump);
    EXPECT_EQ(result.TypeDumpError, "TypeDumpPath is not set, and Setup.Mode is off, so no type dump is built");
    EXPECT_TRUE(off.Builder.Calls.empty());
    EXPECT_TRUE(off.Reports.empty()) << off.AllReports();
    EXPECT_EQ(off.Counters->Reads.load(), 0);
    EXPECT_FALSE(std::filesystem::exists(off.Directory.Path() / "conf.d"));

    SetupHarness asIs("ClientDir = C:/Nowhere\nTypeDumpPath = C:/Stale.json\nSetup.Mode = off\n");
    ClientSetupResult const kept = asIs.Run({});
    ASSERT_TRUE(kept.Install);
    EXPECT_EQ(ClientLocator::PathText(kept.Install->Root), "C:/Nowhere");
    ASSERT_TRUE(kept.TypeDump);
    EXPECT_EQ(ClientLocator::PathText(*kept.TypeDump), "C:/Stale.json");
    EXPECT_TRUE(asIs.Reports.empty()) << asIs.AllReports();
    EXPECT_EQ(asIs.System.DirectoryQueries, 0u);

    SetupHarness environment("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n");
    environment.Environment["AMBROSE_SETUP_MODE"] = " Off ";
    EXPECT_EQ(environment.Run({}).Mode, SetupMode::Off);
    EXPECT_TRUE(environment.Builder.Calls.empty());

    SetupHarness invalid("ClientDir =\nTypeDumpPath =\nSetup.Mode = sometimes\n");
    EXPECT_EQ(invalid.Run({}).Mode, SetupMode::Auto);
    EXPECT_TRUE(invalid.Reported(true, "Setup.Mode 'sometimes' is not auto, ask or off, so setup runs in auto mode")) << invalid.AllReports();

    EXPECT_EQ(ClientSetup::ParseMode(" ASK "), SetupMode::Ask);
    EXPECT_EQ(ClientSetup::ParseMode("auto"), SetupMode::Auto);
    EXPECT_EQ(ClientSetup::ParseMode("Off"), SetupMode::Off);
    EXPECT_FALSE(ClientSetup::ParseMode(""));
    EXPECT_FALSE(ClientSetup::ParseMode("on"));
    EXPECT_EQ(ClientSetup::ModeName(SetupMode::Auto), "auto");
    EXPECT_EQ(ClientSetup::ModeName(SetupMode::Ask), "ask");
    EXPECT_EQ(ClientSetup::ModeName(SetupMode::Off), "off");
    std::unique_ptr<SetupPrompt> const disabled = ClientSetup::ServerPrompt(off.Out, *off.Config);
    EXPECT_EQ(disabled->GetStatus(), SetupPrompt::Status::Disabled);
    EXPECT_FALSE(disabled->IsInteractive());
}

TEST(ClientSetupTest, ASavedValueAnotherFileOverridesIsReportedInsteadOfClaimed)
{
    SetupHarness setup("ClientDir =\nTypeDumpPath =\nSetup.Mode = ask\n");
    std::filesystem::create_directories(setup.Directory.Path() / "conf.d");
    std::ofstream(setup.Directory.Path() / "conf.d" / "zz-local.conf", std::ios::binary) << Header << "ClientDir = D:/Old\n";
    ASSERT_TRUE(setup.Config->Reload().Succeeded());
    ClientSetupResult const result = setup.Run({ "", "" });
    EXPECT_EQ(setup.Builder.Calls, std::vector<std::string>{ Install });
    EXPECT_TRUE(result.Saved.empty());
    ASSERT_TRUE(result.Install);
    EXPECT_EQ(ClientLocator::PathText(result.Install->Root), Install);
    EXPECT_EQ(setup.Saved(), std::string(SavedHeader) + "ClientDir = \"" + Install + "\"\n");
    EXPECT_EQ(setup.Config->GetOption<std::string>("ClientDir", "", true), "D:/Old");
    EXPECT_TRUE(setup.Reported(true, "zz-local.conf sets ClientDir = \"D:/Old\" and overrides it, so gameserver uses the new value for this run only; remove ClientDir there to keep the choice")) << setup.AllReports();
    EXPECT_FALSE(setup.Reported(false, "Saved ClientDir")) << setup.AllReports();

    SetupHarness emptied("ClientDir = D:/Mine\nTypeDumpPath =\n");
    std::filesystem::create_directories(emptied.Directory.Path() / "conf.d");
    std::ofstream(emptied.Directory.Path() / "conf.d" / "zz-local.conf", std::ios::binary) << Header << "ClientDir =\n";
    ASSERT_TRUE(emptied.Config->Reload().Succeeded());
    ClientSetupResult const automatic = emptied.Run({});
    EXPECT_TRUE(automatic.Saved.empty());
    ASSERT_TRUE(automatic.Install);
    EXPECT_EQ(emptied.Saved(), std::string(SavedHeader) + "ClientDir = \"" + Install + "\"\n");
    EXPECT_EQ(emptied.Config->GetOption<std::string>("ClientDir", "x", true), "");
    EXPECT_TRUE(emptied.Reported(true, "zz-local.conf sets ClientDir = \"\" and overrides it, so gameserver uses the new value for this run only; remove ClientDir there to keep the choice")) << emptied.AllReports();

    SetupHarness edited("ClientDir = D:/Mine\nTypeDumpPath =\n");
    std::filesystem::create_directories(edited.Directory.Path() / "conf.d");
    std::ofstream(edited.SavedPath(), std::ios::binary) << SavedHeader << "ClientDir = \"" << Install << "\"\n";
    ASSERT_TRUE(edited.Config->Reload().Succeeded());
    ClientSetupResult const shadowed = edited.Run({});
    ASSERT_TRUE(shadowed.Install);
    EXPECT_EQ(ClientLocator::PathText(shadowed.Install->Root), Install);
    EXPECT_TRUE(shadowed.Saved.empty());
    EXPECT_TRUE(edited.Reported(true, "ClientDir = \"C:/ProgramData/KingsIsle Entertainment/Wizard101\" in " + ClientLocator::PathText(edited.SavedPath()) + " overrides ClientDir = \"D:/Mine\" in " + ClientLocator::PathText(edited.File)
        + "; edit or delete " + ClientLocator::PathText(edited.SavedPath()) + " to use the value in ")) << edited.AllReports();
    EXPECT_EQ(std::count_if(edited.Reports.begin(), edited.Reports.end(), [](auto const& report) { return report.first; }), 1) << edited.AllReports();

    SetupHarness same("ClientDir = \"C:/ProgramData/Other/../KingsIsle Entertainment/Wizard101\"\nTypeDumpPath =\n");
    std::filesystem::create_directories(same.Directory.Path() / "conf.d");
    std::ofstream(same.SavedPath(), std::ios::binary) << SavedHeader << "ClientDir = \"" << Install << "\"\n";
    ASSERT_TRUE(same.Config->Reload().Succeeded());
    ClientSetupResult const agreeing = same.Run({});
    ASSERT_TRUE(agreeing.Install);
    EXPECT_TRUE(same.Reports.size() == 1 && !same.Reports.front().first) << same.AllReports();
}

TEST(ClientSetupTest, SavingMergesLineByLineEscapesAndRefusesUnsafeFiles)
{
    LogTestDirectory directory;
    std::filesystem::path const config = directory.Path() / "gameserver.conf";
    std::filesystem::path const saved = directory.Path() / "conf.d" / "client-data.conf";
    std::filesystem::create_directories(saved.parent_path());
    auto const read = [&saved]
    {
        std::ifstream stream(saved, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), {});
    };
    auto const write = [&saved](std::string const& text) { std::ofstream(saved, std::ios::binary) << text; };

    write(std::string(Header) + "# my note\r\nKeep.Me = 7\nClientDir = C:/Old");
    std::filesystem::path savedTo;
    std::string error;
    std::string const awkward = "C:\\Games\\\"W\"\\101\nnext\tcolumn";
    ASSERT_TRUE(ClientSetup::Save(config, { { "ClientDir", awkward }, { "TypeDumpPath", "C:/Dump.json" } }, savedTo, error)) << error;
    EXPECT_TRUE(std::filesystem::equivalent(savedTo, saved));
    EXPECT_EQ(read(), std::string(Header) + "# my note\r\nKeep.Me = 7\nClientDir = \"C:\\\\Games\\\\\\\"W\\\"\\\\101\\nnext\\tcolumn\"\nTypeDumpPath = \"C:/Dump.json\"\n");
    ParsedConfig const parsed = ConfigMgr::ParseText(read(), saved, ConfigSourceKind::ModuleConfig);
    ASSERT_TRUE(parsed.Errors.empty());
    ASSERT_EQ(parsed.Entries.size(), 3u);
    EXPECT_EQ(parsed.Entries[1].second.Value, awkward);
    for (auto const& entry : std::filesystem::directory_iterator(saved.parent_path()))
        EXPECT_EQ(entry.path().filename(), saved.filename());

    std::string const before = read();
    EXPECT_FALSE(ClientSetup::Save(config, { { "ClientDir", "C:/Bad\rPath" } }, savedTo, error));
    EXPECT_EQ(error, "ClientDir holds a control character that a configuration file cannot keep");
    EXPECT_EQ(read(), before);

    write(std::string(Header) + "this line is not a setting\n");
    EXPECT_FALSE(ClientSetup::Save(config, { { "ClientDir", "C:/x" } }, savedTo, error));
    EXPECT_NE(error.find("has errors, so it is not rewritten"), std::string::npos) << error;
    EXPECT_EQ(read(), std::string(Header) + "this line is not a setting\n");

    write(std::string(Header) + "# " + std::string(ClientSetup::MaxSavedFileBytes, 'x') + "\n");
    EXPECT_FALSE(ClientSetup::Save(config, { { "ClientDir", "C:/x" } }, savedTo, error));
    EXPECT_NE(error.find("is larger than 65536 bytes, so it is not rewritten"), std::string::npos) << error;

    std::filesystem::remove(saved);
    std::filesystem::create_directories(saved);
    EXPECT_FALSE(ClientSetup::Save(config, { { "ClientDir", "C:/x" } }, savedTo, error));
    EXPECT_NE(error.find("is not a regular file"), std::string::npos) << error;

    LogTestDirectory blocked;
    std::ofstream(blocked.Path() / "conf.d", std::ios::binary) << "a file";
    EXPECT_FALSE(ClientSetup::Save(blocked.Path() / "gameserver.conf", { { "ClientDir", "C:/x" } }, savedTo, error));
    EXPECT_NE(error.find("cannot create the folder"), std::string::npos) << error;
    EXPECT_EQ(ClientSetup::ConfigPath("C:/a/./b/../c"), "C:/a/c");
}

TEST(ClientSetupTest, AFailedSaveOrReloadKeepsTheChoiceForThisRunAndRestoresTheFile)
{
    SetupHarness blocked("ClientDir =\nTypeDumpPath =\n");
    std::ofstream(blocked.Directory.Path() / "conf.d", std::ios::binary) << "a file";
    ClientSetupResult const unsaved = blocked.Run({});
    EXPECT_TRUE(unsaved.Saved.empty());
    ASSERT_TRUE(unsaved.Install);
    EXPECT_TRUE(blocked.Reported(true, "Could not save ClientDir = \"C:/ProgramData/KingsIsle Entertainment/Wizard101\" to ")) << blocked.AllReports();
    EXPECT_TRUE(blocked.Reported(true, "gameserver uses the choice for this run only; add that setting to ")) << blocked.AllReports();

    SetupHarness broken("ClientDir =\nTypeDumpPath =\n");
    std::filesystem::create_directories(broken.Directory.Path() / "conf.d");
    std::string const previous = std::string(Header) + "Keep.Me = 7\n";
    std::ofstream(broken.SavedPath(), std::ios::binary) << previous;
    ASSERT_TRUE(broken.Config->Reload().Succeeded());
    std::ofstream(broken.Directory.Path() / "conf.d" / "later.conf", std::ios::binary) << Header << "not a setting\n";
    ClientSetupResult const rolledBack = broken.Run({});
    EXPECT_TRUE(rolledBack.Saved.empty());
    ASSERT_TRUE(rolledBack.Install);
    EXPECT_EQ(broken.Saved(), previous);
    EXPECT_TRUE(broken.Reported(true, "Reloading the configuration after saving ClientDir = \"C:/ProgramData/KingsIsle Entertainment/Wizard101\" to ")) << broken.AllReports();
    EXPECT_TRUE(broken.Reported(true, "was put back as it was, and gameserver uses the choice for this run only")) << broken.AllReports();

    std::filesystem::remove(broken.SavedPath());
    ClientSetupResult const removed = broken.Run({});
    EXPECT_TRUE(removed.Saved.empty());
    EXPECT_FALSE(std::filesystem::exists(broken.SavedPath()));
}

TEST(ClientSetupTest, ToolsFollowTheirSetupMode)
{
    FakeClientSystem system;
    system.Environment["ProgramData"] = "C:/ProgramData";
    system.AddInstall(Install, "r806919.Wizard_1_610");
    FakeBuilder builder;
    auto const counters = std::make_shared<ScriptedPromptInput::Counters>();
    std::ostringstream out;

    std::ostringstream automatic;
    SetupPrompt disabled(nullptr, out, false, std::chrono::seconds(30));
    system.AddInstall(Newer, "r900000.Wizard_1_700");
    system.Uninstall.push_back({ "Wizard101", Newer });
    std::optional<std::string> client;
    std::optional<std::string> dump;
    ClientSetupResult const picked = ClientSetup::ForTool(SetupMode::Auto, client, &dump, disabled, system, builder.Provider(), "extractor", automatic);
    EXPECT_EQ(client, Newer);
    EXPECT_EQ(dump, Built);
    EXPECT_TRUE(picked.TypeDumpBuilt);
    EXPECT_EQ(builder.Calls, std::vector<std::string>{ Newer });
    EXPECT_NE(automatic.str().find("extractor: using the newest install on this machine, D:/Games/Wizard101 (r900000.Wizard_1_700), found through "), std::string::npos) << automatic.str();
    EXPECT_NE(automatic.str().find(std::string(", and the type dump ") + Built + " built from it; pass --client and --type-dump to choose otherwise\n"), std::string::npos) << automatic.str();
    std::string const automaticText = automatic.str();
    EXPECT_EQ(std::count(automaticText.begin(), automaticText.end(), '\n'), 1);

    std::ostringstream named;
    std::optional<std::string> namedClient = std::string(Install);
    std::optional<std::string> namedDump;
    ClientSetup::ForTool(SetupMode::Auto, namedClient, &namedDump, disabled, system, builder.Provider(), "bindecode", named);
    EXPECT_EQ(namedDump, Built);
    EXPECT_EQ(named.str(), std::string("bindecode: using the type dump ") + Built + " built from C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610); pass --type-dump to choose otherwise\n");

    std::ostringstream failing;
    builder.Result.reset();
    std::optional<std::string> failedDump;
    ClientSetup::ForTool(SetupMode::Auto, namedClient, &failedDump, disabled, system, builder.Provider(), "bindecode", failing);
    EXPECT_FALSE(failedDump);
    EXPECT_EQ(failing.str(), "bindecode: cannot build the type dump for C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610): typeextract exited with 1\n");
    builder.Result = std::filesystem::path(Built);

    std::ostringstream notInstall;
    std::optional<std::string> wrongClient = std::string("E:/Nothing");
    std::optional<std::string> wrongDump;
    builder.Calls.clear();
    ClientSetupResult const wrong = ClientSetup::ForTool(SetupMode::Auto, wrongClient, &wrongDump, disabled, system, builder.Provider(), "extractor", notInstall);
    EXPECT_FALSE(wrongDump);
    EXPECT_FALSE(wrong.Install);
    EXPECT_TRUE(builder.Calls.empty());
    EXPECT_EQ(wrong.TypeDumpError, "E:/Nothing holds no Wizard101 install, so no type dump can be built from it");
    EXPECT_EQ(notInstall.str(), "extractor: E:/Nothing holds no Wizard101 install, so no type dump can be built from it\n");

    FakeClientSystem single;
    single.Environment["ProgramData"] = "C:/ProgramData";
    single.AddInstall(Install, "r806919.Wizard_1_610");
    single.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    std::ostringstream asked;
    SetupPrompt interactive(std::make_unique<ScriptedPromptInput>(std::vector<std::string>{ "1", "" }, true, counters), out, true, std::chrono::seconds(30));
    std::optional<std::string> askedClient;
    std::optional<std::string> askedDump;
    ClientSetup::ForTool(SetupMode::Ask, askedClient, &askedDump, interactive, single, builder.Provider(), "extractor", asked);
    EXPECT_EQ(askedClient, Install);
    EXPECT_EQ(askedDump, Dump);
    EXPECT_EQ(asked.str(), "");

    FakeClientSystem noDump;
    noDump.Environment["ProgramData"] = "C:/ProgramData";
    noDump.AddInstall(Install, "r806919.Wizard_1_610");
    std::ostringstream building;
    SetupPrompt yes(std::make_unique<ScriptedPromptInput>(std::vector<std::string>{ "", "yes" }, true, counters), out, true, std::chrono::seconds(30));
    std::optional<std::string> buildClient;
    std::optional<std::string> buildDump;
    builder.Calls.clear();
    ClientSetup::ForTool(SetupMode::Ask, buildClient, &buildDump, yes, noDump, builder.Provider(), "extractor", building);
    EXPECT_EQ(buildDump, Built);
    EXPECT_EQ(builder.Calls, std::vector<std::string>{ Install });
    EXPECT_NE(out.str().find("extractor needs the type dump made from your install, and none was named. Build it from C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610) now? [Y/n]: "), std::string::npos) << out.str();

    std::ostringstream piped;
    std::optional<std::string> pipedClient;
    std::optional<std::string> pipedDump;
    SetupPrompt notTerminal(nullptr, out, true, std::chrono::seconds(30));
    ClientSetup::ForTool(SetupMode::Ask, pipedClient, &pipedDump, notTerminal, single, builder.Provider(), "extractor", piped);
    EXPECT_FALSE(pipedClient);
    EXPECT_FALSE(pipedDump);
    EXPECT_NE(piped.str().find("extractor: Wizard101 was found on this machine: C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610), found through "), std::string::npos) << piped.str();
    EXPECT_NE(piped.str().find(". Pass --client with one of them, or run extractor in a terminal whose input and output are not redirected to choose\n"), std::string::npos) << piped.str();

    std::ostringstream off;
    std::optional<std::string> offClient;
    std::optional<std::string> offDump;
    builder.Calls.clear();
    ClientSetup::ForTool(SetupMode::Off, offClient, &offDump, disabled, single, builder.Provider(), "extractor", off);
    EXPECT_FALSE(offClient);
    EXPECT_FALSE(offDump);
    EXPECT_TRUE(builder.Calls.empty());
    EXPECT_NE(off.str().find(". Pass --client with one of them, or set AMBROSE_SETUP_MODE=auto to use the newest\n"), std::string::npos) << off.str();
    EXPECT_NE(off.str().find(std::string("extractor: a type dump was found on this machine: ") + Dump), std::string::npos) << off.str();
    EXPECT_NE(off.str().find(". Pass --type-dump with one of them, or set AMBROSE_SETUP_MODE=auto to build one from the install\n"), std::string::npos) << off.str();

    std::ostringstream pipedNoDump;
    std::optional<std::string> pipedNamed = std::string(Install);
    std::optional<std::string> pipedNamedDump;
    ClientSetup::ForTool(SetupMode::Ask, pipedNamed, &pipedNamedDump, notTerminal, noDump, builder.Provider(), "extractor", pipedNoDump);
    EXPECT_FALSE(pipedNamedDump);
    EXPECT_EQ(pipedNoDump.str(), "extractor: no type dump was found for C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610). Pass --type-dump, or run extractor in a terminal whose input and output are not redirected to be asked to build it\n");
    std::ostringstream offNoDump;
    std::optional<std::string> offNamed = std::string(Install);
    std::optional<std::string> offNamedDump;
    ClientSetup::ForTool(SetupMode::Off, offNamed, &offNamedDump, disabled, noDump, builder.Provider(), "extractor", offNoDump);
    EXPECT_FALSE(offNamedDump);
    EXPECT_TRUE(builder.Calls.empty());
    EXPECT_EQ(offNoDump.str(), "extractor: no type dump was found for C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610). Pass --type-dump, or set AMBROSE_SETUP_MODE=auto to build it\n");

    FakeClientSystem otherRevision;
    otherRevision.Environment["ProgramData"] = "C:/ProgramData";
    otherRevision.AddInstall(Install, "r806919.Wizard_1_610");
    otherRevision.AddInstall(Newer, "r900000.Wizard_1_700");
    otherRevision.AddFile(Dump, "{\"version\": 2, \"classes\": {}}");
    std::ostringstream questions;
    std::ostringstream otherErr;
    SetupPrompt declineBuild(std::make_unique<ScriptedPromptInput>(std::vector<std::string>{ "n", "1" }, true, counters), questions, true, std::chrono::seconds(30));
    std::optional<std::string> newerClient = std::string(Newer);
    std::optional<std::string> otherDump;
    builder.Calls.clear();
    ClientSetup::ForTool(SetupMode::Ask, newerClient, &otherDump, declineBuild, otherRevision, builder.Provider(), "extractor", otherErr);
    EXPECT_NE(questions.str().find("extractor needs the type dump made from your install, and none was named. Build it from D:/Games/Wizard101 (r900000.Wizard_1_700) now? [Y/n]: "), std::string::npos) << questions.str();
    EXPECT_NE(questions.str().find("No type dump found is known to fit D:/Games/Wizard101 (r900000.Wizard_1_700), so choose one only if it was made from that install. Found on this machine:\n  1) " + std::string(Dump)), std::string::npos) << questions.str();
    EXPECT_EQ(otherDump, Dump);
    EXPECT_TRUE(builder.Calls.empty());

    builder.Current = std::filesystem::path(Built);
    std::ostringstream currentErr;
    std::optional<std::string> currentClient = std::string(Install);
    std::optional<std::string> currentDump;
    ClientSetup::ForTool(SetupMode::Ask, currentClient, &currentDump, notTerminal, noDump, builder.Provider(), "extractor", currentErr);
    EXPECT_EQ(currentDump, Built);
    EXPECT_EQ(currentErr.str(), std::string("extractor: using the type dump ") + Built + " built from C:/ProgramData/KingsIsle Entertainment/Wizard101 (r806919.Wizard_1_610); pass --type-dump to choose otherwise\n");
    EXPECT_TRUE(builder.Calls.empty());
    std::ostringstream offCurrentErr;
    std::optional<std::string> offCurrentDump;
    builder.Lookups.clear();
    ClientSetup::ForTool(SetupMode::Off, currentClient, &offCurrentDump, disabled, noDump, builder.Provider(), "extractor", offCurrentErr);
    EXPECT_FALSE(offCurrentDump);
    EXPECT_TRUE(builder.Lookups.empty());
    builder.Current.reset();

    std::ostringstream modes;
    FakeClientSystem environment;
    EXPECT_EQ(ClientSetup::ModeForTool(environment, modes, "localetool"), SetupMode::Auto);
    environment.Environment["AMBROSE_SETUP_MODE"] = "  Off ";
    EXPECT_EQ(ClientSetup::ModeForTool(environment, modes, "localetool"), SetupMode::Off);
    environment.Environment["AMBROSE_SETUP_MODE"] = "ASK";
    EXPECT_EQ(ClientSetup::ModeForTool(environment, modes, "localetool"), SetupMode::Ask);
    EXPECT_EQ(modes.str(), "");
    environment.Environment["AMBROSE_SETUP_MODE"] = "sometimes";
    EXPECT_EQ(ClientSetup::ModeForTool(environment, modes, "localetool"), SetupMode::Auto);
    EXPECT_EQ(modes.str(), "localetool: AMBROSE_SETUP_MODE 'sometimes' is not auto, ask or off, so setup runs in auto mode\n");
    EXPECT_EQ(ClientSetup::ToolPrompt(out, SetupMode::Auto)->GetStatus(), SetupPrompt::Status::Disabled);
    EXPECT_EQ(ClientSetup::ToolPrompt(out, SetupMode::Off)->GetStatus(), SetupPrompt::Status::Disabled);
}

TEST(ClientSetupTest, BuiltTypeDumpsReportABuildAndNameWhyItFailed)
{
    LogTestDirectory directory;
    std::filesystem::path const root = directory.Path() / "install";
    std::filesystem::create_directories(root / "Bin");
    std::ofstream(root / "Bin" / "WizardGraphicalClient.exe", std::ios::binary) << "MZ not really a program";
    ClientInstall install;
    install.Root = root;
    install.Revision = "r900000.Wizard_1_700";
    install.HasProgram = true;

    FakeClientSystem system;
    system.Environment["LOCALAPPDATA"] = ConfigMgr::PathToUtf8(directory.Path() / "local");
    system.Executable = directory.Path() / "bin";
    std::filesystem::path const extractor = TypeDumpCache::DefaultExtractor(system.Executable);
    std::filesystem::create_directories(system.Executable);
    std::ofstream(extractor, std::ios::binary) << "not a program";
    std::vector<std::pair<bool, std::string>> reports;
    ClientSetup::TypeDumpProvider const provider = ClientSetup::BuiltTypeDumps(system, {}, std::chrono::seconds(30), [&reports](bool warning, std::string const& text) { reports.emplace_back(warning, text); }, [] { return false; });
    std::string error;
    EXPECT_FALSE(provider(install, TypeDumpBuild::Never, error));
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(reports.empty());
    error.clear();
    EXPECT_FALSE(provider(install, TypeDumpBuild::IfNeeded, error));
    EXPECT_FALSE(error.empty());
    auto const building = std::find_if(reports.begin(), reports.end(), [](auto const& report) { return report.second.starts_with("Building the type dump for "); });
    ASSERT_NE(building, reports.end());
    EXPECT_FALSE(building->first);
    std::optional<std::filesystem::path> const target = TypeDumpCache::PathFor(ClientLocator::GetDataFolder(system), install.Revision);
    ASSERT_TRUE(target);
    EXPECT_EQ(building->second, "Building the type dump for revision r900000.Wizard_1_700 from " + ClientLocator::PathText(root) + " into " + ClientLocator::PathText(*target) + " with " + ClientLocator::PathText(extractor) + "; this can take a minute or two");

    std::string hashError;
    std::optional<std::string> const sha256 = TypeDumpCache::ExecutableSha256(install, hashError);
    ASSERT_TRUE(sha256) << hashError;
    std::filesystem::create_directories(target->parent_path());
    auto const writeDump = [&target](std::string const& revision, std::string const& executable)
    {
        std::ofstream(*target, std::ios::binary | std::ios::trunc) << "{\"version\": 2, \"revision\": \"" << revision << "\", \"executable_sha256\": \"" << executable << "\", \"extractor\": \"a test\", \"classes\": {}}";
    };
    reports.clear();
    writeDump(install.Revision, *sha256);
    std::string currentError;
    std::optional<std::filesystem::path> const current = provider(install, TypeDumpBuild::Never, currentError);
    ASSERT_TRUE(current) << currentError;
    EXPECT_TRUE(std::filesystem::equivalent(*current, *target));
    EXPECT_TRUE(reports.empty());

    writeDump(install.Revision, std::string(64, '0'));
    std::string staleError;
    EXPECT_FALSE(provider(install, TypeDumpBuild::Never, staleError));
    EXPECT_NE(staleError.find("was built from another client program"), std::string::npos) << staleError;
    writeDump("r806919.Wizard_1_610", *sha256);
    std::string otherError;
    EXPECT_FALSE(provider(install, TypeDumpBuild::Never, otherError));
    EXPECT_NE(otherError.find("is not a type dump built for"), std::string::npos) << otherError;
    EXPECT_TRUE(reports.empty());

    FakeClientSystem homeless;
    ClientSetup::TypeDumpProvider const nowhere = ClientSetup::BuiltTypeDumps(homeless, {}, std::chrono::seconds(30), {}, {});
    std::string nowhereError;
    EXPECT_FALSE(nowhere(install, TypeDumpBuild::IfNeeded, nowhereError));
    EXPECT_FALSE(nowhereError.empty());
    std::string lookupError;
    EXPECT_FALSE(nowhere(install, TypeDumpBuild::Never, lookupError));
    EXPECT_FALSE(lookupError.empty());
}
