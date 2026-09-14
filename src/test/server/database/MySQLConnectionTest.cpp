/*
 * Project Ambrose by Imjustchico
 * Tests connection strings offline, and with AMBROSE_TEST_DB set runs queries, typed reads, escaping, extra results, TLS, wrong passwords, and reconnect and retry rules after the server kills the connection.
 */

#include "Environment.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "MySQLConnection.h"
#include "QueryResult.h"
#include "ScopeExit.h"
#include "TestAppender.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
    std::optional<MySQLConnectionInfo> TestDatabase()
    {
        std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
        if (!text || text->empty())
            return std::nullopt;
        return MySQLConnectionInfo::Parse(*text);
    }

    MySQLConnectionSettings FastReconnect()
    {
        MySQLConnectionSettings settings;
        settings.FirstReconnectDelay = std::chrono::milliseconds(20);
        settings.MaxReconnectDelay = std::chrono::milliseconds(200);
        settings.GiveUpReconnectAfter = std::chrono::milliseconds(5000);
        return settings;
    }
}

TEST(MySQLConnectionInfoTest, ParsesEveryForm)
{
    std::optional<MySQLConnectionInfo> const plain = MySQLConnectionInfo::Parse("127.0.0.1;3306;acore;secret;ambrose_login");
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->Host, "127.0.0.1");
    EXPECT_EQ(plain->Port, 3306);
    EXPECT_EQ(plain->User, "acore");
    EXPECT_EQ(plain->Password, "secret");
    EXPECT_EQ(plain->Database, "ambrose_login");
    EXPECT_EQ(plain->Tls, DatabaseTls::Off);
    EXPECT_EQ(plain->ToLogString(), "acore@127.0.0.1:3306/ambrose_login");

    std::optional<MySQLConnectionInfo> const socket = MySQLConnectionInfo::Parse(".;/run/mysqld/mysqld.sock;root;;world;tls-verify");
    ASSERT_TRUE(socket);
    EXPECT_EQ(socket->Socket, "/run/mysqld/mysqld.sock");
    EXPECT_EQ(socket->Password, "");
    EXPECT_EQ(socket->Tls, DatabaseTls::RequiredVerified);
    EXPECT_EQ(MySQLConnectionInfo::Parse("h;1;u;p;d;tls")->Tls, DatabaseTls::Required);

    std::string error;
    EXPECT_FALSE(MySQLConnectionInfo::Parse("h;1;u;p", &error));
    EXPECT_NE(error.find("found 4 field(s)"), std::string::npos) << error;
    EXPECT_FALSE(MySQLConnectionInfo::Parse("h;99999;u;p;d", &error));
    EXPECT_NE(error.find("99999"), std::string::npos) << error;
    EXPECT_FALSE(MySQLConnectionInfo::Parse("h;1;u;p;d;maybe", &error));
    EXPECT_FALSE(MySQLConnectionInfo::Parse(";1;u;p;d", &error));
    EXPECT_FALSE(MySQLConnectionInfo::Parse("h;1;u;p;d;off;ca.pem", &error));
    EXPECT_EQ(MySQLConnectionInfo::Parse("h;1;u;p;d;tls-verify;ca.pem")->TlsCa, "ca.pem");
}

TEST(MySQLConnectionTest, SelectOneReturnsOneRowWithOneField)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    MySQLConnection connection(*info, FastReconnect());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    QueryResult const result = connection.Query("SELECT 1");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->GetRowCount(), 1u);
    EXPECT_EQ(result->GetFieldCount(), 1u);
    EXPECT_EQ((*result)[0].Get<int32>(), 1);
    EXPECT_FALSE(result->NextRow());
    EXPECT_GT(connection.GetServerVersion(), 0u);
}

TEST(MySQLConnectionTest, TypedColumnsRowsAndEscaping)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    MySQLConnection connection(*info, FastReconnect());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.Execute("CREATE TEMPORARY TABLE field_types (id TINYINT UNSIGNED, big BIGINT UNSIGNED, name VARCHAR(32), data BLOB, amount DOUBLE, missing INT)"));
    std::string const name = connection.Escape("O'Brien \\ \"quoted\"");
    ASSERT_TRUE(connection.Execute(fmt::format("INSERT INTO field_types VALUES (255, 18446744073709551615, '{}', X'00FF10', 2.5, NULL), (7, 1, 'second', '', 0, 3)", name)));

    QueryResult const result = connection.Query("SELECT id, big, name, data, amount, missing FROM field_types ORDER BY id DESC");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->GetRowCount(), 2u);
    Field const* row = result->Fetch();
    EXPECT_EQ(row[0].Get<uint8>(), 255);
    EXPECT_EQ(row[0].GetMetadata()->TypeName, "TINYINT UNSIGNED");
    EXPECT_EQ(row[1].Get<uint64>(), 18446744073709551615ull);
    EXPECT_EQ(row[2].Get<std::string>(), "O'Brien \\ \"quoted\"");
    EXPECT_EQ(row[3].Get<std::vector<uint8>>(), (std::vector<uint8>{ 0x00, 0xFF, 0x10 }));
    EXPECT_DOUBLE_EQ(row[4].Get<double>(), 2.5);
    EXPECT_TRUE(row[5].IsNull());
    EXPECT_EQ(result->FindField("name"), &row[2]);

    ASSERT_TRUE(result->NextRow());
    row = result->Fetch();
    EXPECT_EQ(row[0].Get<uint8>(), 7);
    EXPECT_EQ(row[5].Get<int32>(), 3);
    EXPECT_FALSE(result->NextRow());

    EXPECT_FALSE(connection.Query("SELECT id FROM field_types WHERE id = 99"));
    EXPECT_EQ(connection.GetLastErrorCode(), 0u);
    EXPECT_FALSE(connection.Query("SELECT nonsense FROM"));
    EXPECT_NE(connection.GetLastErrorCode(), 0u);
}

