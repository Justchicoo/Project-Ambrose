/*
 * Project Ambrose by Imjustchico
 * Names each profile folder from its origin in characters every file system takes, and deletes one by removing the folder, trying again for a while because a web view's own processes let go of their files a moment after its window closes.
 */

#include "ShellProfiles.h"

#include "ConfigMgr.h"

#include <fmt/format.h>

#include <system_error>
#include <thread>

std::string ShellProfiles::NameFor(ShellOrigin const& origin)
{
    std::string name = fmt::format("{}-{}-{}", origin.Scheme, origin.Host, origin.Port);
    for (char& c : name)
    {
        bool const plain = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.';
        if (!plain)
            c = '_';
    }
    return name;
}

std::filesystem::path ShellProfiles::FolderFor(std::filesystem::path const& dataFolder, ShellOrigin const& origin)
{
    return dataFolder / Folder / NameFor(origin);
}

std::filesystem::path ShellProfiles::OwnFolder(std::filesystem::path const& dataFolder)
{
    return dataFolder / Folder / OwnProfile;
}

std::vector<std::string> ShellProfiles::List(std::filesystem::path const& dataFolder)
{
    std::vector<std::string> names;
    std::error_code code;
    for (std::filesystem::directory_iterator it(dataFolder / Folder, code), end; !code && it != end; it.increment(code))
        if (it->is_directory(code))
            names.push_back(ConfigMgr::PathToUtf8(it->path().filename()));
    return names;
}

bool ShellProfiles::Delete(std::filesystem::path const& dataFolder, ShellOrigin const& origin, std::string& error)
{
    std::filesystem::path const folder = FolderFor(dataFolder, origin);
    auto const until = std::chrono::steady_clock::now() + DeleteWait;
    std::error_code code;
    while (true)
    {
        code.clear();
        std::filesystem::remove_all(folder, code);
        if (!code && !std::filesystem::exists(folder, code))
            return true;
        if (std::chrono::steady_clock::now() >= until)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    error = fmt::format("the profile for {} at {} could not be deleted: {}", origin.Describe(), ConfigMgr::PathToUtf8(folder), code.message());
    return false;
}
