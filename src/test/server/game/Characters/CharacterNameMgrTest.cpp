/*
 * Project Ambrose by Imjustchico
 * Tests the character name manager: a closed world database is reported and keeps the empty tables, and with AMBROSE_TEST_DB set it installs the world schema and checks that empty tables load, rows the test writes load and format in the default and a named locale, tables without the default locale or without a complete human locale load with a warning, an edited row applies on reload, a reload that meets an empty first name, a gap in a table's positions together with other problems, a bad gender or a missing table keeps the previous tables and reports every reason, and a pool closed after use is reported as not open.
 */

#include "CharacterNameMgr.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr uint32 Male = 1;

    constexpr uint32 Pack(uint8 first, uint8 middle, uint8 last)
    {
        return NameIndices{ first, middle, last }.Pack();
    }

    bool Mentions(std::vector<std::string> const& errors, std::string_view text)
    {
        return std::any_of(errors.begin(), errors.end(), [text](std::string const& error) { return error.find(text) != std::string::npos; });
    }

    class CharacterNameMgrDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_world_names_{:08x}", std::random_device()());
            _info = *info;
            ASSERT_TRUE(DBUpdater::Run(_info, "world", UpdaterSettings{}));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
        }

        void TearDown() override
        {
            if (_open)
                WorldDatabase.Close();
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
        }

        static void Execute(std::string const& sql)
        {
            ASSERT_TRUE(WorldDatabase.DirectExecute(sql)) << sql;
        }

        static void InsertHumanTables(std::string const& locale, std::string const& blaze)
        {
            Execute(fmt::format("INSERT INTO `character_name_part` (`table_name`, `locale`, `idx`, `locale_key`, `text`) VALUES "
                "('FirstName_HumanMale', '{0}', 0, 'First_Boy_0', 'Aaron'), ('FirstName_HumanMale', '{0}', 1, 'First_Boy_1', '{1}'), "
                "('FirstName_HumanFemale', '{0}', 0, 'First_Girl_0', 'Abby'), "
                "('MiddleName_Human', '{0}', 0, '', ''), ('MiddleName_Human', '{0}', 1, 'Middle_0', 'Storm'), "
                "('LastName_Human', '{0}', 0, '', ''), ('LastName_Human', '{0}', 1, 'Last_0', 'Blade'), ('LastName_Human', '{0}', 2, 'Last_1', 'Rider')", locale, blaze));
        }

        MySQLConnectionInfo _info;
        bool _open = false;
    };
}

TEST(CharacterNameMgrTest, AClosedWorldDatabaseIsReportedAndKeepsTheTables)
{
    WorldDatabase.Close();
    CharacterNameMgr manager;
    EXPECT_EQ(manager.GetDefaultLocale(), "en-US");
    CharacterNameLoadResult const result = manager.Load();
    EXPECT_FALSE(result.Loaded);
    EXPECT_TRUE(Mentions(result.Errors, "the world database is not open"));
    EXPECT_EQ(manager.GetGeneration(), 0u);
    EXPECT_TRUE(manager.GetNames()->GetTables().empty());
    EXPECT_EQ(manager.Check(0, Male), NameCheck::UnknownLocale);
    EXPECT_FALSE(manager.FormatName(0, Male));
    EXPECT_FALSE(manager.IsDisallowed(0, Male));
    manager.SetDefaultLocale("de");
    EXPECT_EQ(manager.GetDefaultLocale(), "de");
}

