/*
 * Project Ambrose by Imjustchico
 * Finds the user's own Wizard101 client data on their machine: an install is a folder holding Data/GameData/Root.wad whose Bin/revision.dat names its revision, found through AMBROSE_CLIENT_DIR, the installed programs named Wizard101, KingsIsle's default folders, every Steam library, Wine, Lutris and Proton prefixes and WSL drive mounts, with the pinned revision listed first; and type dumps named for a found revision beside an install, extracted by typeextract into the Ambrose data folder's types folder, in the Ambrose data folder, the working folder or the executable's folder, or named by AMBROSE_TYPE_DUMP_PATH.
 */

#ifndef AMBROSE_CLIENTLOCATOR_H
#define AMBROSE_CLIENTLOCATOR_H

#include "ClientSystem.h"

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

    bool IsPinned() const noexcept;
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
    static constexpr std::size_t MaxFoldersVisited = 512;
    static constexpr std::size_t MaxChildren = 64;
    static constexpr std::size_t MaxVdfBytes = 1024 * 1024;
    static constexpr std::size_t DumpHeaderBytes = 256;

    ClientLocator() = delete;

    static std::vector<ClientCandidate> FindInstalls(ClientSystem const& system);
    static std::vector<TypeDumpCandidate> FindTypeDumps(ClientSystem const& system, std::vector<ClientCandidate> const& installs);
    static std::vector<std::filesystem::path> ParseSteamLibraries(std::string_view vdf);
    static bool LooksLikeTypeDump(ClientSystem const& system, std::filesystem::path const& path);
    static std::filesystem::path GetDataFolder(ClientSystem const& system);
    static std::string PathText(std::filesystem::path const& path);
};

#endif
