/*
 * Project Ambrose by Imjustchico
 * Finds the revision folder the patch list generator wrote under Patch.OutputDir, the one Patch.Revision names or else the only one there, reads its LatestFileList.bin into memory once, takes its size, client CRC and modification time from those same bytes, and refuses it, naming every problem found, when it cannot be read, does not decode as a LatestFileList, lists no package, or holds a file record without a name.
 */

#include "PatchManifest.h"
#include "ConfigMgr.h"
#include "Crc32.h"
#include "LatestFileList.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <system_error>

namespace
{
    std::string Text(std::filesystem::path const& path)
    {
        return ConfigMgr::PathToUtf8(path);
    }

    std::optional<std::string> FindRevision(std::filesystem::path const& outputDir, std::vector<std::string>& errors)
    {
        std::vector<std::string> found;
        std::error_code error;
        for (std::filesystem::directory_iterator entry(outputDir, error), end; !error && entry != end; entry.increment(error))
        {
            std::string const name = ConfigMgr::PathToUtf8(entry->path().filename());
            std::error_code statError;
            if (name.size() > PatchManifest::DirectoryPrefix.size() && name.starts_with(PatchManifest::DirectoryPrefix) && entry->is_directory(statError)
                && std::filesystem::is_regular_file(entry->path() / PatchManifest::ListFileName, statError))
                found.push_back(name.substr(PatchManifest::DirectoryPrefix.size()));
        }
        if (error)
        {
            errors.push_back(fmt::format("cannot list {}: {}", Text(outputDir), error.message()));
            return std::nullopt;
        }
        std::sort(found.begin(), found.end());
        if (found.empty())
        {
            errors.push_back(fmt::format("{} holds no V_<revision> folder with a {}; run patchlist_generator --out {} first", Text(outputDir), PatchManifest::ListFileName, Text(outputDir)));
            return std::nullopt;
        }
        if (found.size() > 1)
        {
            errors.push_back(fmt::format("{} holds {} revisions ({}); set Patch.Revision to the one to serve", Text(outputDir), found.size(), fmt::join(found, ", ")));
            return std::nullopt;
        }
        return found.front();
    }

    std::optional<uint32> ModifiedTime(std::filesystem::path const& file, std::vector<std::string>& errors)
    {
        std::error_code error;
        std::filesystem::file_time_type const written = std::filesystem::last_write_time(file, error);
        if (error)
        {
            errors.push_back(fmt::format("cannot read when {} was written: {}", Text(file), error.message()));
            return std::nullopt;
        }
        int64 const seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::clock_cast<std::chrono::system_clock>(written).time_since_epoch()).count();
        if (seconds < 0 || seconds > std::numeric_limits<uint32>::max())
        {
            errors.push_back(fmt::format("{} was written at {} seconds from 1970, which MSG_LATEST_FILE_LIST_V2's ListFileTime cannot carry", Text(file), seconds));
            return std::nullopt;
        }
        return static_cast<uint32>(seconds);
    }
}

std::string PatchManifest::GetDirectoryName() const
{
    return std::string(DirectoryPrefix) + Revision;
}

std::optional<PatchManifest> PatchManifest::Load(std::filesystem::path const& outputDir, std::string const& revision, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    if (outputDir.empty())
    {
        errors.push_back("Patch.OutputDir is empty, so there is no manifest to serve; point it at the folder patchlist_generator --out wrote");
        return std::nullopt;
    }
    std::error_code error;
    if (!std::filesystem::is_directory(outputDir, error))
    {
        errors.push_back(fmt::format("the patch output folder {} does not exist or is not a folder", Text(outputDir)));
        return std::nullopt;
    }

    PatchManifest manifest;
    if (!revision.empty())
        manifest.Revision = revision;
    else if (std::optional<std::string> const only = FindRevision(outputDir, errors))
        manifest.Revision = *only;
    else
        return std::nullopt;
    manifest.ListFile = outputDir / manifest.GetDirectoryName() / ListFileName;

    std::ifstream stream(manifest.ListFile, std::ios::binary);
    if (!stream)
    {
        errors.push_back(fmt::format("cannot open {}", Text(manifest.ListFile)));
        return std::nullopt;
    }
    std::vector<uint8> const bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
    if (stream.bad())
        errors.push_back(fmt::format("cannot read {}", Text(manifest.ListFile)));
    if (bytes.empty())
        errors.push_back(fmt::format("{} is empty", Text(manifest.ListFile)));
    if (bytes.size() > std::numeric_limits<uint32>::max())
        errors.push_back(fmt::format("{} holds {} bytes, more than ListFileSize can carry", Text(manifest.ListFile), bytes.size()));
    if (errors.size() != before)
        return std::nullopt;

    try
    {
        LatestFileList const list = LatestFileList::ReadBinary(bytes);
        if (list.Packages.empty())
            errors.push_back(fmt::format("{} lists no package", Text(manifest.ListFile)));
        for (LatestFileList::Package const& package : list.Packages)
        {
            std::size_t const unnamed = static_cast<std::size_t>(std::count_if(package.Records.begin(), package.Records.end(), [](LatestFileList::FileRecord const& record) { return record.SrcFileName.empty(); }));
            if (unnamed != 0)
                errors.push_back(fmt::format("{}: package {} holds {} file record(s) with no SrcFileName", Text(manifest.ListFile), package.Name, unnamed));
            manifest.Files += package.Records.size();
        }
        manifest.Packages = list.Packages.size();
    }
    catch (std::exception const& failure)
    {
        errors.push_back(fmt::format("{} is not a LatestFileList: {}", Text(manifest.ListFile), failure.what()));
    }

    std::optional<uint32> const written = ModifiedTime(manifest.ListFile, errors);
    if (errors.size() != before || !written)
        return std::nullopt;
    manifest.ListFileSize = static_cast<uint32>(bytes.size());
    manifest.ListFileCrc = Crc32::ComputeClient(bytes);
    manifest.ListFileTime = *written;
    return manifest;
}
