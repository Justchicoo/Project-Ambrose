/*
 * Project Ambrose by Imjustchico
 * Tests client discovery on machines the test describes: an install is a folder with Root.wad whose revision.dat names its revision; installs are found through AMBROSE_CLIENT_DIR, installed programs named Wizard101 up to two folders deep, KingsIsle's default folders and Steam libraries on Windows, and through Steam, Proton, Wine and Lutris prefixes and WSL drives elsewhere, each listed once with the pinned revision first; Steam library files parse in both layouts; a search visits a bounded number of folders; and type dumps are found beside installs, in the data folder's types folder for a found revision, in the data folder, the working and executable folders and the environment, only when their header reads as a dump.
 */

#include "ClientLocator.h"
#include "FakeClientSystem.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    constexpr char const* DumpHeader = "{\"version\": 2, \"classes\": {}}";

    std::vector<std::string> Roots(std::vector<ClientCandidate> const& candidates)
    {
        std::vector<std::string> roots;
        for (ClientCandidate const& candidate : candidates)
            roots.push_back(FakeClientSystem::Key(candidate.Install.Root));
        return roots;
    }

    std::vector<std::string> Paths(std::vector<TypeDumpCandidate> const& candidates)
    {
        std::vector<std::string> paths;
        for (TypeDumpCandidate const& candidate : candidates)
            paths.push_back(FakeClientSystem::Key(candidate.Path));
        return paths;
    }
}

TEST(ClientLocatorTest, AnInstallNeedsRootWadAndReadsItsRevision)
{
    FakeClientSystem system;
    system.AddInstall("C:/Games/Pinned", "r806919.Wizard_1_610");
    system.AddFile("C:/Games/Spaced/Data/GameData/Root.wad", "KIWAD");
    system.AddFile("C:/Games/Spaced/Bin/revision.dat", "  r806919  \r\n");
    system.AddFile("C:/Games/Garbage/Data/GameData/Root.wad", "KIWAD");
    system.AddFile("C:/Games/Garbage/Bin/revision.dat", std::string("r8\x01\x02", 4));
    system.AddFile("C:/Games/NoWad/Bin/revision.dat", "r806919");

    std::optional<ClientInstall> const pinned = ClientInstall::Inspect(system, "C:/Games/Pinned");
    ASSERT_TRUE(pinned);
    EXPECT_EQ(pinned->Revision, "r806919.Wizard_1_610");
    EXPECT_TRUE(pinned->IsPinned());
    std::optional<ClientInstall> const spaced = ClientInstall::Inspect(system, "C:/Games/Spaced");
    ASSERT_TRUE(spaced);
    EXPECT_EQ(spaced->Revision, "r806919");
    EXPECT_TRUE(spaced->IsPinned());
    std::optional<ClientInstall> const garbage = ClientInstall::Inspect(system, "C:/Games/Garbage");
    ASSERT_TRUE(garbage);
    EXPECT_EQ(garbage->Revision, "");
    EXPECT_FALSE(garbage->IsPinned());
    EXPECT_NE(garbage->Describe().find("(revision unknown)"), std::string::npos);
    EXPECT_FALSE(ClientInstall::Inspect(system, "C:/Games/NoWad"));
    EXPECT_FALSE(ClientInstall::Inspect(system, ""));

    ClientInstall later;
    later.Revision = "r8069190.Wizard_1_620";
    EXPECT_FALSE(later.IsPinned());
    later.Revision = "r806919.Wizard_1_611";
    EXPECT_TRUE(later.IsPinned());
}

