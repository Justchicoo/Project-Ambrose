/*
 * Project Ambrose by Imjustchico
 * Writes the run folder on the real machine: creating folders, writing a file whole and reporting a failure at close as well as at open, removing a file that need not be there, and reading one archive entry through KiwadArchive.
 */

#include "LauncherFiles.h"

#include "ConfigMgr.h"
#include "KiwadArchive.h"

#include <fmt/format.h>

#include <fstream>
#include <memory>
#include <system_error>

bool LocalLauncherFiles::CreateFolder(std::filesystem::path const& folder, std::string& error) const
{
    std::error_code code;
    std::filesystem::create_directories(folder, code);
    if (std::filesystem::is_directory(folder, code))
        return true;
    error = fmt::format("{} could not be created: {}", ConfigMgr::PathToUtf8(folder), code ? code.message() : std::string("it is not a folder"));
    return false;
}

bool LocalLauncherFiles::WriteFile(std::filesystem::path const& file, std::string_view text, std::string& error) const
{
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        error = fmt::format("{} could not be opened for writing", ConfigMgr::PathToUtf8(file));
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (!stream)
    {
        error = fmt::format("{} could not be written", ConfigMgr::PathToUtf8(file));
        return false;
    }
    return true;
}

bool LocalLauncherFiles::RemoveFile(std::filesystem::path const& file, std::string& error) const
{
    std::error_code code;
    std::filesystem::remove(file, code);
    if (!code)
        return true;
    error = fmt::format("{} could not be removed: {}", ConfigMgr::PathToUtf8(file), code.message());
    return false;
}

std::optional<std::string> LocalLauncherFiles::ReadArchiveEntry(std::filesystem::path const& archive, std::string_view name, std::size_t maxBytes, std::string& error) const
{
    std::unique_ptr<KiwadArchive> const opened = KiwadArchive::Open(archive, error);
    if (!opened)
        return std::nullopt;
    KiwadReadResult result = opened->Read(name, maxBytes);
    if (!result.Succeeded())
    {
        error = result.Error;
        return std::nullopt;
    }
    return std::string(result.Data.begin(), result.Data.end());
}
