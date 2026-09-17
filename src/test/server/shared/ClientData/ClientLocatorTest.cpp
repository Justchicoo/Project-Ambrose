/*
 * Project Ambrose by Imjustchico
 * Tests client discovery on machines the test describes: an install is a folder with Root.wad whose revision.dat names its revision, noting its client program and keeping no trailing separator; installs are found through AMBROSE_CLIENT_DIR, installed programs named Wizard101 up to two folders deep with quoted locations cleaned, KingsIsle's default folders and Steam libraries on Windows, compared case-insensitively there, and through Steam including Snap, Proton, Wine and Lutris prefixes and WSL drives with their per-user and Steam folders elsewhere, each listed once newest revision first even through symbolic links; Steam library files parse in both layouts, and paths that are not valid Unicode convert without throwing; a search visits a bounded number of folders, walks a folder once, spends none of that budget on duplicate Steam folders, searches every fixed place, installed programs' own folders among them, before walking a wide installed program, and gives Lutris and Proton prefixes budgets of their own, so every prefix one listing returns and every other place is searched and a later library's prefixes are skipped once the Proton budget is spent; type dumps are found beside installs, in the data folder's types folder for a found revision, in the data folder, the working and executable folders and the environment, only when their header parses as an object with a root key named version or classes; and the data folder takes XDG_DATA_HOME only when it is an absolute path.
 */

#include "ClientLocator.h"
#include "FakeClientSystem.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <utility>
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

    bool Contains(std::vector<std::string> const& values, std::string const& value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }

    uint64 RevisionNumberOf(std::string revision)
    {
        ClientInstall install;
        install.Revision = std::move(revision);
        return install.RevisionNumber();
    }
}

TEST(ClientLocatorTest, AnInstallNeedsRootWadAndReadsItsRevisionAndProgram)
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
    EXPECT_TRUE(pinned->HasProgram);
    EXPECT_EQ(pinned->RevisionNumber(), 806919u);
    std::optional<ClientInstall> const spaced = ClientInstall::Inspect(system, "C:/Games/Spaced");
    ASSERT_TRUE(spaced);
    EXPECT_EQ(spaced->Revision, "r806919");
    EXPECT_TRUE(spaced->IsPinned());
    EXPECT_FALSE(spaced->HasProgram);
    std::optional<ClientInstall> const garbage = ClientInstall::Inspect(system, "C:/Games/Garbage");
    ASSERT_TRUE(garbage);
    EXPECT_EQ(garbage->Revision, "");
    EXPECT_FALSE(garbage->IsPinned());
    EXPECT_EQ(garbage->RevisionNumber(), 0u);
    EXPECT_NE(garbage->Describe().find("(revision unknown)"), std::string::npos);
    EXPECT_FALSE(ClientInstall::Inspect(system, "C:/Games/NoWad"));
    EXPECT_FALSE(ClientInstall::Inspect(system, ""));

    ClientInstall later;
    later.Revision = "r8069190.Wizard_1_620";
    EXPECT_FALSE(later.IsPinned());
    later.Revision = "r806919.Wizard_1_611";
    EXPECT_TRUE(later.IsPinned());

    EXPECT_EQ(RevisionNumberOf("r1"), 1u);
    EXPECT_EQ(RevisionNumberOf("r12abc"), 12u);
    EXPECT_EQ(RevisionNumberOf("r"), 0u);
    EXPECT_EQ(RevisionNumberOf("806919"), 0u);
    EXPECT_EQ(RevisionNumberOf("R806919"), 0u);
    EXPECT_EQ(RevisionNumberOf("rX1"), 0u);
    EXPECT_EQ(RevisionNumberOf("r18446744073709551615"), std::numeric_limits<uint64>::max());
    EXPECT_EQ(RevisionNumberOf("r18446744073709551616"), 0u);
}

