/*
 * Project Ambrose by Imjustchico
 * Tests parameter storage offline, and with AMBROSE_TEST_DB set runs typed prepared statements, binary results, byte-exact strings and blobs, unbound parameters, failed prepares, re-preparing after a reconnect, and empty results that leave the connection reusable.
 */

#include "CharacterDatabase.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "LoginDatabase.h"
#include "MySQLConnection.h"
#include "PreparedStatement.h"
#include "QueryResult.h"
#include "ScopeExit.h"
#include "TestAppender.h"
#include "WorldDatabase.h"

#include <gtest/gtest.h>

#include <random>
#include <string>

namespace
{
    enum TestStatements : uint32
    {
        TEST_SEL_SUM,
        TEST_SEL_ECHO,
        TEST_SEL_VALUES,
        TEST_SEL_BROKEN,
        TEST_SEL_SUBTRACT,
        TEST_SEL_META,
        TEST_SEL_DUPLICATE,
        TEST_SEL_ASYNC_ONLY,
        TEST_SEL_MAYBE
    };

    class TestConnection : public MySQLConnection
    {
    public:
        using MySQLConnection::MySQLConnection;

        bool IncludeBroken = false;
        bool IncludeTypes = false;
        bool IncludeErrors = false;
        bool IncludeDuplicate = false;
        bool IncludeMaybe = false;

    protected:
        void DoPrepareStatements() override
        {
            PrepareStatement(TEST_SEL_SUM, "TEST_SEL_SUM", "SELECT ? + ?", ConnectionFlags::Both);
            PrepareStatement(TEST_SEL_ECHO, "TEST_SEL_ECHO", "SELECT ?, ?", ConnectionFlags::Sync);
            PrepareStatement(TEST_SEL_ASYNC_ONLY, "TEST_SEL_ASYNC_ONLY", "SELECT 1", ConnectionFlags::Async);
            if (IncludeTypes)
                PrepareStatement(TEST_SEL_VALUES, "TEST_SEL_VALUES", "SELECT a, b, c, d, e, f, g, h, i, j, k FROM prepared_types ORDER BY a DESC", ConnectionFlags::Sync);
            if (IncludeErrors)
            {
                PrepareStatement(TEST_SEL_SUBTRACT, "TEST_SEL_SUBTRACT", "SELECT a - ? FROM prepared_errors", ConnectionFlags::Sync);
                PrepareStatement(TEST_SEL_META, "TEST_SEL_META", "SELECT * FROM prepared_meta", ConnectionFlags::Sync);
            }
            if (IncludeMaybe)
                PrepareStatement(TEST_SEL_MAYBE, "TEST_SEL_MAYBE", "SELECT 5 FROM DUAL WHERE ? = 1", ConnectionFlags::Both);
            if (IncludeDuplicate)
                PrepareStatement(TEST_SEL_SUM, "TEST_SEL_DUPLICATE", "SELECT 2", ConnectionFlags::Both);
            if (IncludeBroken)
                PrepareStatement(TEST_SEL_BROKEN, "TEST_SEL_BROKEN", "SELEC nothing FROM", ConnectionFlags::Both);
        }
    };

    std::optional<MySQLConnectionInfo> TestDatabase()
    {
        std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
        if (!text || text->empty())
            return std::nullopt;
        return MySQLConnectionInfo::Parse(*text);
    }

    MySQLConnectionSettings SyncSettings()
    {
        MySQLConnectionSettings settings;
        settings.Flags = ConnectionFlags::Sync;
        settings.FirstReconnectDelay = std::chrono::milliseconds(20);
        settings.GiveUpReconnectAfter = std::chrono::milliseconds(5000);
        return settings;
    }

    struct CapturedLog
    {
        CapturedLog() : Store(std::make_shared<TestAppenderStore>())
        {
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(Store));
            sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n"));
        }

        ~CapturedLog()
        {
            sLog.Reset();
        }

