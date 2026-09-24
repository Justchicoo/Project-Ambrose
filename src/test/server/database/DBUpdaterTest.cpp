/*
 * Project Ambrose by Imjustchico
 * Tests SQL splitting, data-only statement and file classification, update names and LF-normalized hashes offline, and with AMBROSE_TEST_DB set runs the updater on fresh databases: base import, ordered and custom updates, a folder an update adds applied in the same run, bad names, failing files, the repository's own login schema, and a live listing and data-only apply that stops before a schema change and rolls a failing file back.
 */

#include "DBUpdater.h"
#include "Environment.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "LogTestDirectory.h"
#include "MySQLConnection.h"
#include "QueryResult.h"
#include "ScopeExit.h"
#include "SqlScript.h"
#include "TestAppender.h"
#include "UpdateFetcher.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <fstream>
#include <random>

namespace
{
    std::optional<MySQLConnectionInfo> TestDatabase(std::string const& database)
    {
        std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
        if (!text || text->empty())
            return std::nullopt;
        std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
        if (info)
            info->Database = fmt::format("{}_{:08x}", database, std::random_device()());
        return info;
    }

    void DropDatabase(MySQLConnectionInfo info)
    {
        std::string const name = info.Database;
        info.Database.clear();
        MySQLConnection connection(info);
        if (connection.Open() == 0)
            connection.Execute(fmt::format("DROP DATABASE IF EXISTS `{}`", name));
    }

