/*
 * Project Ambrose by Imjustchico
 * Scans a user's Wizard101 installation into a deterministic LatestFileList manifest with cached file checksums.
 */

#ifndef AMBROSE_PATCHLISTGENERATOR_H
#define AMBROSE_PATCHLISTGENERATOR_H

#include "LatestFileList.h"

#include <filesystem>
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
    };

    std::optional<Result> Generate(Options const& options, std::string& error);
    std::string Diff(LatestFileList const& generated, LatestFileList const& reference);
}

#endif
