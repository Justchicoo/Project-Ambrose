/*
 * Project Ambrose by Imjustchico
 * Where a program's web views keep their data: a folder of the program's own under the Ambrose data folder, never beside the executable, holding one profile for the program's own pages and one for each remote origin, so no two panels see each other's cookies or storage, and a profile deleted with everything in it once the web view has let go of its files.
 */

#ifndef AMBROSE_SHELLPROFILES_H
#define AMBROSE_SHELLPROFILES_H

#include "ShellRules.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

class ShellProfiles
{
public:
    static constexpr char const* Folder = "webview";
    static constexpr char const* OwnProfile = "own";
    static constexpr std::chrono::seconds DeleteWait{ 15 };

    ShellProfiles() = delete;

    static std::string NameFor(ShellOrigin const& origin);
    static std::filesystem::path FolderFor(std::filesystem::path const& dataFolder, ShellOrigin const& origin);
    static std::filesystem::path OwnFolder(std::filesystem::path const& dataFolder);
    static std::vector<std::string> List(std::filesystem::path const& dataFolder);
    static bool Delete(std::filesystem::path const& dataFolder, ShellOrigin const& origin, std::string& error);
};

#endif