TEST(ClientLocatorTest, AnInstallRootKeepsNoTrailingSeparatorSoTheDumpBesideItIsFound)
{
    FakeClientSystem system;
    system.AddInstall("C:/Games/KingsIsle/Wizard101", "r1", false);
    system.AddFile("C:/Games/KingsIsle/r1.json", DumpHeader);

    std::vector<std::string> spellings = { "C:/Games/KingsIsle/Wizard101/", "C:/Games/KingsIsle/Wizard101//" };
#ifdef _WIN32
    spellings.emplace_back("C:\\Games\\KingsIsle\\Wizard101\\");
#endif
    for (std::string const& spelling : spellings)
    {
        std::optional<ClientInstall> const install = ClientInstall::Inspect(system, spelling);
        ASSERT_TRUE(install) << spelling;
        EXPECT_EQ(ClientLocator::PathText(install->Root), "C:/Games/KingsIsle/Wizard101") << spelling;
        EXPECT_FALSE(install->HasProgram);
        EXPECT_EQ(install->Describe(), "C:/Games/KingsIsle/Wizard101 (r1)");
        std::vector<TypeDumpCandidate> const dumps = ClientLocator::FindTypeDumps(system, { ClientCandidate{ *install, "test" } });
        EXPECT_EQ(Paths(dumps), (std::vector<std::string>{ "C:/Games/KingsIsle/r1.json" })) << spelling;
    }
}

TEST(ClientLocatorTest, WindowsInstallsAreFoundOnceNewestRevisionFirst)
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
    system.Uninstall.push_back({ "Wizard101 Copy", "E:/Copy" });
    system.AddInstall("E:/Copy", "");
    system.Uninstall.push_back({ "Wizard101 Twin", "F:/Twin" });
    system.AddInstall("F:/Twin", "r806919");
    system.Steam = "C:/Program Files (x86)/Steam";
    system.AddFile("C:/Program Files (x86)/Steam/steamapps/libraryfolders.vdf", "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:/Program Files (x86)/Steam\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"K:/SteamLibrary\"\n\t}\n}\n");
    system.AddInstall("K:/SteamLibrary/steamapps/common/Wizard101", "");

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    EXPECT_EQ(Roots(found), (std::vector<std::string>{ "C:/ProgramData/KingsIsle Entertainment/Wizard101", "F:/Twin", "D:/Apps/Launcher/data/V_r806919.Wizard_1_610", "E:/Copy", "K:/SteamLibrary/steamapps/common/Wizard101" }));
    ASSERT_EQ(found.size(), 5u);
    EXPECT_EQ(found[0].Source, "AMBROSE_CLIENT_DIR");
    EXPECT_EQ(found[1].Source, "the installed program Wizard101 Twin");
    EXPECT_EQ(found[2].Source, "the installed program Wizard101 Launcher");
    EXPECT_EQ(found[3].Source, "the installed program Wizard101 Copy");
    EXPECT_EQ(found[4].Source, "the Steam library K:/SteamLibrary");
    EXPECT_EQ(found[4].Install.Revision, "");
    EXPECT_TRUE(found[0].Install.HasProgram);
}