TEST(ClientLocatorTest, WindowsInstallsAreFoundOnceWithThePinnedRevisionFirst)
{
    FakeClientSystem system;
    system.Environment["ProgramData"] = "C:/ProgramData";
    system.Environment["ProgramFiles"] = "C:/Program Files";
    system.Environment["AMBROSE_CLIENT_DIR"] = "C:/ProgramData/KingsIsle Entertainment/Wizard101/";
    system.AddInstall("C:/ProgramData/KingsIsle Entertainment/Wizard101", "r900000.Wizard_1_700");
    system.AddFolder("C:/Program Files/KingsIsle Entertainment/Wizard101/Bin");
    system.Uninstall.push_back({ "Wizard101", "C:/ProgramData/KingsIsle Entertainment/Wizard101" });
    system.Uninstall.push_back({ "Wizard101 Launcher", "D:/Apps/Launcher" });
    system.AddInstall("D:/Apps/Launcher/data/V_r806919.Wizard_1_610", "r806919.Wizard_1_610");
    system.AddInstall("D:/Apps/Launcher/data/deeper/V_r1", "r1");
    system.Uninstall.push_back({ "Another Game", "E:/Other" });
    system.AddInstall("E:/Other", "r806919.Wizard_1_610");
    system.Steam = "C:/Program Files (x86)/Steam";
    system.AddFile("C:/Program Files (x86)/Steam/steamapps/libraryfolders.vdf", "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:/Program Files (x86)/Steam\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"K:/SteamLibrary\"\n\t}\n}\n");
    system.AddInstall("K:/SteamLibrary/steamapps/common/Wizard101", "");

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    EXPECT_EQ(Roots(found), (std::vector<std::string>{ "D:/Apps/Launcher/data/V_r806919.Wizard_1_610", "C:/ProgramData/KingsIsle Entertainment/Wizard101", "K:/SteamLibrary/steamapps/common/Wizard101" }));
    ASSERT_EQ(found.size(), 3u);
    EXPECT_EQ(found[0].Source, "the installed program Wizard101 Launcher");
    EXPECT_EQ(found[1].Source, "AMBROSE_CLIENT_DIR");
    EXPECT_EQ(found[2].Source, "the Steam library K:/SteamLibrary");
    EXPECT_EQ(found[2].Install.Revision, "");
}

