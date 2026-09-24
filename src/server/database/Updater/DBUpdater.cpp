/*
 * Project Ambrose by Imjustchico
 * Creates a missing schema with utf8mb4 when auto setup allows, imports base files into an empty schema with the update bookkeeping tables last, and runs every SQL file on its own multi-statement connection in batches under max_allowed_packet, inside one transaction when asked so a failure leaves nothing behind; a live listing or data-only apply first checks that the updater set the database up, and every failure is kept as text for the caller as well as logged.
 */

#include "DBUpdater.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "Log.h"
#include "QueryResult.h"
#include "SourceFolder.h"
#include "SqlScript.h"
#include "UpdateFetcher.h"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <vector>

namespace
{
    constexpr uint64 DefaultMaxPacket = 16 * 1024 * 1024;
    constexpr uint64 PacketMargin = 64 * 1024;

    void RegisterModuleIncludes(MySQLConnection& bookkeeping, std::filesystem::path const& source, std::string_view database)
    {
        std::filesystem::path const modules = source / "modules";
        std::error_code error;
        for (std::filesystem::directory_iterator iterator(modules, error); !error && iterator != std::filesystem::directory_iterator(); iterator.increment(error))
        {
            if (!iterator->is_directory(error))
                continue;
            std::filesystem::path const sql = iterator->path() / "data" / "sql" / fmt::format("db-{}", database);
            if (!std::filesystem::is_directory(sql, error))
                continue;
            std::string const path = fmt::format("$/modules/{}/data/sql/db-{}", ConfigMgr::PathToUtf8(iterator->path().filename()), database);
            if (!bookkeeping.Execute(fmt::format("INSERT IGNORE INTO `updates_include` (`path`, `state`) VALUES ('{}', 'MODULE')", bookkeeping.Escape(path))))
                LOG_ERROR("sql.updates", "Cannot register module SQL folder {}: [{}] {}", path, bookkeeping.GetLastErrorCode(), bookkeeping.GetLastErrorText());
        }
    }

    struct Batch
    {
        std::string Text;
        std::size_t FirstStatement = 1;
    };
}

std::filesystem::path DBUpdater::GetBuiltInSourceDirectory()
{
    return Ambrose::FindSourceFolder();
}

std::string DBUpdater::QuoteIdentifier(std::string_view identifier)
{
    std::string quoted = "`";
    for (char const c : identifier)
    {
        if (c == '`')
            quoted += "``";
        else
            quoted.push_back(c);
    }
    quoted.push_back('`');
    return quoted;
}

std::filesystem::path DBUpdater::SourceDirectoryFor(UpdaterSettings const& settings)
{
    return settings.SourceDirectory.empty() ? GetBuiltInSourceDirectory() : settings.SourceDirectory;
}

bool DBUpdater::ApplyScript(MySQLConnectionInfo const& info, MySQLConnectionSettings const& connectionSettings, std::string_view fileLabel, std::string_view contents, std::string* failure, bool inTransaction)
{
    auto const fail = [failure](std::string text)
    {
        LOG_ERROR("sql.updates", "{}", text);
        if (failure)
            *failure = std::move(text);
        return false;
    };
    std::string_view const body = SqlScript::StripByteOrderMark(contents);
    std::vector<SqlScript::Statement> statements;
    std::string error;
    if (!SqlScript::Split(body, statements, error))
        return fail(fmt::format("{} cannot be applied: {}", fileLabel, error));
    if (statements.empty())
        return true;

    MySQLConnectionSettings settings = connectionSettings;
    settings.MultiStatements = true;
    settings.Flags = ConnectionFlags::Sync;
    settings.ReadTimeout = std::max<std::chrono::seconds>(settings.ReadTimeout, ScriptReadTimeout);
    settings.WriteTimeout = std::max<std::chrono::seconds>(settings.WriteTimeout, ScriptWriteTimeout);
    MySQLConnection connection(info, settings);
    if (connection.Open() != 0)
        return fail(fmt::format("{} cannot be applied: no connection to {}", fileLabel, info.ToLogString()));

    uint64 maxPacket = DefaultMaxPacket;
    if (QueryResult const packet = connection.Query("SELECT @@max_allowed_packet"))
        maxPacket = std::max<uint64>((*packet)[0].Get<uint64>(), PacketMargin * 2);
    uint64 const limit = maxPacket - PacketMargin;

    std::vector<Batch> batches;
    if (body.size() <= limit)
        batches.push_back(Batch{ std::string(body), 1 });
    else
    {
        Batch current;
        for (std::size_t index = 0; index < statements.size(); ++index)
        {
            std::string_view const text = statements[index].Text;
            if (text.size() + 2 > limit)
                return fail(fmt::format("{} cannot be applied: statement {} on line {} is {} bytes, more than the server's max_allowed_packet of {}", fileLabel, index + 1, statements[index].Line, text.size(), maxPacket));
            if (!current.Text.empty() && current.Text.size() + text.size() + 2 > limit)
            {
                batches.push_back(std::move(current));
                current = Batch{};
            }
            if (current.Text.empty())
                current.FirstStatement = index + 1;
            current.Text.append(text).append(";\n");
        }
        if (!current.Text.empty())
            batches.push_back(std::move(current));
    }

    if (inTransaction && !connection.Execute("START TRANSACTION"))
        return fail(fmt::format("{} cannot be applied: no transaction could start: [{}] {}", fileLabel, connection.GetLastErrorCode(), connection.GetLastErrorText()));
    for (Batch const& batch : batches)
    {
        std::size_t failedInBatch = 0;
        if (connection.ExecuteScript(batch.Text, failedInBatch))
            continue;
        std::size_t const failed = failedInBatch ? batch.FirstStatement + failedInBatch - 1 : 0;
        std::string_view const undone = inTransaction ? "; nothing it changed was kept" : "";
        if (failed >= 1 && failed <= statements.size())
        {
            SqlScript::Statement const& statement = statements[failed - 1];
            return fail(fmt::format("{} failed at statement {} on line {}: [{}] {} in: {}{}", fileLabel, failed, statement.Line, connection.GetLastErrorCode(), connection.GetLastErrorText(), SqlScript::Excerpt(statement.Text), undone));
        }
        return fail(fmt::format("{} failed: [{}] {}{}", fileLabel, connection.GetLastErrorCode(), connection.GetLastErrorText(), undone));
    }
    if (inTransaction && !connection.Execute("COMMIT"))
        return fail(fmt::format("{} could not be committed: [{}] {}; nothing it changed was kept", fileLabel, connection.GetLastErrorCode(), connection.GetLastErrorText()));
    return true;
}

