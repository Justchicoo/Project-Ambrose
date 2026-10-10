/*
 * Project Ambrose by Imjustchico
 * Scans a user's Wizard101 installation into a deterministic LatestFileList manifest with cached file checksums.
 */

#ifndef AMBROSE_PATCHLISTGENERATOR_H
#define AMBROSE_PATCHLISTGENERATOR_H

#include "LatestFileList.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace PatchListGenerator
{
    struct Options
    {
        std::filesystem::path Client;
        std::filesystem::path Output;
        std::optional<std::filesystem::path> Rules;
        std::optional<std::filesystem::path> Reference;
    };

    struct Result
    {
        LatestFileList Manifest;
        std::string Revision;
        std::filesystem::path OutputDirectory;
        std::size_t CacheHits = 0;
        std::size_t FilesScanned = 0;
        std::vector<std::string> StillStreaming;
    };

    using OnDisk = std::function<bool(std::string const& package, LatestFileList::FileRecord const& record)>;

    std::optional<Result> Generate(Options const& options, std::string& error);
    std::string Diff(LatestFileList const& generated, LatestFileList const& reference, OnDisk const& onDisk = {});
    // The client's CRC of a file, or of length bytes from offset (to the end when no length is given).
    std::optional<uint32> FileCrc(std::filesystem::path const& path, uint64 offset, std::optional<uint64> length, std::string& error);
}

#endif
