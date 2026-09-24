/*
 * Project Ambrose by Imjustchico
 * Brings one database current before its pool opens: creates it when it is missing and allowed, imports the base snapshot into an empty schema, then applies pending update files, each on a fresh connection; on a running server it lists what is applied and pending without changing anything, and applies pending updates only while they are data-only, each inside one transaction when it can be.
 */

#ifndef AMBROSE_DBUPDATER_H
#define AMBROSE_DBUPDATER_H

#include "MySQLConnection.h"
#include "UpdateFetcher.h"

#include <filesystem>
#include <string>
#include <string_view>

class DBUpdater
{
public:
    static constexpr std::chrono::seconds ScriptReadTimeout{ 3600 };
    static constexpr std::chrono::seconds ScriptWriteTimeout{ 600 };

    static std::filesystem::path GetBuiltInSourceDirectory();
    static bool Run(MySQLConnectionInfo const& info, std::string_view folderName, UpdaterSettings const& settings, MySQLConnectionSettings const& connectionSettings = {});
    static bool Inspect(MySQLConnectionInfo const& info, UpdaterSettings const& settings, UpdateReport& report, std::string& error, MySQLConnectionSettings const& connectionSettings = {});
    static UpdateSummary ApplyDataOnly(MySQLConnectionInfo const& info, std::string_view folderName, UpdaterSettings const& settings, MySQLConnectionSettings const& connectionSettings = {});
    static bool ApplyScript(MySQLConnectionInfo const& info, MySQLConnectionSettings const& connectionSettings, std::string_view fileLabel, std::string_view contents, std::string* failure = nullptr, bool inTransaction = false);
    static std::string QuoteIdentifier(std::string_view identifier);

private:
    static std::filesystem::path SourceDirectoryFor(UpdaterSettings const& settings);
    static bool OpenBookkeeping(MySQLConnection& connection, MySQLConnectionInfo const& info, std::string& error);
    static bool Populate(MySQLConnection& bookkeeping, MySQLConnectionInfo const& info, MySQLConnectionSettings const& connectionSettings, std::filesystem::path const& baseDirectory);
};

#endif