bool DBUpdater::OpenBookkeeping(MySQLConnection& connection, MySQLConnectionInfo const& info, std::string& error)
{
    if (connection.Open() != 0)
    {
        error = fmt::format("cannot connect to {}: [{}] {}", info.ToLogString(), connection.GetLastErrorCode(), connection.GetLastErrorText());
        return false;
    }
    QueryResult const tables = connection.Query(fmt::format(
        "SELECT CAST(COALESCE(SUM(table_name IN ('updates', 'updates_include')), 0) AS UNSIGNED) FROM information_schema.tables WHERE table_schema = '{}'",
        connection.Escape(info.Database)));
    if (connection.GetLastErrorCode() != 0)
    {
        error = fmt::format("cannot read the tables of {}: [{}] {}", info.Database, connection.GetLastErrorCode(), connection.GetLastErrorText());
        return false;
    }
    if (!tables || (*tables)[0].Get<uint64>() != 2)
    {
        error = fmt::format("{} has no updates and updates_include tables, so the updater has not set it up", info.Database);
        return false;
    }
    return true;
}

bool DBUpdater::Inspect(MySQLConnectionInfo const& info, UpdaterSettings const& settings, UpdateReport& report, std::string& error, MySQLConnectionSettings const& connectionSettings)
{
    report = UpdateReport{};
    MySQLConnection bookkeeping(info, connectionSettings);
    if (!OpenBookkeeping(bookkeeping, info, error))
        return false;
    UpdaterSettings effective = settings;
    effective.SourceDirectory = SourceDirectoryFor(settings);
    UpdateFetcher const fetcher(bookkeeping, std::move(effective), {});
    return fetcher.Inspect(report, error);
}

UpdateSummary DBUpdater::ApplyDataOnly(MySQLConnectionInfo const& info, std::string_view folderName, UpdaterSettings const& settings, MySQLConnectionSettings const& connectionSettings)
{
    UpdateSummary summary;
    MySQLConnection bookkeeping(info, connectionSettings);
    if (!OpenBookkeeping(bookkeeping, info, summary.Failure))
    {
        LOG_ERROR("sql.updates", "Cannot apply data-only updates to the {} database: {}", folderName, summary.Failure);
        summary.Succeeded = false;
        return summary;
    }
    UpdaterSettings effective = settings;
    effective.SourceDirectory = SourceDirectoryFor(settings);
    UpdateFetcher fetcher(bookkeeping, std::move(effective), [&info, &connectionSettings](UpdateFile const& file, std::string_view contents, std::string& failure)
    {
        return ApplyScript(info, connectionSettings, ConfigMgr::PathToUtf8(file.Path), contents, &failure, UpdateFetcher::Classify(contents).Transactional);
    });
    return fetcher.Update(folderName, [](UpdateFile const&, std::string_view contents) { return UpdateFetcher::IsDataOnly(contents); });
}