TEST(MySQLConnectionTest, KilledConnectionReconnectsOnTheNextQuery)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    MySQLConnection connection(*info, FastReconnect());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    uint64 const before = connection.GetThreadId();
    connection.Execute("KILL CONNECTION_ID()");
    QueryResult const afterSelfKill = connection.Query("SELECT CONNECTION_ID()");
    ASSERT_TRUE(afterSelfKill) << connection.GetLastErrorText();
    uint64 const second = (*afterSelfKill)[0].Get<uint64>();
    EXPECT_NE(second, before);
    EXPECT_GE(connection.GetReconnectCount(), 1u);

    MySQLConnection killer(*info, FastReconnect());
    ASSERT_EQ(killer.Open(), 0u) << killer.GetLastErrorText();
    ASSERT_TRUE(killer.Execute(fmt::format("KILL CONNECTION {}", second)));
    uint64 const reconnectsBefore = connection.GetReconnectCount();
    QueryResult const afterRemoteKill = connection.Query("SELECT CONNECTION_ID()");
    ASSERT_TRUE(afterRemoteKill) << connection.GetLastErrorText();
    EXPECT_NE((*afterRemoteKill)[0].Get<uint64>(), second);
    EXPECT_EQ(connection.GetReconnectCount(), reconnectsBefore + 1);
}

TEST(MySQLConnectionTest, LostConnectionInsideATransactionIsNotRetried)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    MySQLConnection connection(*info, FastReconnect());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.Execute("START TRANSACTION"));
    MySQLConnection killer(*info, FastReconnect());
    ASSERT_EQ(killer.Open(), 0u) << killer.GetLastErrorText();
    ASSERT_TRUE(killer.Execute(fmt::format("KILL CONNECTION {}", connection.GetThreadId())));

    EXPECT_FALSE(connection.Query("SELECT 1"));
    EXPECT_TRUE(MySQLConnection::IsConnectionLost(connection.GetLastErrorCode(), connection.IsMariaDB())) << connection.GetLastErrorCode();
    EXPECT_EQ(connection.GetReconnectCount(), 1u);
    QueryResult const after = connection.Query("SELECT 1");
    ASSERT_TRUE(after) << connection.GetLastErrorText();
    EXPECT_EQ(connection.GetLastErrorCode(), 0u);
}

TEST(MySQLConnectionTest, ExtraResultsAreDrainedAndClosedConnectionsStayClosed)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    MySQLConnection connection(*info, FastReconnect());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.Execute("DROP PROCEDURE IF EXISTS ambrose_two_results"));
    ASSERT_TRUE(connection.Execute("CREATE PROCEDURE ambrose_two_results() BEGIN SELECT 1; SELECT 2; END"));
    ScopeExit const dropProcedure([&connection] { connection.Execute("DROP PROCEDURE IF EXISTS ambrose_two_results"); });
    EXPECT_TRUE(connection.Execute("CALL ambrose_two_results()"));
    QueryResult const called = connection.Query("CALL ambrose_two_results()");
    ASSERT_TRUE(called);
    EXPECT_EQ(called->Fetch()[0].Get<int32>(), 1);
    QueryResult const next = connection.Query("SELECT 3");
    ASSERT_TRUE(next) << connection.GetLastErrorText();
    EXPECT_EQ(next->Fetch()[0].Get<int32>(), 3);

    EXPECT_FALSE(connection.Execute(""));
    EXPECT_NE(connection.GetLastErrorCode(), 0u);

    MySQLConnection closed(*info, FastReconnect());
    ASSERT_EQ(closed.Open(), 0u);
    closed.Close();
    EXPECT_FALSE(closed.Query("SELECT 1"));
    EXPECT_FALSE(closed.IsOpen());
    EXPECT_EQ(closed.GetReconnectCount(), 0u);
}

TEST(MySQLConnectionTest, RequiredTlsIsEncryptedOrRefused)
{
    std::optional<MySQLConnectionInfo> info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    info->Tls = DatabaseTls::Required;
    MySQLConnection connection(*info, FastReconnect());
    uint32 const code = connection.Open();
    if (code == 0)
        EXPECT_TRUE(connection.IsEncrypted());
    else
        EXPECT_FALSE(connection.IsOpen());
}

TEST(MySQLConnectionTest, WrongPasswordLogsTheErrorCodeAndFailsOpen)
{
    std::optional<MySQLConnectionInfo> info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    info->Password += "-wrong";

    auto const store = std::make_shared<TestAppenderStore>();
    ScopeExit const resetLog([] { sLog.Reset(); });
    ASSERT_TRUE(sLog.RegisterAppenderType(TestAppender::GetTypeInfo(store)).Succeeded());
    ASSERT_TRUE(sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n")).Succeeded());

    MySQLConnection connection(*info, FastReconnect());
    uint32 const code = connection.Open();
    EXPECT_EQ(code, 1045u);
    EXPECT_FALSE(connection.IsOpen());
    std::vector<LogMessage> const messages = store->Messages("Capture");
    ASSERT_FALSE(messages.empty());
    EXPECT_EQ(messages.back().Category, "sql.sql");
    EXPECT_EQ(messages.back().Level, LogLevel::Error);
    EXPECT_NE(messages.back().Text.find("[1045]"), std::string::npos) << messages.back().Text;
}