TEST_F(CharacterNameMgrDatabaseTest, RowsLoadAndAReloadAppliesEditsOrKeepsTheLastGoodTables)
{
    CharacterNameMgr manager;
    CharacterNameLoadResult empty = manager.Load();
    ASSERT_TRUE(empty.Loaded) << (empty.Errors.empty() ? std::string() : empty.Errors.front());
    EXPECT_EQ(empty.Tables, 0u);
    EXPECT_EQ(empty.HumanLocales, 0u);
    EXPECT_TRUE(empty.Warnings.empty());
    EXPECT_EQ(manager.GetGeneration(), 1u);

    Execute("INSERT INTO `character_name_part` (`table_name`, `locale`, `idx`, `locale_key`, `text`) VALUES ('FirstName', 'fr', 0, 'First_0', 'Minou')");
    CharacterNameLoadResult const petsOnly = manager.Load();
    ASSERT_TRUE(petsOnly.Loaded);
    EXPECT_TRUE(Mentions(petsOnly.Warnings, "hold no locale with all four human name tables"));
    Execute("DELETE FROM `character_name_part`");
    InsertHumanTables("de", "J\xC3\xBCrgen");
    CharacterNameLoadResult const germanOnly = manager.Load();
    ASSERT_TRUE(germanOnly.Loaded);
    EXPECT_TRUE(Mentions(germanOnly.Warnings, "the default locale en-US has no human name tables, so names checked or shown without a locale fail; the tables have de"));
    Execute("DELETE FROM `character_name_part`");

    InsertHumanTables("en-US", "Blaze");
    InsertHumanTables("de", "J\xC3\xBCrgen");
    Execute("INSERT INTO `character_name_part` (`table_name`, `locale`, `idx`, `locale_key`, `text`) VALUES ('FirstName', 'fr', 0, 'First_0', 'Minou')");
    uint64 const before = manager.GetGeneration();
    Execute("INSERT INTO `character_name_disallowed` (`id`, `locale_id`, `gender`, `first_idx`, `middle_idx`, `last_idx`) VALUES (0, 1, 1, 1, 1, 999)");
    CharacterNameLoadResult const loaded = manager.Load();
    ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
    EXPECT_EQ(loaded.Tables, 9u);
    EXPECT_EQ(loaded.Parts, 17u);
    EXPECT_EQ(loaded.Disallowed, 1u);
    EXPECT_EQ(loaded.HumanLocales, 2u);
    EXPECT_TRUE(loaded.Warnings.empty());
    EXPECT_EQ(manager.GetGeneration(), before + 1);
    EXPECT_EQ(manager.FormatName(Pack(1, 1, 2), Male), "Blaze StormRider");
    EXPECT_EQ(manager.FormatName(Pack(1, 1, 2), Male, "de"), "J\xC3\xBCrgen StormRider");
    EXPECT_EQ(manager.FormatName(Pack(0, 0, 0), 0), "Abby");
    EXPECT_TRUE(manager.IsValidIndices(Pack(1, 1, 2), Male));
    EXPECT_FALSE(manager.IsValidIndices(Pack(2, 0, 0), Male));
    EXPECT_TRUE(manager.IsDisallowed(Pack(1, 1, 0), Male));
    EXPECT_EQ(manager.Check(Pack(1, 1, 2), Male, "en-US", 1), NameCheck::Disallowed);
    EXPECT_EQ(manager.Check(Pack(1, 1, 2), Male, "en-US", 3), NameCheck::Ok);
    ASSERT_TRUE(manager.GetNames()->FindTable("FirstName", "fr"));
    manager.SetDefaultLocale("de");
    EXPECT_EQ(manager.FormatName(Pack(1, 0, 1), Male), "J\xC3\xBCrgen Blade");
    manager.SetDefaultLocale("en-US");

    Execute("UPDATE `character_name_part` SET `text` = 'Frost' WHERE `table_name` = 'MiddleName_Human' AND `locale` = 'en-US' AND `idx` = 1");
    ASSERT_TRUE(manager.Load().Loaded);
    EXPECT_EQ(manager.FormatName(Pack(1, 1, 2), Male), "Blaze FrostRider");
    uint64 const good = manager.GetGeneration();

    Execute("UPDATE `character_name_part` SET `text` = '' WHERE `table_name` = 'FirstName_HumanMale' AND `locale` = 'en-US' AND `idx` = 1");
    CharacterNameLoadResult const emptyFirst = manager.Load();
    EXPECT_FALSE(emptyFirst.Loaded);
    EXPECT_TRUE(Mentions(emptyFirst.Errors, "FirstName_HumanMale (en-US) position 1: a name needs a locale key and text"));
    EXPECT_EQ(manager.FormatName(Pack(1, 1, 2), Male), "Blaze FrostRider");
    EXPECT_EQ(manager.GetGeneration(), good);
    Execute("UPDATE `character_name_part` SET `text` = 'Blaze' WHERE `table_name` = 'FirstName_HumanMale' AND `locale` = 'en-US' AND `idx` = 1");

    Execute("DELETE FROM `character_name_part` WHERE `table_name` = 'LastName_Human' AND `locale` = 'de' AND `idx` = 1");
    Execute("UPDATE `character_name_disallowed` SET `gender` = 7");
    CharacterNameLoadResult const gap = manager.Load();
    EXPECT_FALSE(gap.Loaded);
    ASSERT_EQ(gap.Errors.size(), 2u);
    EXPECT_EQ(gap.Errors.front(), "character_name_part LastName_Human (de) has position 2 where position 1 should be; positions must run from 0 without gaps");
    EXPECT_EQ(gap.Errors.back(), "disallowed name 0 has gender 7; 0 is female and 1 is male");
    Execute("UPDATE `character_name_disallowed` SET `gender` = 1");
    EXPECT_EQ(manager.FormatName(Pack(1, 0, 2), Male, "de"), "J\xC3\xBCrgen Rider");
    Execute("INSERT INTO `character_name_part` (`table_name`, `locale`, `idx`, `locale_key`, `text`) VALUES ('LastName_Human', 'de', 1, 'Last_0', 'Klinge')");

    Execute("UPDATE `character_name_disallowed` SET `gender` = 5");
    CharacterNameLoadResult const gender = manager.Load();
    EXPECT_FALSE(gender.Loaded);
    EXPECT_TRUE(Mentions(gender.Errors, "disallowed name 0 has gender 5"));
    Execute("UPDATE `character_name_disallowed` SET `gender` = 1");

    Execute("DROP TABLE `character_name_disallowed`");
    CharacterNameLoadResult const missing = manager.Load();
    EXPECT_FALSE(missing.Loaded);
    EXPECT_TRUE(Mentions(missing.Errors, "character_name_part and character_name_disallowed cannot be read from the world database"));
    QueryResult const stillUsable = WorldDatabase.Query("SELECT COUNT(*) FROM `character_name_part`");
    ASSERT_TRUE(stillUsable);
    EXPECT_EQ((*stillUsable)[0].Get<uint64>(), 17u);
    EXPECT_EQ(manager.GetGeneration(), good);
    EXPECT_EQ(manager.FormatName(Pack(1, 0, 1), Male, "de"), "J\xC3\xBCrgen Blade");
    manager.Clear();
    EXPECT_FALSE(manager.FormatName(Pack(1, 0, 1), Male));
    EXPECT_GT(manager.GetGeneration(), good);

    WorldDatabase.Close();
    _open = false;
    CharacterNameLoadResult const closed = manager.Load();
    EXPECT_FALSE(closed.Loaded);
    EXPECT_TRUE(Mentions(closed.Errors, "the world database is not open"));
}