bool DBUpdater::Populate(MySQLConnection& bookkeeping, MySQLConnectionInfo const& info, MySQLConnectionSettings const& connectionSettings, std::filesystem::path const& baseDirectory)
{
    QueryResult const tables = bookkeeping.Query(fmt::format(
        "SELECT COUNT(*), CAST(COALESCE(SUM(table_name IN ('updates', 'updates_include')), 0) AS UNSIGNED) FROM information_schema.tables WHERE table_schema = '{}'",
        bookkeeping.Escape(info.Database)));
    if (!tables)
        return false;
    uint64 const total = (*tables)[0].Get<uint64>();
    uint64 const markers = (*tables)[1].Get<uint64>();
    if (markers == 2)
        return true;
    if (total > 0)
    {
        LOG_ERROR("sql.updates", "Database {} has {} table(s) but no updates and updates_include tables; it was not installed by Project Ambrose or its base import was interrupted, so empty it or import the base by hand", info.Database, total);
        return false;
    }

    std::vector<std::filesystem::path> files;
    std::string error;
    std::error_code directoryError;
    if (!std::filesystem::is_directory(baseDirectory, directoryError))
    {
        LOG_ERROR("sql.updates", "Database {} is empty and its base folder {} does not exist; set Updates.SourcePath to the Project Ambrose folder", info.Database, ConfigMgr::PathToUtf8(baseDirectory));
        return false;
    }
    if (!UpdateFetcher::ListSqlFiles(baseDirectory, files, error))
    {
        LOG_ERROR("sql.updates", "Cannot import the base of {}: {}", info.Database, error);
        return false;
    }
    std::stable_partition(files.begin(), files.end(), [](std::filesystem::path const& file)
    {
        std::string const name = ConfigMgr::PathToUtf8(file.filename());
        return name != "updates.sql" && name != "updates_include.sql";
    });
    if (files.empty())
    {
        LOG_ERROR("sql.updates", "Database {} is empty and {} has no base files", info.Database, ConfigMgr::PathToUtf8(baseDirectory));
        return false;
    }

    LOG_INFO("sql.updates", "Database {} is empty; importing {} base file(s)", info.Database, files.size());
    for (std::filesystem::path const& file : files)
    {
        std::string contents;
        if (!UpdateFetcher::ReadFile(file, contents, error))
        {
            LOG_ERROR("sql.updates", "Cannot import the base of {}: {}", info.Database, error);
            return false;
        }
        if (!ApplyScript(info, connectionSettings, ConfigMgr::PathToUtf8(file), contents))
            return false;
    }
    return true;
}

bool DBUpdater::Run(MySQLConnectionInfo const& info, std::string_view folderName, UpdaterSettings const& settings, MySQLConnectionSettings const& connectionSettings)
{
    if (info.Database.empty())
    {
        LOG_ERROR("sql.updates", "The {} connection string names no database to update", folderName);
        return false;
    }
    std::filesystem::path const source = SourceDirectoryFor(settings);

    MySQLConnectionInfo serverOnly = info;
    serverOnly.Database.clear();
    MySQLConnection server(serverOnly, connectionSettings);
    if (server.Open() != 0)
        return false;
    QueryResult const exists = server.Query(fmt::format("SELECT COUNT(*) FROM information_schema.schemata WHERE schema_name = '{}'", server.Escape(info.Database)));
    if (server.GetLastErrorCode() != 0)
        return false;
    bool created = false;
    if (!exists || (*exists)[0].Get<uint64>() == 0)
    {
        if (!settings.AutoSetup)
        {
            LOG_ERROR("sql.updates", "Database {} does not exist; create it or set Updates.AutoSetup = 1", info.Database);
            return false;
        }
        if (!server.Execute(fmt::format("CREATE DATABASE {} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci", QuoteIdentifier(info.Database))))
        {
            LOG_ERROR("sql.updates", "Cannot create database {}", info.Database);
            return false;
        }
        created = true;
        LOG_INFO("sql.updates", "Created database {}", info.Database);
    }

    MySQLConnection bookkeeping(info, connectionSettings);
    if (bookkeeping.Open() != 0)
        return false;
    std::string const folder = fmt::format("db_{}", folderName);
    if (!Populate(bookkeeping, info, connectionSettings, source / "data" / "sql" / "base" / folder))
    {
        if (created)
        {
            bookkeeping.Close();
            server.Execute(fmt::format("DROP DATABASE {}", QuoteIdentifier(info.Database)));
            LOG_ERROR("sql.updates", "Dropped the new database {} again because its base import failed", info.Database);
        }
        return false;
    }
    RegisterModuleIncludes(bookkeeping, source, folderName);
    UpdaterSettings effective = settings;
    effective.SourceDirectory = source;
    UpdateFetcher fetcher(bookkeeping, std::move(effective), [&info, &connectionSettings](UpdateFile const& file, std::string_view contents, std::string& failure)
    {
        return ApplyScript(info, connectionSettings, ConfigMgr::PathToUtf8(file.Path), contents, &failure);
    });
    return fetcher.Update(folderName).Succeeded;
}
