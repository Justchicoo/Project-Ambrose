/*
 * Project Ambrose by Imjustchico
 * A client install written by the project for runs with no Wizard101 install: Bin/revision.dat naming a fixture revision, a Data/GameData/Root.wad holding the project's own LOGIN, SYSTEM and EXTENDEDBASE message definitions, and a type dump named for that revision holding the character select classes and every class and property the built-in typed views bind, so a login server started on it serves authentication and the character list.
 */

#ifndef AMBROSE_FIXTUREINSTALL_H
#define AMBROSE_FIXTUREINSTALL_H

#include "Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace FixtureInstall
{
    inline constexpr std::string_view Revision = "r0.AmbroseFixture";

    std::vector<uint8> MessageArchive();
    std::optional<std::string> TypeDump(std::string& error);
    std::filesystem::path TypeDumpPath(std::filesystem::path const& root);
    bool Write(std::filesystem::path const& root, std::string& error);
}

#endif
