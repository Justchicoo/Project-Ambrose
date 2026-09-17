/*
 * Project Ambrose by Imjustchico
 * Finds the user's own Wizard101 client data on their machine, and judges whether a path is absolute for the machine described rather than for the host the code runs on: an install is a folder holding Data/GameData/Root.wad whose Bin/revision.dat names its revision, noting whether Bin/WizardGraphicalClient.exe is there, found through AMBROSE_CLIENT_DIR, the installed programs named Wizard101, KingsIsle's default folders, every Steam library including Flatpak and Snap Steam, Wine, Lutris and Proton prefixes, and on WSL the Windows drives' KingsIsle, per-user and Steam folders, with one budget of folder queries for those places and the walks below installed programs and a budget of its own for the Lutris prefixes and another for the Proton prefixes, each covering every prefix one listing returns, listed newest revision first; and type dumps: the file AMBROSE_TYPE_DUMP_PATH names, a dump named for a found revision in or beside an install, in the Ambrose data folder's types folder, the working folder or the executable's folder, and any .json in the Ambrose data folder itself whose header reads as a dump; the data folder is ProjectAmbrose in LOCALAPPDATA on Windows, and elsewhere project-ambrose in XDG_DATA_HOME when that is an absolute path, or else in ~/.local/share.
 */

#ifndef AMBROSE_CLIENTLOCATOR_H
#define AMBROSE_CLIENTLOCATOR_H

#include "ClientSystem.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ClientInstall
{
    static constexpr std::string_view PinnedRevision = "r806919";

    std::filesystem::path Root;
    std::string Revision;
    bool HasProgram = false;

    bool IsPinned() const noexcept;
    uint64 RevisionNumber() const noexcept;
    std::string Describe() const;

    static std::optional<ClientInstall> Inspect(ClientSystem const& system, std::filesystem::path const& root);
};

struct ClientCandidate
{
    ClientInstall Install;
    std::string Source;
};

struct TypeDumpCandidate
{
    std::filesystem::path Path;
    std::string Source;
};

class ClientLocator
{
public:
    static constexpr std::size_t MaxPlaceFolders = 512;
    static constexpr std::size_t MaxPrefixes = 256;
    static constexpr std::size_t FoldersPerPrefix = 4;
    static constexpr std::size_t MaxPrefixFolders = MaxPrefixes * FoldersPerPrefix;
    static constexpr std::size_t MaxFoldersVisited = MaxPlaceFolders + 2 * MaxPrefixFolders;
    static constexpr std::size_t MaxChildren = 64;
    static constexpr std::size_t MaxVdfBytes = 1024 * 1024;
    static constexpr std::size_t DumpHeaderBytes = 4096;

    ClientLocator() = delete;

    static std::vector<ClientCandidate> FindInstalls(ClientSystem const& system);
    static std::vector<TypeDumpCandidate> FindTypeDumps(ClientSystem const& system, std::vector<ClientCandidate> const& installs);
    static std::vector<std::filesystem::path> ParseSteamLibraries(std::string_view vdf);
    static bool LooksLikeTypeDump(ClientSystem const& system, std::filesystem::path const& path);
    static std::filesystem::path GetDataFolder(ClientSystem const& system);
    static std::string PathText(std::filesystem::path const& path);
    static bool IsAbsoluteFor(ClientSystem const& system, std::filesystem::path const& path);
    static std::filesystem::path AbsoluteFor(ClientSystem const& system, std::filesystem::path const& path);
};

#endif
