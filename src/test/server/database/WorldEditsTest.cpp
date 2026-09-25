/*
 * Project Ambrose by Imjustchico
 * Checks a live world edit end to end against real world databases when AMBROSE_TEST_DB is set: an edit the database takes changes the live world database and writes exactly one journal entry naming who made it and where it came from, one the database refuses and an empty one are never journaled, and the pending update file the journal exports applies cleanly to a fresh world database, which then holds the edit.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "MySQLConnection.h"
#include "WorldEditJournal.h"
#include "WorldEdits.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    std::string const Raise = "INSERT INTO `command_security` (`command`, `security_level`, `comment`) VALUES ('server info', 1, 'raised live for the journal test')";

    class WorldEditsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            uint32 const tag = std::random_device()();
            _live = *info;
            _live.Database = fmt::format("ambrose_edits_live_{:08x}", tag);
            _fresh = *info;
            _fresh.Database = fmt::format("ambrose_edits_fresh_{:08x}", tag);
            ASSERT_TRUE(DBUpdater::Run(_live, "world", UpdaterSettings{}));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_live.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            sWorldEditJournal.Clear();
        }

        void TearDown() override
        {
            sWorldEditJournal.Clear();
            if (_open)
                WorldDatabase.Close();
            for (MySQLConnectionInfo const* database : { &_live, &_fresh })
            {
                if (database->Database.empty())
                    continue;
                MySQLConnectionInfo server = *database;
                server.Database.clear();
                MySQLConnection connection(server);
                if (connection.Open() == 0)
                    connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(database->Database)));
            }
        }

        static std::optional<uint8> LevelOf(MySQLConnectionInfo const& database, std::string const& command)
        {
            MySQLConnection connection(database);
            if (connection.Open() != 0)
                return std::nullopt;
            QueryResult const rows = connection.Query(fmt::format("SELECT `security_level` FROM `command_security` WHERE `command` = '{}'", command));
            if (!rows)
                return std::nullopt;
            return rows->Fetch()[0].Get<uint8>();
        }

        LogTestDirectory _directory;
        MySQLConnectionInfo _live;
        MySQLConnectionInfo _fresh;
        bool _open = false;
    };
}

TEST_F(WorldEditsTest, AnEditTheDatabaseTakesIsJournaledOnceAndItsExportAppliesToAFreshWorldDatabase)
{
    std::string error;
    ASSERT_TRUE(WorldEdits::Apply("Merle", "the console", Raise, error)) << error;
    EXPECT_EQ(LevelOf(_live, "server info"), uint8{ 1 }) << "the live world database holds the edit at once";
    std::vector<WorldEdit> const entries = sWorldEditJournal.Entries();
    ASSERT_EQ(entries.size(), 1u) << "one edit writes one journal entry";
    EXPECT_EQ(entries.front().Who, "Merle");
    EXPECT_EQ(entries.front().Source, "the console");
    EXPECT_EQ(entries.front().Statement, Raise);

    std::optional<std::filesystem::path> const exported = sWorldEditJournal.Export(_directory.Path(), error);
    ASSERT_TRUE(exported) << error;
    std::ifstream stream(*exported, std::ios::binary);
    std::string const contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    ASSERT_FALSE(contents.empty());

    ASSERT_TRUE(DBUpdater::Run(_fresh, "world", UpdaterSettings{}));
    EXPECT_FALSE(LevelOf(_fresh, "server info")) << "a fresh world database has no such row until the export is applied";
    std::string failure;
    ASSERT_TRUE(DBUpdater::ApplyScript(_fresh, MySQLConnectionSettings{}, exported->filename().string(), contents, &failure)) << failure;
    EXPECT_EQ(LevelOf(_fresh, "server info"), uint8{ 1 }) << "the export replays the edit on another world database";
}

TEST_F(WorldEditsTest, AnEditTheDatabaseRefusesOrOneWithNothingInItIsNeverJournaled)
{
    std::string error;
    EXPECT_FALSE(WorldEdits::Apply("Merle", "the console", "INSERT INTO `no_such_table` VALUES (1)", error));
    EXPECT_NE(error.find("refused"), std::string::npos) << error;
    EXPECT_FALSE(WorldEdits::Apply("Merle", "the console", "   ", error));
    EXPECT_NE(error.find("needs a statement"), std::string::npos) << error;
    EXPECT_EQ(sWorldEditJournal.Count(), 0u) << "the journal holds only what the database took";
}
