/*
 * Project Ambrose by Imjustchico
 * With AMBROSE_TEST_DB set, installs a login schema, routes log lines to the DB appender and checks they land in logs with their category, level and realm while sql lines never do.
 */

#include "AppenderDB.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "DatabaseLogSink.h"
#include "Environment.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "QueryResult.h"
#include "ScopeExit.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <random>

TEST(AppenderDBTest, LinesReachTheLogsTableExceptSqlCategories)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(info);
    info->Database = fmt::format("ambrose_appender_{:08x}", std::random_device()());
    ScopeExit const drop([&info]
    {
        MySQLConnectionInfo server = *info;
        server.Database.clear();
        MySQLConnection connection(server);
        if (connection.Open() == 0)
            connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(info->Database)));
    });
    ASSERT_TRUE(DBUpdater::Run(*info, "login", UpdaterSettings{}));

    ASSERT_TRUE(LoginDatabase.SetConnectionInfo(info->ToConnectionString(), 1, 1));
    ASSERT_EQ(LoginDatabase.Open(), 0u);
    ScopeExit const closePool([] { LoginDatabase.Close(); });

    ScopeExit const resetLog([] { sLog.Reset(); });
    ASSERT_TRUE(sLog.Apply(LogTestConfig::Settings("Appender.DB = 4,2,0,7\nLogger.root = 2,DB\n")).Succeeded());
    ASSERT_EQ(sLog.GetPendingAppenderNames(), std::vector<std::string>{ "DB" });
    ASSERT_TRUE(AppenderDB::Enable(sLog, 3));
    EXPECT_TRUE(sLog.GetPendingAppenderNames().empty());

    LOG_INFO("server.gameserver", "Realm {} online", "Ambrose");
    LOG_DEBUG("network.session", "Session {} accepted", 42);
    LOG_ERROR("sql.sql", "never stored");
    LOG_INFO("server.gameserver", "{}", std::string("bad \xFF utf8"));
    AppenderDB::Disable(sLog);
    EXPECT_EQ(sDatabaseLogSink.GetDroppedCount(), 0u);
    sLog.Reset();
    LoginDatabase.Close();

    MySQLConnection reader(*info);
    ASSERT_EQ(reader.Open(), 0u);
    QueryResult const rows = reader.Query("SELECT `category`, `level`, `realm_id`, `message` FROM `logs` ORDER BY `id`");
    ASSERT_TRUE(rows);
    ASSERT_EQ(rows->GetRowCount(), 3u);
    EXPECT_EQ((*rows)[0].Get<std::string>(), "server.gameserver");
    EXPECT_EQ((*rows)[1].Get<uint8>(), static_cast<uint8>(LogLevel::Info));
    EXPECT_EQ((*rows)[2].Get<uint32>(), 7u);
    EXPECT_EQ((*rows)[3].Get<std::string>(), "Realm Ambrose online");
    ASSERT_TRUE(rows->NextRow());
    EXPECT_EQ((*rows)[0].Get<std::string>(), "network.session");
    EXPECT_EQ((*rows)[1].Get<uint8>(), static_cast<uint8>(LogLevel::Debug));
    ASSERT_TRUE(rows->NextRow());
    EXPECT_EQ((*rows)[3].Get<std::string>(), "bad \xEF\xBF\xBD utf8");
}

TEST(AppenderDBTest, SinkTextIsValidUtf8AndBounded)
{
    EXPECT_EQ(DatabaseLogSink::PrepareText("plain", 255), "plain");
    EXPECT_EQ(DatabaseLogSink::PrepareText("a\xC3\xA9" "b", 2), "a");
    EXPECT_EQ(DatabaseLogSink::PrepareText("x\xFF" "y", 255), "x\xEF\xBF\xBD" "y");
}