        bool Contains(std::string_view text) const
        {
            for (LogMessage const& message : Store->Messages("Capture"))
                if (message.Text.find(text) != std::string::npos)
                    return true;
            return false;
        }

        std::shared_ptr<TestAppenderStore> Store;
    };
}

TEST(PreparedStatementTest, StoresValuesByIndexAndRejectsOutOfRange)
{
    PreparedStatementBase statement(7, 3);
    statement.SetData(0, uint32{ 3 });
    statement.SetData(1, "text");
    statement.SetData(2, nullptr);
    EXPECT_EQ(statement.GetIndex(), 7u);
    ASSERT_EQ(statement.GetParameterCount(), 3u);
    EXPECT_TRUE(std::holds_alternative<uint32>(statement.GetValues()[0]));
    EXPECT_EQ(std::get<std::string>(statement.GetValues()[1]), "text");
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(statement.GetValues()[2]));
    EXPECT_EQ(statement.DescribeValues(), "3, <4-byte string>, NULL");

    CapturedLog log;
    statement.SetData(3, int8{ 1 });
    EXPECT_TRUE(log.Contains("parameter 4 cannot be set"));
    statement.Clear();
    EXPECT_EQ(statement.DescribeValues(), "<unset>, <unset>, <unset>");
}

TEST(PreparedStatementTest, SumOfTwoParametersIsSeven)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestConnection connection(*info, SyncSettings());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.PrepareStatements());
    EXPECT_FALSE(connection.IsStatementPrepared(TEST_SEL_ASYNC_ONLY));
    EXPECT_EQ(connection.GetPreparedStatementCount(), 2u);

    std::unique_ptr<PreparedStatementBase> const statement = connection.GetPreparedStatement(TEST_SEL_SUM);
    ASSERT_TRUE(statement);
    EXPECT_EQ(statement->GetParameterCount(), 2u);
    statement->SetData(0, uint32{ 3 });
    statement->SetData(1, uint32{ 4 });
    PreparedQueryResult const result = connection.Query(*statement);
    ASSERT_TRUE(result) << connection.GetLastErrorText();
    EXPECT_EQ(result->GetRowCount(), 1u);
    EXPECT_EQ((*result)[0].Get<uint32>(), 7u);
}

TEST(PreparedStatementTest, StringsWithNulAndLargeBlobsRoundTripExactly)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestConnection connection(*info, SyncSettings());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.PrepareStatements());

    std::string const text("before\0after", 12);
    std::vector<uint8> blob(1 << 20);
    for (std::size_t i = 0; i < blob.size(); ++i)
        blob[i] = static_cast<uint8>((i * 131) ^ (i >> 7));
    std::unique_ptr<PreparedStatementBase> const statement = connection.GetPreparedStatement(TEST_SEL_ECHO);
    ASSERT_TRUE(statement);
    statement->SetData(0, text);
    statement->SetData(1, blob);
    PreparedQueryResult const result = connection.Query(*statement);
    ASSERT_TRUE(result) << connection.GetLastErrorText();
    EXPECT_EQ((*result)[0].Get<std::string>(), text);
    EXPECT_EQ((*result)[1].Get<std::vector<uint8>>(), blob);

    statement->SetData(0, std::string());
    statement->SetData(1, std::vector<uint8>());
    PreparedQueryResult const empty = connection.Query(*statement);
    ASSERT_TRUE(empty) << connection.GetLastErrorText();
    EXPECT_FALSE((*empty)[0].IsNull());
    EXPECT_FALSE((*empty)[1].IsNull());
    EXPECT_EQ((*empty)[0].Get<std::string>(), "");
    EXPECT_TRUE((*empty)[1].Get<std::vector<uint8>>().empty());

    statement->SetData(0, static_cast<char const*>(nullptr));
    PreparedQueryResult const null = connection.Query(*statement);
    ASSERT_TRUE(null) << connection.GetLastErrorText();
    EXPECT_TRUE((*null)[0].IsNull());
}

