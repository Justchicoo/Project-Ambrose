/*
 * Project Ambrose by Imjustchico
 * Brings one database current before its pool opens: creates it when it is missing and allowed, imports the base snapshot into an empty schema, then applies pending update files, each on a fresh connection.
 */

#ifndef AMBROSE_DBUPDATER_H
#define AMBROSE_DBUPDATER_H

#include "MySQLConnection.h"

#include <filesystem>
#include <string>
#include <string_view>

struct UpdaterSettings
{
    bool AutoSetup = true;
    std::filesystem::path SourceDirectory;
};

class DBUpdater
{
public:
    static constexpr std::chrono::seconds ScriptReadTimeout{ 3600 };
    static constexpr std::chrono::seconds ScriptWriteTimeout{ 600 };

    static std::filesystem::path GetBuiltInSourceDirectory();
    static bool Run(MySQLConnectionInfo const& info, std::string_view folderName, UpdaterSettings const& settings, MySQLConnectionSettings const& connectionSettings = {});
    static bool ApplyScript(MySQLConnectionInfo const& info, MySQLConnectionSettings const& connectionSettings, std::string_view fileLabel, std::string_view contents);
    static std::string QuoteIdentifier(std::string_view identifier);

private:
    static bool Populate(MySQLConnection& bookkeeping, MySQLConnectionInfo const& info, MySQLConnectionSettings const& connectionSettings, std::filesystem::path const& baseDirectory);
};

#endif