    void WriteFile(std::filesystem::path const& path, std::string const& contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
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

    class UpdaterSource
    {
    public:
        UpdaterSource()
        {
            WriteFile(Base() / "updates.sql", "CREATE TABLE `updates` (`name` VARCHAR(200) NOT NULL PRIMARY KEY, `hash` CHAR(64) NOT NULL DEFAULT '', `state` ENUM('RELEASED','CUSTOM','MODULE','ARCHIVED','PENDING') NOT NULL DEFAULT 'RELEASED', `timestamp` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, `speed` INT UNSIGNED NOT NULL DEFAULT 0);\n");
            WriteFile(Base() / "updates_include.sql", "CREATE TABLE `updates_include` (`path` VARCHAR(200) NOT NULL PRIMARY KEY, `state` ENUM('RELEASED','CUSTOM','MODULE','ARCHIVED','PENDING') NOT NULL DEFAULT 'RELEASED');\nINSERT INTO `updates_include` VALUES ('$/data/sql/updates/db_test', 'RELEASED'), ('$/data/sql/custom/db_test', 'CUSTOM');\n");
        }

        std::filesystem::path Root() const { return _directory.Path(); }
        std::filesystem::path Base() const { return _directory.Path() / "data" / "sql" / "base" / "db_test"; }
        std::filesystem::path Updates() const { return _directory.Path() / "data" / "sql" / "updates" / "db_test"; }
        std::filesystem::path Custom() const { return _directory.Path() / "data" / "sql" / "custom" / "db_test"; }

    private:
        LogTestDirectory _directory;
    };

    uint64 Count(MySQLConnectionInfo const& info, std::string const& sql)
    {
        MySQLConnection connection(info);
        if (connection.Open() != 0)
            return 0;
        QueryResult const result = connection.Query(sql);
        return result ? (*result)[0].Get<uint64>() : 0;
    }
}

TEST(SqlScriptTest, SplitsOnTopLevelSemicolonsOnly)
{
    std::vector<SqlScript::Statement> statements;
    std::string error;
    ASSERT_TRUE(SqlScript::Split("-- header; not a statement\nCREATE TABLE a (b TEXT);\n\nINSERT INTO a VALUES ('x;y'), (\"q\\\";\"), (`c;d`);\n/* block; */ # hash;\nSELECT 1", statements, error)) << error;
    ASSERT_EQ(statements.size(), 3u);
    EXPECT_EQ(statements[0].Text, "CREATE TABLE a (b TEXT)");
    EXPECT_EQ(statements[0].Line, 2u);
    EXPECT_EQ(statements[1].Line, 4u);
    EXPECT_EQ(statements[2].Text, "SELECT 1");
    EXPECT_EQ(statements[2].Line, 6u);

    ASSERT_TRUE(SqlScript::Split("-- only comments\n/* and blocks */\n", statements, error));
    EXPECT_TRUE(statements.empty());

    ASSERT_TRUE(SqlScript::Split("\xEF\xBB\xBF/*!40101 SET NAMES utf8mb4 */;\nSELECT 1;;\nSELECT 2", statements, error)) << error;
    ASSERT_EQ(statements.size(), 4u);
    EXPECT_EQ(statements[0].Text, "/*!40101 SET NAMES utf8mb4 */");
    EXPECT_EQ(statements[1].Text, "SELECT 1");
    EXPECT_EQ(statements[2].Text, "");
    EXPECT_EQ(statements[3].Text, "SELECT 2");
    EXPECT_EQ(SqlScript::StripByteOrderMark("\xEF\xBB\xBFSELECT 1"), "SELECT 1");

    ASSERT_TRUE(SqlScript::Split("CREATE PROCEDURE p()\nBEGIN\n  DECLARE a INT DEFAULT IF(1, 2, 3);\n  IF a > 1 THEN\n    DROP TABLE IF EXISTS t;\n  ELSE\n    SET a = CASE WHEN a = 2 THEN 1 ELSE 0 END;\n  END IF;\n  WHILE a > 0 DO\n    SET a = a - 1;\n  END WHILE;\nEND;\nCALL p();", statements, error)) << error;
    ASSERT_EQ(statements.size(), 2u);
    EXPECT_EQ(statements[1].Text, "CALL p()");
    EXPECT_EQ(statements[1].Line, 13u);
    EXPECT_FALSE(SqlScript::Split("DELIMITER //\nCREATE PROCEDURE p() BEGIN END//\n", statements, error));
    EXPECT_NE(error.find("DELIMITER"), std::string::npos);
    EXPECT_FALSE(SqlScript::Split("SELECT 'open", statements, error));
    EXPECT_EQ(SqlScript::Excerpt("SELECT\n   a,\n\tb FROM t"), "SELECT a, b FROM t");
}

TEST(UpdateFetcherTest, NamesStatesAndHashes)
{
    EXPECT_TRUE(UpdateFetcher::IsReleasedFileName("2026_01_01_00.sql"));
    EXPECT_FALSE(UpdateFetcher::IsReleasedFileName("2026-1-1.sql"));
    EXPECT_FALSE(UpdateFetcher::IsReleasedFileName("2026_01_01_0a.sql"));
    EXPECT_FALSE(UpdateFetcher::IsReleasedFileName("2026_01_01_00.txt"));
    EXPECT_EQ(UpdateFetcher::ParseState("CUSTOM"), std::optional<UpdateState>(UpdateState::Custom));
    EXPECT_FALSE(UpdateFetcher::ParseState("custom"));
    EXPECT_EQ(UpdateFetcher::HashContents("a\r\nb\r\n"), UpdateFetcher::HashContents("a\nb\n"));
    EXPECT_NE(UpdateFetcher::HashContents("a\rb"), UpdateFetcher::HashContents("a\nb"));
    EXPECT_EQ(UpdateFetcher::HashContents(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_TRUE(UpdateFetcher::IsPendingFileName("rev_1767225600_npc.sql"));
    EXPECT_FALSE(UpdateFetcher::IsPendingFileName("2026_01_01_00.sql"));
    EXPECT_FALSE(UpdateFetcher::IsPendingFileName("rev_now_npc.sql"));
}

TEST(SqlScriptTest, TellsDataOnlyStatementsFromOnesThatCanChangeTheSchema)
{
    for (std::string_view const data : { "INSERT INTO t VALUES (1)", "insert ignore into t values (1)", "REPLACE INTO t VALUES (1)", "UPDATE t SET a = 1", "DELETE FROM t",
             "-- note\nINSERT INTO t VALUES (1)", "# note\nDELETE FROM t", "/* note */ UPDATE t SET a = 2", "SELECT 1", "WITH x AS (SELECT 1) SELECT * FROM x",
             "START TRANSACTION", "BEGIN", "BEGIN WORK", "COMMIT", "ROLLBACK", "SAVEPOINT a", "RELEASE SAVEPOINT a", "SET @a = 1", "SET NAMES utf8mb4",
             "SET SESSION sql_mode = ''", "LOCK TABLES t WRITE", "UNLOCK TABLES", "" })
        EXPECT_TRUE(SqlScript::IsDataOnly(data)) << data;
    for (std::string_view const schema : { "CREATE TABLE t (a INT)", "ALTER TABLE t ADD b INT", "DROP TABLE t", "TRUNCATE t", "RENAME TABLE a TO b", "CALL p()",
             "BEGIN NOT ATOMIC SELECT 1", "START SLAVE", "SET GLOBAL max_connections = 10", "SET @@GLOBAL.max_connections = 10", "SET PERSIST max_connections = 10",
             "SET PASSWORD = 'x'", "SET STATEMENT max_statement_time = 1 FOR ALTER TABLE t ADD c INT", "SELECT * FROM t INTO OUTFILE '/tmp/x'",
             "/*!40101 SET NAMES utf8 */", "LOAD DATA INFILE 'x' INTO TABLE t", "GRANT ALL ON *.* TO a", "CREATE OR REPLACE VIEW v AS SELECT 1", "DO SLEEP(1)" })
        EXPECT_FALSE(SqlScript::IsDataOnly(schema)) << schema;
    EXPECT_EQ(SqlScript::LeadingKeyword("  -- c\n# d\n/* e */ insert into t values (1)"), "INSERT");
    EXPECT_EQ(SqlScript::LeadingKeyword("/*!40101 SET NAMES utf8 */"), "");
    EXPECT_EQ(SqlScript::LeadingKeyword(""), "");
}

TEST(UpdateFetcherTest, ClassifiesAFileByItsFirstStatementThatCanChangeTheSchema)
{
    UpdateClassification const data = UpdateFetcher::Classify("-- header\nINSERT INTO t VALUES (1);\nUPDATE t SET a = 2;\n");
    EXPECT_EQ(data.Kind, UpdateKind::Data);
    EXPECT_TRUE(data.Transactional);
    EXPECT_TRUE(UpdateFetcher::IsDataOnly("DELETE FROM t;"));

    UpdateClassification const locked = UpdateFetcher::Classify("LOCK TABLES t WRITE;\nINSERT INTO t VALUES (1);\nUNLOCK TABLES;\n");
    EXPECT_EQ(locked.Kind, UpdateKind::Data);
    EXPECT_FALSE(locked.Transactional);

    UpdateClassification const schema = UpdateFetcher::Classify("INSERT INTO t VALUES (1);\n\nCREATE TABLE u (a INT);\nINSERT INTO u VALUES (1);\n");
    EXPECT_EQ(schema.Kind, UpdateKind::Schema);
    EXPECT_FALSE(schema.Transactional);
    EXPECT_EQ(schema.Line, 3u);
    EXPECT_EQ(schema.Statement, "CREATE TABLE u (a INT)");
    EXPECT_TRUE(schema.Problem.empty());

    UpdateClassification const broken = UpdateFetcher::Classify("INSERT INTO t VALUES ('open);\n");
    EXPECT_EQ(broken.Kind, UpdateKind::Schema);
    EXPECT_FALSE(broken.Problem.empty());
    EXPECT_FALSE(UpdateFetcher::IsDataOnly("INSERT INTO t VALUES ('open);"));
}

TEST(DBUpdaterTest, FreshDatabaseImportsBaseAppliesUpdatesInOrderAndThenIsUpToDate)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase("ambrose_updater_order");
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    DropDatabase(*info);
    ScopeExit const drop([&info] { DropDatabase(*info); });
    UpdaterSource source;
    WriteFile(source.Updates() / "2026_01_02_00.sql", "INSERT INTO `sequence` (`step`) VALUES (2);\n");
    WriteFile(source.Updates() / "2026_01_01_00.sql", "CREATE TABLE `sequence` (`id` INT AUTO_INCREMENT PRIMARY KEY, `step` INT NOT NULL);\nINSERT INTO `sequence` (`step`) VALUES (1);\n");
    WriteFile(source.Custom() / "0 local tweaks.sql", "INSERT INTO `sequence` (`step`) VALUES (3);\n");

    CapturedLog log;
    UpdaterSettings settings;
    settings.SourceDirectory = source.Root();
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_TRUE(log.Contains("importing 2 base file(s)"));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates`"), 3u);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = '0 local tweaks.sql' AND `state` = 'CUSTOM'"), 1u);
    EXPECT_EQ(Count(*info, "SELECT CAST(GROUP_CONCAT(`step` ORDER BY `id` SEPARATOR '') AS UNSIGNED) FROM `sequence`"), 123u);

    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_TRUE(log.Contains("The test database is up to date"));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `sequence`"), 3u);

    WriteFile(source.Updates() / "2026_01_01_00.sql", "CREATE TABLE `sequence` (`id` INT AUTO_INCREMENT PRIMARY KEY, `step` INT NOT NULL);\r\nINSERT INTO `sequence` (`step`) VALUES (1);\r\n");
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_FALSE(log.Contains("changed after it was applied"));

    std::filesystem::rename(source.Updates() / "2026_01_02_00.sql", source.Updates() / "2026_01_03_00.sql");
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_TRUE(log.Contains("2026_01_03_00.sql was already applied to the test database as 2026_01_02_00.sql"));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `sequence`"), 3u);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = '2026_01_03_00.sql'"), 1u);
}

TEST(DBUpdaterTest, InterruptedBaseImportIsRefusedAndAFailedFreshImportIsDropped)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase("ambrose_updater_partial");
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    DropDatabase(*info);
    ScopeExit const drop([&info] { DropDatabase(*info); });
    UpdaterSource source;
    WriteFile(source.Base() / "01_broken.sql", "CREATE TABLE `first` (`id` INT);\nCREATE TABLEE `second` (`id` INT);\n");
    UpdaterSettings settings;
    settings.SourceDirectory = source.Root();
    {
        CapturedLog log;
        EXPECT_FALSE(DBUpdater::Run(*info, "test", settings));
        EXPECT_TRUE(log.Contains("because its base import failed"));
    }
    EXPECT_EQ(Count(*info, "SELECT 1"), 0u);

    MySQLConnectionInfo serverOnly = *info;
    serverOnly.Database.clear();
    MySQLConnection server(serverOnly);
    ASSERT_EQ(server.Open(), 0u);
    ASSERT_TRUE(server.Execute(fmt::format("CREATE DATABASE {}", DBUpdater::QuoteIdentifier(info->Database))));
    ASSERT_TRUE(server.Execute(fmt::format("CREATE TABLE {}.`stray` (`id` INT)", DBUpdater::QuoteIdentifier(info->Database))));
    CapturedLog log;
    EXPECT_FALSE(DBUpdater::Run(*info, "test", settings));
    EXPECT_TRUE(log.Contains("has 1 table(s) but no updates and updates_include tables"));
    EXPECT_EQ(DBUpdater::QuoteIdentifier("a`b"), "`a``b`");
}

TEST(DBUpdaterTest, FailingOrBadlyNamedUpdatesStopWithoutRecording)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase("ambrose_updater_failure");
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    DropDatabase(*info);
    ScopeExit const drop([&info] { DropDatabase(*info); });
    UpdaterSource source;
    WriteFile(source.Updates() / "2026_02_01_00.sql", "CREATE TABLE `ok` (`id` INT);\n");
    WriteFile(source.Updates() / "2026_02_02_00.sql", "INSERT INTO `ok` VALUES (1);\nINSRT INTO `ok` VALUES (2);\n");

    UpdaterSettings settings;
    settings.SourceDirectory = source.Root();
    {
        CapturedLog log;
        EXPECT_FALSE(DBUpdater::Run(*info, "test", settings));
        EXPECT_TRUE(log.Contains("2026_02_02_00.sql failed at statement 2 on line 2"));
    }
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = '2026_02_02_00.sql'"), 0u);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = '2026_02_01_00.sql'"), 1u);

    std::filesystem::remove(source.Updates() / "2026_02_02_00.sql");
    WriteFile(source.Updates() / "2026-1-1.sql", "SELECT 1;\n");
    CapturedLog log;
    EXPECT_FALSE(DBUpdater::Run(*info, "test", settings));
    EXPECT_TRUE(log.Contains("2026-1-1.sql"));
    EXPECT_TRUE(log.Contains("is not named YYYY_MM_DD_NN.sql"));
}

