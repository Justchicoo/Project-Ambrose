/*
 * Project Ambrose by Imjustchico
 * Takes the folder the build was made from when it still holds data/sql, then the installed share/ambrose beside the executable, and falls back to the build folder so an error names a path a person can look at.
 */

#include "SourceFolder.h"
#include "ConfigMgr.h"
#include "Environment.h"

#include <system_error>

std::filesystem::path Ambrose::FindSourceFolder()
{
    std::error_code error;
    std::filesystem::path const built = ConfigMgr::PathFromUtf8(AMBROSE_SOURCE_DIRECTORY);
    if (std::filesystem::is_directory(built / "data" / "sql", error))
        return built;
    std::filesystem::path const installed = GetExecutableDirectory().parent_path() / "share" / "ambrose";
    if (std::filesystem::is_directory(installed / "data" / "sql", error))
        return installed;
    return built;
}