TEST(ClientLocatorTest, LinuxPrefixesSteamAndWslDrivesAreSearched)
{
    FakeClientSystem system;
    system.Windows = false;
    system.Environment["HOME"] = "/home/wiz";
    system.AddInstall("/home/wiz/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/home/wiz/Games/wizard101/drive_c/Program Files (x86)/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddFile("/home/wiz/.local/share/Steam/steamapps/libraryfolders.vdf", "\"libraryfolders\" { \"0\" { \"path\" \"/mnt/games\" } }");
    system.AddInstall("/mnt/games/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/mnt/games/steamapps/compatdata/4242/pfx/drive_c/Program Files/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddFolder("/mnt/c/Windows");
    system.AddInstall("/mnt/c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/mnt/d/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    std::vector<std::string> const roots = Roots(found);
    for (char const* const expected : { "/home/wiz/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "/home/wiz/Games/wizard101/drive_c/Program Files (x86)/KingsIsle Entertainment/Wizard101",
        "/mnt/games/steamapps/common/Wizard101", "/mnt/games/steamapps/compatdata/4242/pfx/drive_c/Program Files/KingsIsle Entertainment/Wizard101", "/mnt/c/ProgramData/KingsIsle Entertainment/Wizard101" })
        EXPECT_NE(std::find(roots.begin(), roots.end(), expected), roots.end()) << expected;
    EXPECT_EQ(std::find(roots.begin(), roots.end(), "/mnt/d/ProgramData/KingsIsle Entertainment/Wizard101"), roots.end());
    EXPECT_EQ(found.size(), 5u);

    FakeClientSystem windows;
    windows.AddInstall("/home/wiz/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919");
    windows.Environment["HOME"] = "/home/wiz";
    EXPECT_TRUE(ClientLocator::FindInstalls(windows).empty());
}

TEST(ClientLocatorTest, SteamLibraryFilesParseInBothLayouts)
{
    std::string const newer = "\"libraryfolders\"\n{\n\t\"contentstatsid\"\t\t\"123\"\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"228980\"\t\t\"157818239\"\n\t\t}\n\t}\n\t\"1\"\n\t{\n\t\t\"PATH\"\t\t\"K:\\\\Steam \\\"Library\\\"\"\n\t}\n}\n";
    std::vector<std::filesystem::path> const libraries = ClientLocator::ParseSteamLibraries(newer);
    ASSERT_EQ(libraries.size(), 2u);
    EXPECT_EQ(libraries[0].generic_string(), std::filesystem::path("C:\\Program Files (x86)\\Steam").generic_string());
    EXPECT_EQ(libraries[1].generic_string(), std::filesystem::path("K:\\Steam \"Library\"").generic_string());

    std::string const older = "// Steam library file\n\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\t\"1600000000\"\n\t\"ContentStatsID\"\t\t\"-42\"\n\t\"1\"\t\t\"D:\\\\Games\"\n\t\"2\"\t\t\"E:\\\\More Games\"\n}\n";
    std::vector<std::filesystem::path> const old = ClientLocator::ParseSteamLibraries(older);
    ASSERT_EQ(old.size(), 2u);
    EXPECT_EQ(old[0].generic_string(), std::filesystem::path("D:\\Games").generic_string());
    EXPECT_EQ(old[1].generic_string(), std::filesystem::path("E:\\More Games").generic_string());

    EXPECT_TRUE(ClientLocator::ParseSteamLibraries("").empty());
    EXPECT_TRUE(ClientLocator::ParseSteamLibraries("\"libraryfolders\" { \"0\" { \"path\"").empty());
    EXPECT_EQ(ClientLocator::ParseSteamLibraries("unquoted { path /srv/steam path /srv/steam }").size(), 1u);
}

TEST(ClientLocatorTest, ASearchVisitsABoundedNumberOfFolders)
{
    FakeClientSystem system;
    system.Uninstall.push_back({ "Wizard101", "C:/Huge" });
    for (int outer = 0; outer < 30; ++outer)
        for (int inner = 0; inner < 30; ++inner)
            system.AddFolder("C:/Huge/" + std::to_string(outer) + "/" + std::to_string(inner));
    system.AddInstall("C:/Huge/29/29", "r806919");
    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    EXPECT_TRUE(found.empty());
    EXPECT_LE(system.DirectoryQueries, ClientLocator::MaxFoldersVisited);
}

TEST(ClientLocatorTest, TypeDumpsAreFoundWhereTheirHeaderReadsAsADump)
{
    FakeClientSystem system;
    system.Environment["LOCALAPPDATA"] = "C:/Users/wiz/AppData/Local";
    system.Environment["AMBROSE_TYPE_DUMP_PATH"] = "C:/Dumps/named.json";
    system.Environment["AMBROSE_CLIENT_DIR"] = "C:/Games/Wizard101";
    system.Working = "C:/Work";
    system.Executable = "C:/Ambrose/bin";
    system.AddFile("C:/Dumps/named.json", std::string("\xEF\xBB\xBF  ") + DumpHeader);
    system.AddInstall("C:/Games/Wizard101", "r806919.Wizard_1_610");
    system.AddFile("C:/Games/Wizard101/r806919.Wizard_1_610.json", DumpHeader);
    system.AddFile("C:/Games/r806919.Wizard_1_610.json", "not a dump");
    system.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/mine.JSON", DumpHeader);
    system.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/types/r806919.Wizard_1_610.json", DumpHeader);
    system.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/types/r801440.Wizard_1_610.json", DumpHeader);
    system.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/notes.json", "{\"version\": 1}");
    system.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/readme.txt", DumpHeader);
    system.AddFile("C:/Work/r806919.Wizard_1_610.json", DumpHeader);
    system.AddFile("C:/Ambrose/bin/r806919.Wizard_1_610.json", DumpHeader);

    std::vector<ClientCandidate> const installs = ClientLocator::FindInstalls(system);
    std::vector<TypeDumpCandidate> const dumps = ClientLocator::FindTypeDumps(system, installs);
    EXPECT_EQ(Paths(dumps), (std::vector<std::string>{ "C:/Dumps/named.json", "C:/Games/Wizard101/r806919.Wizard_1_610.json", "C:/Users/wiz/AppData/Local/ProjectAmbrose/types/r806919.Wizard_1_610.json",
        "C:/Users/wiz/AppData/Local/ProjectAmbrose/mine.JSON", "C:/Work/r806919.Wizard_1_610.json", "C:/Ambrose/bin/r806919.Wizard_1_610.json" }));
    ASSERT_EQ(dumps.size(), 6u);
    EXPECT_EQ(dumps[0].Source, "through AMBROSE_TYPE_DUMP_PATH");
    EXPECT_EQ(dumps[2].Source, "extracted into the Ambrose data folder");
    EXPECT_EQ(dumps[3].Source, "in the Ambrose data folder");
    EXPECT_EQ(dumps[4].Source, "in the working folder");
    EXPECT_EQ(dumps[1].Source, "beside the install C:/Games/Wizard101");

    FakeClientSystem unixLike;
    unixLike.Windows = false;
    unixLike.Environment["HOME"] = "/home/wiz";
    EXPECT_EQ(FakeClientSystem::Key(ClientLocator::GetDataFolder(unixLike)), "/home/wiz/.local/share/project-ambrose");
    unixLike.Environment["XDG_DATA_HOME"] = "/data";
    EXPECT_EQ(FakeClientSystem::Key(ClientLocator::GetDataFolder(unixLike)), "/data/project-ambrose");
    EXPECT_FALSE(ClientLocator::LooksLikeTypeDump(unixLike, "/missing.json"));
}