TEST(PreparedStatementTest, ServerErrorsAndSchemaChangesLeaveStatementsUsable)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestConnection connection(*info, SyncSettings());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.Execute("DROP TABLE IF EXISTS prepared_meta"));
    ScopeExit const dropTable([&connection] { connection.Execute("DROP TABLE IF EXISTS prepared_meta"); });
    ASSERT_TRUE(connection.Execute("CREATE TEMPORARY TABLE prepared_errors (a INT UNSIGNED)"));
    ASSERT_TRUE(connection.Execute("INSERT INTO prepared_errors VALUES (0)"));
    ASSERT_TRUE(connection.Execute("CREATE TABLE prepared_meta (a INT)"));
    ASSERT_TRUE(connection.Execute("INSERT INTO prepared_meta VALUES (5)"));
    connection.IncludeErrors = true;
    ASSERT_TRUE(connection.PrepareStatements());

    std::unique_ptr<PreparedStatementBase> const subtract = connection.GetPreparedStatement(TEST_SEL_SUBTRACT);
    ASSERT_TRUE(subtract);
    subtract->SetData(0, uint32{ 1 });
    EXPECT_FALSE(connection.Query(*subtract));
    EXPECT_NE(connection.GetLastErrorCode(), 0u);
    subtract->SetData(0, uint32{ 0 });
    auto const start = std::chrono::steady_clock::now();
    PreparedQueryResult const again = connection.Query(*subtract);
    ASSERT_TRUE(again) << connection.GetLastErrorText();
    EXPECT_EQ((*again)[0].Get<int64>(), 0);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));

    std::unique_ptr<PreparedStatementBase> const meta = connection.GetPreparedStatement(TEST_SEL_META);
    ASSERT_TRUE(meta);
    PreparedQueryResult const before = connection.Query(*meta);
    ASSERT_TRUE(before) << connection.GetLastErrorText();
    EXPECT_EQ(before->GetFieldCount(), 1u);
    ASSERT_TRUE(connection.Execute("ALTER TABLE prepared_meta ADD COLUMN b INT NOT NULL DEFAULT 7"));
    PreparedQueryResult const after = connection.Query(*meta);
    ASSERT_TRUE(after) << connection.GetLastErrorText();
    ASSERT_EQ(after->GetFieldCount(), 2u);
    EXPECT_EQ((*after)[1].Get<int32>(), 7);
    QueryResult const text = connection.Query("SELECT 1");
    ASSERT_TRUE(text) << connection.GetLastErrorText();
}

