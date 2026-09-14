/*
 * Project Ambrose by Imjustchico
 * Tests SQL splitting, update names and LF-normalized hashes offline, and with AMBROSE_TEST_DB set runs the updater on fresh databases: base import, ordered and custom updates, bad names, failing files, and the repository's own login schema.
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
