/*
 * Project Ambrose by Imjustchico
 * Lets a client test read the install through the classes it holds that the type dump does not describe: the class file schemaprobe builds once per revision in the Ambrose data folder for the install AMBROSE_CLIENT_DIR names, read when it is current for the dump AMBROSE_TYPE_DUMP_PATH names whichever authored classes it was built on, and otherwise an error saying how to build it.
 */

#ifndef AMBROSE_INSTALLEDCLASSES_H
#define AMBROSE_INSTALLEDCLASSES_H

#include "ClientLocator.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LogConfig.h"
#include "ServerClassCache.h"
#include "TypeDumpLoader.h"

#include <fmt/format.h>

#include <filesystem>
#include <optional>
#include <string>

namespace InstalledClasses
{
    inline bool Read(TypeDumpLoader::RawDump& classes, std::string& source, std::string& error)
    {
        std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
        std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
        if (!client || client->empty() || !dump || dump->empty())
        {
            error = "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            return false;
        }
        LocalClientSystem const system;
        std::optional<ClientInstall> const install = ClientInstall::Inspect(system, LogConfig::Utf8Path(*client));
        if (!install)
        {
            error = fmt::format("{} is not a client install", *client);
            return false;
        }
        std::optional<std::filesystem::path> const path = ServerClassCache::PathFor(ClientLocator::GetDataFolder(system), install->Revision);
        if (!path || !ServerClassCache::IsCurrent(*path, LogConfig::Utf8Path(*dump), ServerClassCache::ReadBuiltOn(*path).value_or(std::string())))
        {
            error = fmt::format("no current class file for {}; run the extractor's classes command, or start the game server once, to build it", install->Describe());
            return false;
        }
        source = ConfigMgr::PathToUtf8(*path);
        return ServerClassCache::Read(*path, classes, error);
    }
}

#endif