TEST(PreparedStatementTest, TypedBinaryColumnsReadBack)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestConnection connection(*info, SyncSettings());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.PrepareStatements());
    ASSERT_TRUE(connection.Execute("CREATE TEMPORARY TABLE prepared_types (a TINYINT UNSIGNED, b SMALLINT, c INT UNSIGNED, d BIGINT, e FLOAT, f DOUBLE, g DECIMAL(10,2), h DATETIME, i VARCHAR(40), j BLOB, k INT)"));
    ASSERT_TRUE(connection.Execute("INSERT INTO prepared_types VALUES (255, -32768, 4294967295, -9223372036854775807, 1.5, 2.25, 12.50, '2026-09-14 12:34:56', 'wizard', X'0001FF', NULL), (1, 2, 3, 4, 5, 6, 7, '2000-01-01 00:00:00', 'second', X'', 9)"));

    connection.IncludeTypes = true;
    ASSERT_TRUE(connection.PrepareStatements());

    std::unique_ptr<PreparedStatementBase> const select = connection.GetPreparedStatement(TEST_SEL_VALUES);
    ASSERT_TRUE(select);
    PreparedQueryResult const result = connection.Query(*select);
    ASSERT_TRUE(result) << connection.GetLastErrorText();
    ASSERT_EQ(result->GetRowCount(), 2u);
    Field const* row = result->Fetch();
    EXPECT_EQ(row[0].Get<uint8>(), 255);
    EXPECT_EQ(row[1].Get<int16>(), -32768);
    EXPECT_EQ(row[2].Get<uint32>(), 4294967295u);
    EXPECT_EQ(row[3].Get<int64>(), -9223372036854775807LL);
    EXPECT_FLOAT_EQ(row[4].Get<float>(), 1.5f);
    EXPECT_DOUBLE_EQ(row[5].Get<double>(), 2.25);
    EXPECT_EQ(row[6].Get<std::string>(), "12.50");
    EXPECT_EQ(row[7].Get<std::string>(), "2026-09-14 12:34:56");
    EXPECT_EQ(row[8].Get<std::string>(), "wizard");
    EXPECT_EQ(row[9].Get<std::vector<uint8>>(), (std::vector<uint8>{ 0x00, 0x01, 0xFF }));
    EXPECT_TRUE(row[10].IsNull());
    EXPECT_EQ(row[2].Get<std::string>(), "4294967295");
    EXPECT_EQ(result->FindField("i"), &row[8]);

    ASSERT_TRUE(result->NextRow());
    row = result->Fetch();
    EXPECT_EQ(row[0].Get<uint8>(), 1);
    EXPECT_TRUE(row[9].Get<std::vector<uint8>>().empty());
    EXPECT_FALSE(row[9].IsNull());
    EXPECT_EQ(row[10].Get<int32>(), 9);
    EXPECT_FALSE(result->NextRow());

    std::unique_ptr<PreparedStatementBase> const sum = connection.GetPreparedStatement(TEST_SEL_SUM);
    ASSERT_TRUE(sum);
    sum->SetData(0, int32{ -5 });
    sum->SetData(1, int64{ 2 });
    PreparedQueryResult const total = connection.Query(*sum);
    ASSERT_TRUE(total) << connection.GetLastErrorText();
    EXPECT_EQ((*total)[0].Get<int32>(), -3);
}

TEST(PreparedStatementTest, UnboundParameterAndFailedPrepareAreReported)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    CapturedLog log;
    TestConnection connection(*info, SyncSettings());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.PrepareStatements());
    std::unique_ptr<PreparedStatementBase> const statement = connection.GetPreparedStatement(TEST_SEL_SUM);
    ASSERT_TRUE(statement);
    statement->SetData(0, uint32{ 1 });
    EXPECT_FALSE(connection.Execute(*statement));
    EXPECT_TRUE(log.Contains("parameter 2 not bound"));

    TestConnection broken(*info, SyncSettings());
    broken.IncludeBroken = true;
    ASSERT_EQ(broken.Open(), 0u);
    EXPECT_FALSE(broken.PrepareStatements());
    EXPECT_TRUE(log.Contains("Could not prepare statement TEST_SEL_BROKEN"));
    EXPECT_FALSE(broken.IsStatementPrepared(TEST_SEL_BROKEN));
    EXPECT_TRUE(broken.IsStatementPrepared(TEST_SEL_SUM));

    TestConnection duplicate(*info, SyncSettings());
    duplicate.IncludeDuplicate = true;
    ASSERT_EQ(duplicate.Open(), 0u);
    EXPECT_FALSE(duplicate.PrepareStatements());
    EXPECT_TRUE(log.Contains("index 0 is already used by TEST_SEL_SUM"));
}

