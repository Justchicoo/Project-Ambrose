/*
 * Project Ambrose by Imjustchico
 * One loaded revision of the patch manifest: the revision and its V_ folder, the LatestFileList.bin it was read from with that file's size, client CRC and modification time, and how many packages and files it lists, built only from a file that reads and validates whole.
 */

#ifndef AMBROSE_PATCHMANIFEST_H
#define AMBROSE_PATCHMANIFEST_H

#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct PatchManifest
{
    static constexpr std::string_view ListFileName = "LatestFileList.bin";
    static constexpr std::string_view DirectoryPrefix = "V_";

    std::string Revision;
    std::filesystem::path ListFile;
    uint32 ListFileSize = 0;
    uint32 ListFileCrc = 0;
    uint32 ListFileTime = 0;
    std::size_t Packages = 0;
    std::size_t Files = 0;

    std::string GetDirectoryName() const;

    static std::optional<PatchManifest> Load(std::filesystem::path const& outputDir, std::string const& revision, std::vector<std::string>& errors);
};

#endif