TEST(DBUpdaterTest, RepositoryLoginSchemaInstallsFromScratch)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase("ambrose_updater_login");
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    DropDatabase(*info);
    ScopeExit const drop([&info] { DropDatabase(*info); });
    CapturedLog log;
    ASSERT_TRUE(DBUpdater::Run(*info, "login", UpdaterSettings{}));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = '2026_01_01_00.sql' AND `state` = 'RELEASED'"), 1u);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates_include` WHERE `path` = '$/data/sql/updates/pending_db_login' AND `state` = 'PENDING'"), 1u);
    ASSERT_TRUE(DBUpdater::Run(*info, "login", UpdaterSettings{}));
    EXPECT_TRUE(log.Contains("The login database is up to date"));
}

TEST(DBUpdaterTest, AnUpdateThatAddsAFolderHasItsFilesAppliedInTheSameRun)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase("ambrose_updater_include");
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    DropDatabase(*info);
    ScopeExit const drop([&info] { DropDatabase(*info); });
    UpdaterSource source;
    WriteFile(source.Updates() / "2026_01_01_00.sql",
        "INSERT INTO `updates_include` (`path`, `state`) VALUES ('$/data/sql/updates/pending_db_test', 'PENDING');\n"
        "CREATE TABLE `sequence` (`id` INT AUTO_INCREMENT PRIMARY KEY, `step` INT NOT NULL);\n");
    WriteFile(source.Root() / "data" / "sql" / "updates" / "pending_db_test" / "rev_1767225600_sequence.sql", "INSERT INTO `sequence` (`step`) VALUES (7);\n");

    CapturedLog log;
    UpdaterSettings settings;
    settings.SourceDirectory = source.Root();
    settings.AllowPending = true;
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = 'rev_1767225600_sequence.sql' AND `state` = 'PENDING'"), 1u);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `sequence`"), 1u);
    EXPECT_TRUE(log.Contains("Applied 2 update(s) to the test database"));
    EXPECT_FALSE(log.Contains("The test database is up to date"));

    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_TRUE(log.Contains("The test database is up to date"));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `sequence`"), 1u);
}