TEST(ClientLocatorTest, WindowsPathsCompareCaseInsensitivelyAndQuotedLocationsAreCleaned)
{
    FakeClientSystem system;
    system.Environment["ProgramFiles(x86)"] = "C:\\Program Files (x86)";
    system.AddInstall("C:/Program Files (x86)/Steam/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.Steam = "c:/program files (x86)/steam";
    system.AddFile("C:/Program Files (x86)/Steam/steamapps/libraryfolders.vdf", "\"libraryfolders\" { \"0\" { \"path\" \"C:\\\\Program Files (x86)\\\\Steam\" } }");
    system.Uninstall.push_back({ "Wizard101", "  \"C:\\Program Files (x86)\\Steam\\steamapps\\common\\WIZARD101\\\"  " });
    system.Uninstall.push_back({ "Wizard101 (quoted)", "\"D:\\Games\\Wizard101\"" });
    system.AddInstall("D:/Games/Wizard101", "r1");

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    EXPECT_EQ(Roots(found), (std::vector<std::string>{ "C:/Program Files (x86)/Steam/steamapps/common/WIZARD101", "D:/Games/Wizard101" }));
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].Source, "the installed program Wizard101");
    EXPECT_EQ(found[1].Source, "the installed program Wizard101 (quoted)");

    EXPECT_TRUE(system.IsFile("c:/PROGRAM FILES (X86)/steam/STEAMAPPS/common/wizard101/data/gamedata/root.wad"));
    system.Windows = false;
    EXPECT_FALSE(system.IsFile("c:/PROGRAM FILES (X86)/steam/STEAMAPPS/common/wizard101/data/gamedata/root.wad"));
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
    system.AddInstall("/home/wiz/snap/steam/common/.local/share/Steam/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.AddFolder("/mnt/c/Windows");
    system.AddInstall("/mnt/c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/mnt/d/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddFile("/mnt/c/Program Files (x86)/Steam/config/libraryfolders.vdf", "\"libraryfolders\" { \"0\" { \"path\" \"C:\\\\Program Files (x86)\\\\Steam\" } \"1\" { \"path\" \"D:\\\\SteamLibrary\" } }");
    system.AddInstall("/mnt/d/SteamLibrary/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/mnt/c/Users/wiz/AppData/Local/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddFolder("/mnt/c/Users/Public/AppData/Local");

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    std::vector<std::string> const roots = Roots(found);
    std::vector<std::string> const expected = { "/home/wiz/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "/home/wiz/Games/wizard101/drive_c/Program Files (x86)/KingsIsle Entertainment/Wizard101",
        "/mnt/games/steamapps/common/Wizard101", "/mnt/games/steamapps/compatdata/4242/pfx/drive_c/Program Files/KingsIsle Entertainment/Wizard101", "/mnt/c/ProgramData/KingsIsle Entertainment/Wizard101",
        "/home/wiz/snap/steam/common/.local/share/Steam/steamapps/common/Wizard101", "/mnt/d/SteamLibrary/steamapps/common/Wizard101", "/mnt/c/Users/wiz/AppData/Local/KingsIsle Entertainment/Wizard101" };
    for (std::string const& path : expected)
        EXPECT_TRUE(Contains(roots, path)) << path;
    EXPECT_FALSE(Contains(roots, "/mnt/d/ProgramData/KingsIsle Entertainment/Wizard101"));
    EXPECT_EQ(found.size(), expected.size());
    for (ClientCandidate const& candidate : found)
    {
        if (FakeClientSystem::Key(candidate.Install.Root) == "/mnt/d/SteamLibrary/steamapps/common/Wizard101")
        {
            EXPECT_EQ(candidate.Source, "the Steam library /mnt/d/SteamLibrary");
        }
        if (FakeClientSystem::Key(candidate.Install.Root) == "/mnt/c/Users/wiz/AppData/Local/KingsIsle Entertainment/Wizard101")
        {
            EXPECT_EQ(candidate.Source, "the Windows user folder /mnt/c/Users/wiz");
        }
    }

    FakeClientSystem windows;
    windows.AddInstall("/home/wiz/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919");
    windows.Environment["HOME"] = "/home/wiz";
    EXPECT_TRUE(ClientLocator::FindInstalls(windows).empty());

    FakeClientSystem emptyHome;
    emptyHome.Windows = false;
    emptyHome.Environment["HOME"] = "";
    emptyHome.AddInstall(".local/share/Steam/steamapps/common/Wizard101", "r806919");
    emptyHome.AddInstall(".wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919");
    EXPECT_FALSE(emptyHome.GetEnv("HOME"));
    EXPECT_TRUE(ClientLocator::FindInstalls(emptyHome).empty());
}

TEST(ClientLocatorTest, SymlinkedSteamRootsListAnInstallOnce)
{
    FakeClientSystem system;
    system.Windows = false;
    system.Environment["HOME"] = "/home/u";
    system.AddInstall("/home/u/.local/share/Steam/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.AddLink("/home/u/.steam/steam", "/home/u/.local/share/Steam");
    system.AddLink("/home/u/.steam/root", "/home/u/.steam/steam");
    system.AddFile("/home/u/.local/share/Steam/steamapps/libraryfolders.vdf", "\"libraryfolders\" { \"0\" { \"path\" \"/home/u/.local/share/Steam\" } }");
    system.AddFile("/home/u/.local/share/Steam/config/libraryfolders.vdf", "\"libraryfolders\" { \"0\" { \"path\" \"/home/u/.steam/root\" } }");

    EXPECT_EQ(FakeClientSystem::Key(system.Canonical("/home/u/.steam/root/steamapps")), "/home/u/.local/share/Steam/steamapps");
    EXPECT_TRUE(system.IsDirectory("/home/u/.steam/steam/steamapps/common/Wizard101"));
    std::vector<std::filesystem::path> const children = system.ListDirectories("/home/u/.steam", 8);
    ASSERT_EQ(children.size(), 2u);
    EXPECT_EQ(FakeClientSystem::Key(children[0]), "/home/u/.steam/root");

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    EXPECT_EQ(Roots(found), (std::vector<std::string>{ "/home/u/.steam/steam/steamapps/common/Wizard101" }));
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].Source, "the Steam library /home/u/.steam/steam");
}

