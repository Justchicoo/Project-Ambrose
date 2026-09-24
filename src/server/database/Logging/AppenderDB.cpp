/*
 * Project Ambrose by Imjustchico
 * Parses Type,Level,Flags[,RealmId] for DB appenders, converts accepted lines into sink rows, and starts the sink before registering the type so replayed pending lines are kept, reporting registration errors instead of hiding them.
 */

#include "AppenderDB.h"
#include "DatabaseLogSink.h"
#include "Log.h"
#include "LogConfig.h"
#include "StringUtil.h"

#include <chrono>

AppenderDB::AppenderDB(AppenderDefinition const& definition, uint32 realmId)
    : Appender(definition.Name, definition.Type, definition.Level, definition.Flags), _realmId(realmId)
{
}

AppenderTypeInfo AppenderDB::GetTypeInfo()
{
    AppenderTypeInfo info;
    info.Type = AppenderType::DB;
    info.Name = "DB";
    info.ExcludedCategories = { "sql" };
    info.Create = [](AppenderDefinition const& definition, AppenderCreateContext const&, LogConfigResult& result) -> std::shared_ptr<Appender>
    {
        uint32 realmId = _defaultRealmId.load();
        if (definition.Fields.size() > 1)
        {
            result.Errors.push_back(definition.MakeIssue("DB appenders take Type,Level,Flags[,RealmId]"));
            return nullptr;
        }
        if (definition.Fields.size() == 1)
        {
            std::optional<uint32> const parsed = Ambrose::StringTo<uint32>(Ambrose::Trim(definition.Fields[0]));
            if (!parsed)
            {
                result.Errors.push_back(definition.MakeIssue("the DB appender realm id must be a number"));
                return nullptr;
            }
            realmId = *parsed;
        }
        return std::make_shared<AppenderDB>(definition, realmId);
    };
    return info;
}

bool AppenderDB::Enable(Log& log, uint32 defaultRealmId)
{
    _defaultRealmId = defaultRealmId;
    sDatabaseLogSink.Start();
    LogConfigResult const result = log.RegisterAppenderType(GetTypeInfo());
    for (ConfigIssue const& issue : result.Warnings)
        AMBROSE_LOG(log, LogLevel::Warn, "server.logging", "DB log appender: {}", issue.ToString());
    for (ConfigIssue const& issue : result.Errors)
        AMBROSE_LOG(log, LogLevel::Error, "server.logging", "DB log appender stays off: {}", issue.ToString());
    return result.Succeeded();
}

void AppenderDB::Disable(Log& log)
{
    log.UnregisterAppenderType(AppenderType::DB);
    sDatabaseLogSink.Stop(std::chrono::seconds(5));
}

void AppenderDB::Flush()
{
    sDatabaseLogSink.Flush();
}

void AppenderDB::WriteMessage(LogMessage const& message)
{
    DatabaseLogSink::Row row;
    row.LoggedAtMs = static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(message.Time.time_since_epoch()).count());
    row.RealmId = _realmId;
    row.Category = message.Category;
    row.Level = static_cast<uint8>(message.Level);
    row.Message = message.Text;
    sDatabaseLogSink.Push(std::move(row));
}