TEST(PreparedStatementTest, StatementsArePreparedAgainAfterAReconnect)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    TestConnection connection(*info, SyncSettings());
    ASSERT_EQ(connection.Open(), 0u) << connection.GetLastErrorText();
    ASSERT_TRUE(connection.PrepareStatements());
    MySQLConnection killer(*info, SyncSettings());
    ASSERT_EQ(killer.Open(), 0u);
    ASSERT_TRUE(killer.Execute(fmt::format("KILL CONNECTION {}", connection.GetThreadId())));

    std::unique_ptr<PreparedStatementBase> const statement = connection.GetPreparedStatement(TEST_SEL_SUM);
    ASSERT_TRUE(statement);
    statement->SetData(0, uint8{ 20 });
    statement->SetData(1, uint16{ 22 });
    PreparedQueryResult const result = connection.Query(*statement);
    ASSERT_TRUE(result) << connection.GetLastErrorText();
    EXPECT_EQ((*result)[0].Get<uint32>(), 42u);
    EXPECT_EQ(connection.GetReconnectCount(), 1u);
    EXPECT_EQ(connection.GetPreparedStatementCount(), 2u);

    connection.Close();
    EXPECT_EQ(connection.GetPreparedStatementCount(), 0u);
    ASSERT_EQ(connection.Open(), 0u);
    EXPECT_EQ(connection.GetPreparedStatementCount(), 2u);
    PreparedQueryResult const reopened = connection.Query(*statement);
    ASSERT_TRUE(reopened) << connection.GetLastErrorText();
    EXPECT_EQ((*reopened)[0].Get<uint32>(), 42u);
}

TEST(PreparedStatementTest, DatabaseConnectionsPrepareTheirStatements)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    MySQLConnectionInfo loginInfo = *info;
    loginInfo.Database = fmt::format("ambrose_statements_{:08x}", std::random_device()());
    ScopeExit const dropLogin([&loginInfo]
    {
        MySQLConnectionInfo server = loginInfo;
        server.Database.clear();
        MySQLConnection connection(server);
        if (connection.Open() == 0)
            connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(loginInfo.Database)));
    });
    ASSERT_TRUE(DBUpdater::Run(loginInfo, "login", UpdaterSettings{}));
    LoginDatabaseConnection login(loginInfo);
    CharacterDatabaseConnection characters(*info);
    WorldDatabaseConnection world(*info);
    for (MySQLConnection* connection : { static_cast<MySQLConnection*>(&login), static_cast<MySQLConnection*>(&characters), static_cast<MySQLConnection*>(&world) })
    {
        ASSERT_EQ(connection->Open(), 0u) << connection->GetLastErrorText();
        EXPECT_TRUE(connection->PrepareStatements());
    }
    std::unique_ptr<PreparedStatementBase> const time = login.GetPreparedStatement(LOGIN_SEL_SERVER_TIME);
    ASSERT_TRUE(time);
    PreparedQueryResult const result = login.Query(*time);
    ASSERT_TRUE(result) << login.GetLastErrorText();
    EXPECT_GT((*result)[0].Get<uint64>(), 1700000000u);
}

TEST(PreparedStatementTest, EmptyResultsLeaveTheConnectionReusable)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase();
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    MySQLConnectionSettings settings = SyncSettings();
    settings.ReadTimeout = std::chrono::seconds(5);
    TestConnection connection(*info, settings);
    connection.IncludeMaybe = true;
    ASSERT_EQ(connection.Open(), 0u);
    ASSERT_TRUE(connection.PrepareStatements());
    CapturedLog log;
    for (int round = 0; round < 3; ++round)
    {
        PreparedStatementBase empty(TEST_SEL_MAYBE, 1);
        empty.SetData(0, uint32{ 0 });
        EXPECT_FALSE(connection.Query(empty));
        EXPECT_EQ(connection.GetLastErrorCode(), 0u) << connection.GetLastErrorText();
        ASSERT_TRUE(connection.Query("SELECT 1")) << connection.GetLastErrorText();
        PreparedStatementBase one(TEST_SEL_MAYBE, 1);
        one.SetData(0, uint32{ 1 });
        PreparedQueryResult const row = connection.Query(one);
        ASSERT_TRUE(row) << round << ": " << connection.GetLastErrorText();
        EXPECT_EQ((*row)[0].Get<uint32>(), 5u);
        PreparedStatementBase again(TEST_SEL_MAYBE, 1);
        again.SetData(0, uint32{ 0 });
        EXPECT_FALSE(connection.Query(again));
        EXPECT_EQ(connection.GetLastErrorCode(), 0u) << connection.GetLastErrorText();
    }
    EXPECT_FALSE(log.Contains("Reconnecting"));
}