TEST(ClientLocatorTest, DuplicateSteamFoldersAndLibraryFilesSpendNoExtraFolderQueries)
{
    FakeClientSystem system;
    system.Windows = false;
    system.Environment["HOME"] = "/home/u";
    system.AddLink("/home/u/.steam/steam", "/home/u/.local/share/Steam");
    std::string const libraries = "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"/home/u/.local/share/Steam\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"/mnt/games\"\n\t}\n}\n";
    system.AddFile("/home/u/.local/share/Steam/steamapps/libraryfolders.vdf", libraries);
    system.AddFile("/home/u/.local/share/Steam/config/libraryfolders.vdf", libraries);
    for (int prefix = 0; prefix < 85; ++prefix)
        system.AddFolder("/home/u/.local/share/Steam/steamapps/compatdata/" + std::to_string(1000 + prefix) + "/pfx/drive_c/Program Files");
    system.AddInstall("/home/u/.local/share/Steam/steamapps/compatdata/999999/pfx/drive_c/Program Files (x86)/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/home/u/.local/share/Steam/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/mnt/games/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/home/u/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/home/u/Games/wizard101/drive_c/Program Files (x86)/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    std::vector<std::string> const roots = Roots(found);
    EXPECT_EQ(found.size(), 5u);
    EXPECT_TRUE(Contains(roots, "/home/u/.steam/steam/steamapps/common/Wizard101"));
    EXPECT_TRUE(Contains(roots, "/mnt/games/steamapps/common/Wizard101"));
    EXPECT_TRUE(Contains(roots, "/home/u/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101"));
    EXPECT_TRUE(Contains(roots, "/home/u/Games/wizard101/drive_c/Program Files (x86)/KingsIsle Entertainment/Wizard101"));
    EXPECT_TRUE(Contains(roots, "/home/u/.steam/steam/steamapps/compatdata/999999/pfx/drive_c/Program Files (x86)/KingsIsle Entertainment/Wizard101"));
    EXPECT_LE(system.DirectoryQueries, 26u + 4u + 8u + 86u * 4u);
}

TEST(ClientLocatorTest, LutrisAndProtonPrefixesSpendTheirOwnBudgetsSoEveryListedPrefixAndEveryOtherPlaceIsSearched)
{
    FakeClientSystem system;
    system.Windows = false;
    system.Environment["HOME"] = "/home/u";
    system.AddFile("/home/u/.local/share/Steam/steamapps/libraryfolders.vdf", "\"libraryfolders\" { \"0\" { \"path\" \"/home/u/.local/share/Steam\" } \"1\" { \"path\" \"/mnt/games\" } }");
    std::string const compatdata = "/home/u/.local/share/Steam/steamapps/compatdata/";
    std::string const games = "/home/u/Games/";
    for (std::size_t prefix = 0; prefix + 1 < ClientLocator::MaxPrefixes; ++prefix)
    {
        system.AddFolder(compatdata + std::to_string(1000 + prefix) + "/pfx/drive_c/users");
        system.AddFolder(games + "game" + std::to_string(1000 + prefix) + "/drive_c/users");
    }
    std::string const lastProton = compatdata + "999999/pfx/drive_c/Program Files/KingsIsle Entertainment/Wizard101";
    std::string const lastLutris = games + "wizard101/drive_c/Program Files (x86)/KingsIsle Entertainment/Wizard101";
    system.AddInstall(lastProton, "r806919.Wizard_1_610");
    system.AddInstall(lastLutris, "r806919.Wizard_1_610");
    system.AddInstall("/mnt/games/steamapps/compatdata/4242/pfx/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/home/u/.local/share/Steam/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/mnt/games/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.AddInstall("/home/u/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.AddFolder("/mnt/c/Windows");
    system.AddInstall("/mnt/c/Users/wiz/AppData/Local/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");

    std::vector<std::filesystem::path> const listed = system.ListDirectories(compatdata, ClientLocator::MaxPrefixes + 1);
    ASSERT_EQ(listed.size(), ClientLocator::MaxPrefixes);
    EXPECT_EQ(FakeClientSystem::Key(listed.back()), compatdata + "999999");
    ASSERT_EQ(FakeClientSystem::Key(system.ListDirectories(games, ClientLocator::MaxPrefixes).back()), games + "wizard101");

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    std::vector<std::string> const roots = Roots(found);
    std::vector<std::string> const expected = { lastProton, lastLutris, "/home/u/.local/share/Steam/steamapps/common/Wizard101", "/mnt/games/steamapps/common/Wizard101",
        "/home/u/.wine/drive_c/ProgramData/KingsIsle Entertainment/Wizard101", "/mnt/c/Users/wiz/AppData/Local/KingsIsle Entertainment/Wizard101" };
    for (std::string const& path : expected)
        EXPECT_TRUE(Contains(roots, path)) << path;
    EXPECT_EQ(found.size(), expected.size());
    EXPECT_FALSE(Contains(roots, "/mnt/games/steamapps/compatdata/4242/pfx/drive_c/ProgramData/KingsIsle Entertainment/Wizard101"));
    EXPECT_LE(system.DirectoryQueries, ClientLocator::MaxFoldersVisited);
}

TEST(ClientLocatorTest, FixedPlacesAreSearchedBeforeWalkingWideInstalledProgramFolders)
{
    FakeClientSystem system;
    system.Environment["ProgramData"] = "C:/ProgramData";
    system.AddInstall("C:/ProgramData/KingsIsle Entertainment/Wizard101", "r806919.Wizard_1_610");
    system.Steam = "C:/Program Files (x86)/Steam";
    system.AddInstall("C:/Program Files (x86)/Steam/steamapps/common/Wizard101", "r806919.Wizard_1_610");
    system.Uninstall.push_back({ "Wizard101 Tools", "D:/Wide" });
    system.Uninstall.push_back({ "Wizard101", "E:/Games/Wizard101" });
    system.AddInstall("E:/Games/Wizard101", "r806919.Wizard_1_610");
    std::size_t const outerFolders = 10;
    for (std::size_t outer = 0; outer < outerFolders; ++outer)
        for (std::size_t inner = 0; inner < ClientLocator::MaxChildren; ++inner)
            system.AddFolder("D:/Wide/" + std::to_string(outer) + "/" + std::to_string(inner));
    system.AddInstall("D:/Wide/0/0", "r900000.Wizard_1_700");
    ASSERT_GT(1 + outerFolders + outerFolders * ClientLocator::MaxChildren, ClientLocator::MaxPlaceFolders);

    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    EXPECT_EQ(Roots(found), (std::vector<std::string>{ "D:/Wide/0/0", "E:/Games/Wizard101", "C:/ProgramData/KingsIsle Entertainment/Wizard101", "C:/Program Files (x86)/Steam/steamapps/common/Wizard101" }));
    ASSERT_EQ(found.size(), 4u);
    EXPECT_EQ(found[0].Source, "the installed program Wizard101 Tools");
    EXPECT_EQ(found[1].Source, "the installed program Wizard101");
    EXPECT_EQ(found[2].Source, "KingsIsle's default folder");
    EXPECT_EQ(found[3].Source, "the Steam library C:/Program Files (x86)/Steam");
    EXPECT_EQ(system.DirectoryQueries, ClientLocator::MaxPlaceFolders);
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

    std::vector<std::filesystem::path> const hostile = ClientLocator::ParseSteamLibraries("\"libraryfolders\" { \"0\" { \"path\" \"D:/Bad\xFF\xFE\" } \"1\" { \"path\" \"D:/Cut\xC3\" } }");
    ASSERT_EQ(hostile.size(), 2u);
    EXPECT_TRUE(ClientLocator::PathText(hostile[0]).starts_with("D:/Bad"));
    EXPECT_TRUE(ClientLocator::PathText(hostile[1]).starts_with("D:/Cut"));
#ifdef _WIN32
    EXPECT_NE(ClientLocator::PathText(hostile[0]).find("\xEF\xBF\xBD"), std::string::npos);
    std::filesystem::path const unpaired(std::wstring{ L'a', static_cast<wchar_t>(0xD800), L'/', L'b' });
    EXPECT_EQ(ClientLocator::PathText(unpaired), "a\xEF\xBF\xBD/b");
#endif
}

TEST(ClientLocatorTest, ASearchVisitsABoundedNumberOfFoldersAndEachFolderOnce)
{
    FakeClientSystem system;
    system.Uninstall.push_back({ "Wizard101", "C:/Huge" });
    for (int outer = 0; outer < 30; ++outer)
        for (int inner = 0; inner < 30; ++inner)
            system.AddFolder("C:/Huge/" + std::to_string(outer) + "/" + std::to_string(inner));
    system.AddInstall("C:/Huge/29/29", "r806919");
    std::vector<ClientCandidate> const found = ClientLocator::FindInstalls(system);
    EXPECT_TRUE(found.empty());
    EXPECT_EQ(system.DirectoryQueries, ClientLocator::MaxPlaceFolders);

    FakeClientSystem once;
    for (int outer = 0; outer < 6; ++outer)
        for (int inner = 0; inner < 6; ++inner)
            once.AddFolder("C:/Games/" + std::to_string(outer) + "/" + std::to_string(inner));
    once.AddInstall("C:/Games/5/5", "r806919");
    once.Environment["AMBROSE_CLIENT_DIR"] = "C:/Games/5";
    once.Uninstall.push_back({ "Wizard101", "C:/Games" });
    std::size_t const single = [&]
    {
        once.DirectoryQueries = 0;
        EXPECT_EQ(ClientLocator::FindInstalls(once).size(), 1u);
        return once.DirectoryQueries;
    }();
    once.Uninstall.push_back({ "Wizard101 Again", "c:/games/" });
    once.Uninstall.push_back({ "Wizard101 Inside", "C:/Games/5" });
    once.DirectoryQueries = 0;
    EXPECT_EQ(ClientLocator::FindInstalls(once).size(), 1u);
    EXPECT_EQ(once.DirectoryQueries, single);
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
    system.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/notes.json", "{\"name\": \"version\", \"note\": \"classes\"}");
    system.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/sorted.json", "{\"classes\": {}, \"version\": 2}");
    system.AddFile("C:/Users/wiz/AppData/Local/ProjectAmbrose/readme.txt", DumpHeader);
    system.AddFile("C:/Work/r806919.Wizard_1_610.json", DumpHeader);
    system.AddFile("C:/Ambrose/bin/r806919.Wizard_1_610.json", DumpHeader);

    std::vector<ClientCandidate> const installs = ClientLocator::FindInstalls(system);
    std::vector<TypeDumpCandidate> const dumps = ClientLocator::FindTypeDumps(system, installs);
    EXPECT_EQ(Paths(dumps), (std::vector<std::string>{ "C:/Dumps/named.json", "C:/Games/Wizard101/r806919.Wizard_1_610.json", "C:/Users/wiz/AppData/Local/ProjectAmbrose/types/r806919.Wizard_1_610.json",
        "C:/Users/wiz/AppData/Local/ProjectAmbrose/mine.JSON", "C:/Users/wiz/AppData/Local/ProjectAmbrose/sorted.json", "C:/Work/r806919.Wizard_1_610.json", "C:/Ambrose/bin/r806919.Wizard_1_610.json" }));
    ASSERT_EQ(dumps.size(), 7u);
    EXPECT_EQ(dumps[0].Source, "through AMBROSE_TYPE_DUMP_PATH");
    EXPECT_EQ(dumps[1].Source, "beside the install C:/Games/Wizard101");
    EXPECT_EQ(dumps[2].Source, "extracted into the Ambrose data folder");
    EXPECT_EQ(dumps[3].Source, "in the Ambrose data folder");
    EXPECT_EQ(dumps[5].Source, "in the working folder");
    EXPECT_EQ(dumps[6].Source, "in the executable's folder");

}

TEST(ClientLocatorTest, TheDataFolderUsesXdgDataHomeOnlyWhenItIsAnAbsolutePath)
{
    FakeClientSystem windows;
    EXPECT_TRUE(ClientLocator::GetDataFolder(windows).empty());
    windows.Environment["LOCALAPPDATA"] = "C:/Users/wiz/AppData/Local";
    windows.Environment["XDG_DATA_HOME"] = "/data";
    EXPECT_EQ(FakeClientSystem::Key(ClientLocator::GetDataFolder(windows)), "C:/Users/wiz/AppData/Local/ProjectAmbrose");

    FakeClientSystem unixLike;
    unixLike.Windows = false;
    EXPECT_TRUE(ClientLocator::GetDataFolder(unixLike).empty());
    unixLike.Environment["HOME"] = "/home/wiz";
    EXPECT_EQ(FakeClientSystem::Key(ClientLocator::GetDataFolder(unixLike)), "/home/wiz/.local/share/project-ambrose");
    unixLike.Environment["XDG_DATA_HOME"] = "/data";
    EXPECT_EQ(FakeClientSystem::Key(ClientLocator::GetDataFolder(unixLike)), "/data/project-ambrose");
    for (char const* relative : { "data", "./data", "~/data", "C:/data" })
    {
        unixLike.Environment["XDG_DATA_HOME"] = relative;
        EXPECT_EQ(FakeClientSystem::Key(ClientLocator::GetDataFolder(unixLike)), "/home/wiz/.local/share/project-ambrose") << relative;
    }
    unixLike.Environment.erase("HOME");
    EXPECT_TRUE(ClientLocator::GetDataFolder(unixLike).empty());
    unixLike.Environment["XDG_DATA_HOME"] = "/data";
    EXPECT_EQ(FakeClientSystem::Key(ClientLocator::GetDataFolder(unixLike)), "/data/project-ambrose");
}

TEST(ClientLocatorTest, ATypeDumpHeaderNamesVersionOrClassesAsAKeyInAnyOrder)
{
    FakeClientSystem system;
    auto const looks = [&system](std::string const& content)
    {
        system.AddFile("C:/Dumps/candidate.json", content);
        return ClientLocator::LooksLikeTypeDump(system, "C:/Dumps/candidate.json");
    };
    EXPECT_TRUE(looks("{\"classes\": {\"A\": {\"properties\": []}}, \"version\": 2}"));
    EXPECT_TRUE(looks("\xEF\xBB\xBF\n\t {\n \"version\" : 2"));
    EXPECT_TRUE(looks("{\"version\": 2}"));
    EXPECT_TRUE(looks("{\"revision\": \"r1\", \"classes\"\n:\n{"));
    EXPECT_TRUE(looks("{\"padding\": \"" + std::string(4000, 'x') + "\", \"classes\": {}}"));
    EXPECT_FALSE(looks("{\"padding\": \"" + std::string(5000, 'x') + "\", \"classes\": {}, \"version\": 2}"));
    EXPECT_FALSE(looks("{\"name\": \"version\", \"other\": \"classes\"}"));
    EXPECT_FALSE(looks("{\"meta\": {\"version\": 2}, \"list\": [{\"classes\": {}}]}"));
    EXPECT_FALSE(looks("{\"text\": \"\\\"version\\\": 2\"}"));
    EXPECT_FALSE(looks("{ x, \"version\": 2}"));
    EXPECT_FALSE(looks("{} {\"version\": 2}"));
    EXPECT_FALSE(looks("\xFF\xFE{\"version\": 2}"));
    EXPECT_FALSE(looks("[{\"version\": 2, \"classes\": {}}]"));
    EXPECT_FALSE(looks("\"version\": 2, \"classes\": {}"));
    EXPECT_FALSE(looks(""));
    EXPECT_FALSE(ClientLocator::LooksLikeTypeDump(system, "C:/Dumps/missing.json"));
    EXPECT_FALSE(ClientLocator::LooksLikeTypeDump(system, ""));
}