TEST(DBUpdaterTest, ALiveDataOnlyApplyStopsBeforeASchemaChangeAndRollsAFailingFileBack)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase("ambrose_updater_live");
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    DropDatabase(*info);
    ScopeExit const drop([&info] { DropDatabase(*info); });
    UpdaterSource source;
    UpdaterSettings settings;
    settings.SourceDirectory = source.Root();

    UpdateReport report;
    std::string error;
    EXPECT_FALSE(DBUpdater::Inspect(*info, settings, report, error));
    EXPECT_FALSE(error.empty());

    WriteFile(source.Updates() / "2026_03_01_00.sql", "CREATE TABLE `rows` (`id` INT PRIMARY KEY, `note` VARCHAR(20) NOT NULL);\n");
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    WriteFile(source.Updates() / "2026_03_02_00.sql", "INSERT INTO `rows` VALUES (1, 'one');\nUPDATE `rows` SET `note` = 'first' WHERE `id` = 1;\n");
    WriteFile(source.Updates() / "2026_03_03_00.sql", "ALTER TABLE `rows` ADD `extra` INT NOT NULL DEFAULT 0;\n");
    WriteFile(source.Updates() / "2026_03_04_00.sql", "INSERT INTO `rows` VALUES (2, 'two', 2);\n");

    ASSERT_TRUE(DBUpdater::Inspect(*info, settings, report, error)) << error;
    ASSERT_EQ(report.Applied.size(), 1u);
    EXPECT_EQ(report.Applied[0].Name, "2026_03_01_00.sql");
    EXPECT_TRUE(report.Applied[0].Present);
    EXPECT_FALSE(report.Applied[0].Changed);
    EXPECT_GT(report.Applied[0].AppliedAt, 0);
    ASSERT_EQ(report.Pending.size(), 3u);
    EXPECT_EQ(report.Pending[0].Classification.Kind, UpdateKind::Data);
    EXPECT_TRUE(report.Pending[0].Classification.Transactional);
    EXPECT_EQ(report.Pending[1].Classification.Kind, UpdateKind::Schema);
    EXPECT_EQ(report.Pending[1].Classification.Line, 1u);
    EXPECT_EQ(report.Pending[2].Classification.Kind, UpdateKind::Data);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `rows`"), 0u);

    {
        CapturedLog log;
        UpdateSummary const applied = DBUpdater::ApplyDataOnly(*info, "test", settings);
        ASSERT_TRUE(applied.Succeeded) << applied.Failure;
        EXPECT_EQ(applied.AppliedNames, std::vector<std::string>{ "2026_03_02_00.sql" });
        EXPECT_EQ(applied.StoppedAt, "2026_03_03_00.sql");
        EXPECT_TRUE(log.Contains("2026_03_03_00.sql can change the schema"));
    }

    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `rows` WHERE `note` = 'first'"), 1u);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM information_schema.columns WHERE table_schema = DATABASE() AND table_name = 'rows' AND column_name = 'extra'"), 0u);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` IN ('2026_03_03_00.sql', '2026_03_04_00.sql')"), 0u);

    std::filesystem::remove(source.Updates() / "2026_03_03_00.sql");
    std::filesystem::remove(source.Updates() / "2026_03_04_00.sql");
    WriteFile(source.Updates() / "2026_03_05_00.sql", "INSERT INTO `rows` VALUES (5, 'five');\nINSERT INTO `rows` VALUES (1, 'duplicate');\n");
    CapturedLog log;
    UpdateSummary const failed = DBUpdater::ApplyDataOnly(*info, "test", settings);
    EXPECT_FALSE(failed.Succeeded);
    EXPECT_EQ(failed.FailedAt, "2026_03_05_00.sql");
    EXPECT_NE(failed.Failure.find("nothing it changed was kept"), std::string::npos) << failed.Failure;
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `rows` WHERE `id` = 5"), 0u);
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = '2026_03_05_00.sql'"), 0u);
}

TEST(DBUpdaterTest, EnforcesRehashRedundancyDeadReferenceAndPendingPolicies)
{
    std::optional<MySQLConnectionInfo> const info = TestDatabase("ambrose_updater_part_two");
    if (!info)
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    DropDatabase(*info);
    ScopeExit const drop([&info] { DropDatabase(*info); });
    UpdaterSource source;
    WriteFile(source.Base() / "policy.sql", "CREATE TABLE `policy` (`value` INT NOT NULL);\n");
    WriteFile(source.Updates() / "2026_03_01_00.sql", "INSERT INTO `policy` VALUES (1);\n");
    UpdaterSettings settings;
    settings.SourceDirectory = source.Root();
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));

    WriteFile(source.Updates() / "2026_03_01_00.sql", "INSERT INTO `policy` VALUES (2);\n");
    EXPECT_FALSE(DBUpdater::Run(*info, "test", settings));
    settings.Redundancy = true;
    EXPECT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `policy`"), 2u);

    for (int index = 0; index < 4; ++index)
        WriteFile(source.Updates() / fmt::format("2026_03_02_{:02}.sql", index), "INSERT INTO `policy` VALUES (3);\n");
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    for (int index = 0; index < 4; ++index)
        std::filesystem::remove(source.Updates() / fmt::format("2026_03_02_{:02}.sql", index));
    settings.CleanDeadRefMaxCount = 3;
    EXPECT_FALSE(DBUpdater::Run(*info, "test", settings));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` LIKE '2026_03_02_%'"), 4u);

    std::filesystem::path const pending = source.Root() / "data" / "sql" / "updates" / "pending_db_test";
    WriteFile(pending / "rev_1767225600_policy.sql", "INSERT INTO `policy` VALUES (4);\n");
    MySQLConnection bookkeeping(*info);
    ASSERT_EQ(bookkeeping.Open(), 0u);
    ASSERT_TRUE(bookkeeping.Execute("INSERT INTO `updates_include` (`path`, `state`) VALUES ('$/data/sql/updates/pending_db_test', 'PENDING')"));
    settings.CleanDeadRefMaxCount = -1;
    settings.AllowPending = false;
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = 'rev_1767225600_policy.sql'"), 0u);
    settings.AllowPending = true;
    ASSERT_TRUE(DBUpdater::Run(*info, "test", settings));
    EXPECT_EQ(Count(*info, "SELECT COUNT(*) FROM `updates` WHERE `name` = 'rev_1767225600_policy.sql' AND `state` = 'PENDING'"), 1u);
}
