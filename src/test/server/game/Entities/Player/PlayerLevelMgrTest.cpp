/*
 * Project Ambrose by Imjustchico
 * Tests the level manager: a closed world database is reported and keeps the empty sets, and with AMBROSE_TEST_DB set it installs the world schema and checks that empty tables load as empty, rows the test writes load with Fire's level 1 matching its row, an edited row applies through the player_level_stats reload target, a reload that meets a gap, a badge out of order or a school id that is not its name's hash keeps the serving set and names each fault, and the stat tables reload on their own target; and a reload reaches the next wizard to enter the world, whose stats are built from the set then serving.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "PlayerLevelMgr.h"
#include "PlayerStats.h"
#include "ReloadMgr.h"
#include "StringHash.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    uint32 const Fire = StringHash::KiStringHash("Fire");

    bool Mentions(std::vector<std::string> const& errors, std::string_view text)
    {
        return std::any_of(errors.begin(), errors.end(), [text](std::string const& error) { return error.find(text) != std::string::npos; });
    }

    class PlayerLevelMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            _info = *info;
            _info.Database = fmt::format("ambrose_world_levels_{:08x}", std::random_device()());
            ASSERT_TRUE(DBUpdater::Run(_info, "world", UpdaterSettings{}));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            sReloadMgr.Clear();
            sPlayerLevelMgr.Clear();
            sPlayerLevelMgr.RegisterReloadTargets();
        }

        void TearDown() override
        {
            sPlayerLevelMgr.Clear();
            sReloadMgr.Clear();
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

        static void InsertLevels()
        {
            Execute(fmt::format("INSERT INTO `magic_school_template` (`school_id`, `school_name`, `min_level`, `school_index`) VALUES ({}, 'Fire', 1, 1)", Fire));
            Execute(fmt::format("INSERT INTO `magic_school_badge` (`school_id`, `position`, `badge_name`) VALUES ({0}, 0, 'FireWeaving-Level01'), ({0}, 1, 'FireWeaving-Level02')", Fire));
            Execute("INSERT INTO `magic_xp_config` (`name`, `value`) VALUES ('m_maxSchoolLevel', 1), ('m_experienceBonus', 3)");
            Execute("INSERT INTO `magic_xp_encounter_factor` (`position`, `factor`) VALUES (0, 1), (1, 0.5)");
            Execute("INSERT INTO `mob_rank_level` (`rank`, `level`) VALUES (1, 1), (2, 6)");
            Execute(fmt::format("INSERT INTO `player_level_stats` (`school_id`, `level`, `xp_to_level`, `hitpoints`, `mana`, `gold`, `pip_chance`, `training_points`, `pet_energy`, `pip_conversion_fire`, "
                "`shadow_pip_rating`, `level_name`) VALUES ({0}, 0, 0, 0, 0, 0, 0.1, 0, 0, 0, 0, 'Levels_Level0'), ({0}, 1, 45, 415, 15, 300000, 0, 2, 40, 12, 5, 'Levels_Level1')", Fire));
        }

        MySQLConnectionInfo _info;
        bool _open = false;
    };
}

TEST(PlayerLevelMgrClosedTest, AClosedWorldDatabaseIsReportedAndKeepsTheEmptySets)
{
    WorldDatabase.Close();
    PlayerLevelMgr manager;
    PlayerLevelLoadResult const result = manager.Load();
    EXPECT_FALSE(result.Loaded);
    EXPECT_TRUE(Mentions(result.Errors, "the world database is not open, so the level tables cannot be read"));
    EXPECT_TRUE(Mentions(result.Errors, "the world database is not open, so the stat tables cannot be read"));
    EXPECT_TRUE(result.Empty);
    EXPECT_EQ(manager.GetLevelGeneration(), 0u);
    EXPECT_TRUE(manager.GetLevels()->IsEmpty());
    EXPECT_TRUE(manager.GetStats()->IsEmpty());
    EXPECT_FALSE(manager.GetLevels()->GetInfo(Fire, 1));
}

TEST_F(PlayerLevelMgrTest, RowsLoadAndAReloadAppliesEditsOrKeepsTheServingSet)
{
    PlayerLevelLoadResult const empty = sPlayerLevelMgr.Load();
    ASSERT_TRUE(empty.Loaded) << (empty.Errors.empty() ? std::string() : empty.Errors.front());
    EXPECT_TRUE(empty.Empty);

    InsertLevels();
    Execute("INSERT INTO `stat_effect_config` (`name`, `value`) VALUES ('m_shadowPipMax', 2), ('m_criticalDamageAddPercentPvP', 0.30000001192092896)");
    Execute("INSERT INTO `stat_crit_block_band` (`min_level`, `position`, `cap_value`, `critical_hit_scalar_base`, `critical_hit_scaling_factor`, `block_scalar_base`, `block_scaling_factor`) VALUES (0, 0, 1, 0.00205, 0.002, 0.00255, 0.002)");
    Execute("INSERT INTO `stat_pip_conversion_band` (`min_level`, `position`, `cap_value`, `scalar_base`, `scaling_factor`) VALUES (0, 0, 1, 0.00205, 0.009), (0, 1, 0.33, 0.00205, 0.0085)");
    PlayerLevelLoadResult const loaded = sPlayerLevelMgr.Load();
    ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
    EXPECT_FALSE(loaded.Empty);
    EXPECT_EQ(loaded.Schools, 1u);
    EXPECT_EQ(loaded.LevelTables, 1u);
    EXPECT_EQ(loaded.MaxLevel, 1u);
    EXPECT_EQ(loaded.Levels, 2u);
    EXPECT_EQ(loaded.StatSettings, 2u);
    EXPECT_EQ(loaded.BandValues, 3u);

    std::shared_ptr<PlayerLevelSet const> const first = sPlayerLevelMgr.GetLevels();
    PlayerLevelInfo const* const fire = first->GetInfo("Fire", 1);
    ASSERT_TRUE(fire);
    EXPECT_EQ(fire->Hitpoints, 415);
    EXPECT_EQ(fire->Mana, 15);
    EXPECT_EQ(fire->TrainingPoints, 2);
    EXPECT_EQ(fire->XpToLevel, 45);
    EXPECT_EQ(fire->Gold, 300000);
    EXPECT_EQ(fire->PetEnergy, 40);
    EXPECT_EQ(fire->PipConversion[0], 12);
    EXPECT_EQ(fire->ShadowPipRating, 5.0f);
    EXPECT_EQ(fire->LevelName, "Levels_Level1");
    EXPECT_EQ(first->GetInfo(Fire, 0)->PipChance, 0.1f);
    EXPECT_EQ(first->FindSchool(Fire)->Badges, (std::vector<std::string>{ "FireWeaving-Level01", "FireWeaving-Level02" }));
    EXPECT_EQ(first->GetData().EncounterXpFactors, (std::vector<float>{ 1.0f, 0.5f }));
    EXPECT_EQ(first->GetLevelForRank(2), 6);
    EXPECT_EQ(sPlayerLevelMgr.GetStats()->Get("m_criticalDamageAddPercentPvP"), 0.30000001192092896);
    EXPECT_EQ(sPlayerLevelMgr.GetStats()->GetData().PipConversion[1].CapValue, 0.33f);

    Execute(fmt::format("UPDATE `player_level_stats` SET `hitpoints` = 450, `mana` = 16 WHERE `school_id` = {} AND `level` = 1", Fire));
    uint64 const before = sPlayerLevelMgr.GetLevelGeneration();
    ReloadOutcome const edited = sReloadMgr.Reload(PlayerLevelMgr::LevelTarget);
    ASSERT_TRUE(edited.Ok) << (edited.Errors.empty() ? std::string() : edited.Errors.front());
    EXPECT_EQ(sPlayerLevelMgr.GetLevelGeneration(), before + 1);
    EXPECT_EQ(sPlayerLevelMgr.GetLevels()->GetInfo(Fire, 1)->Hitpoints, 450);
    EXPECT_EQ(sPlayerLevelMgr.GetLevels()->GetInfo(Fire, 1)->Mana, 16);
    EXPECT_EQ(fire->Hitpoints, 415);

    Execute(fmt::format("DELETE FROM `player_level_stats` WHERE `school_id` = {} AND `level` = 0", Fire));
    Execute(fmt::format("UPDATE `magic_school_badge` SET `position` = 5 WHERE `school_id` = {} AND `position` = 1", Fire));
    Execute(fmt::format("INSERT INTO `magic_school_template` (`school_id`, `school_name`, `min_level`, `school_index`) VALUES (7, 'Ice', 1, 2)"));
    ReloadOutcome const broken = sReloadMgr.Reload(PlayerLevelMgr::LevelTarget);
    EXPECT_FALSE(broken.Ok);
    EXPECT_TRUE(Mentions(broken.Errors, "player_level_stats has no row for Fire at level 0")) << fmt::format("{}", fmt::join(broken.Errors, "\n"));
    EXPECT_TRUE(Mentions(broken.Errors, "magic_school_badge gives Fire a badge at position 5 where position 1 comes next"));
    EXPECT_TRUE(Mentions(broken.Errors, "magic_school_template gives Ice the id 7"));
    EXPECT_EQ(sPlayerLevelMgr.GetLevels()->GetInfo(Fire, 1)->Hitpoints, 450);
    EXPECT_EQ(sPlayerLevelMgr.GetLevelGeneration(), before + 1);

    Execute("UPDATE `stat_effect_config` SET `value` = 3 WHERE `name` = 'm_shadowPipMax'");
    ReloadOutcome const stats = sReloadMgr.Reload(PlayerLevelMgr::StatTarget);
    ASSERT_TRUE(stats.Ok) << (stats.Errors.empty() ? std::string() : stats.Errors.front());
    EXPECT_EQ(sPlayerLevelMgr.GetStats()->Get("m_shadowPipMax"), 3.0);
    Execute("INSERT INTO `stat_pip_conversion_band` (`min_level`, `position`, `cap_value`, `scalar_base`, `scaling_factor`) VALUES (0, 3, 0.1, 0, 0)");
    ReloadOutcome const gap = sReloadMgr.Reload(PlayerLevelMgr::StatTarget);
    EXPECT_FALSE(gap.Ok);
    EXPECT_TRUE(Mentions(gap.Errors, "stat_pip_conversion_band gives the band from level 0 3 positions that do not run from 0 without a gap"));
    EXPECT_EQ(sPlayerLevelMgr.GetStats()->Get("m_shadowPipMax"), 3.0);
}

TEST_F(PlayerLevelMgrTest, AReloadReachesTheNextWizardToEnterTheWorld)
{
    InsertLevels();
    ASSERT_TRUE(sPlayerLevelMgr.Load().Loaded);
    CharacterSummary wizard;
    wizard.Guid = 9;
    wizard.SchoolId = Fire;
    wizard.Level = 1;
    std::string problem;
    std::optional<PlayerStats> const before = PlayerStats::Create(wizard, std::nullopt, *sPlayerLevelMgr.GetLevels(), *sPlayerLevelMgr.GetStats(), problem);
    ASSERT_TRUE(before) << problem;
    EXPECT_EQ(before->GetMaxHitpoints(), 415);
    EXPECT_EQ(before->GetMaxMana(), 15);
    EXPECT_EQ(before->GetTrainingPoints(), 2);

    Execute(fmt::format("UPDATE `player_level_stats` SET `hitpoints` = 450, `mana` = 16, `training_points` = 3 WHERE `school_id` = {} AND `level` = 1", Fire));
    ReloadOutcome const reloaded = sReloadMgr.Reload(PlayerLevelMgr::LevelTarget);
    ASSERT_TRUE(reloaded.Ok) << (reloaded.Errors.empty() ? std::string() : reloaded.Errors.front());
    std::optional<PlayerStats> const after = PlayerStats::Create(wizard, std::nullopt, *sPlayerLevelMgr.GetLevels(), *sPlayerLevelMgr.GetStats(), problem);
    ASSERT_TRUE(after) << problem;
    EXPECT_EQ(after->GetMaxHitpoints(), 450);
    EXPECT_EQ(after->GetMaxMana(), 16);
    EXPECT_EQ(after->GetTrainingPoints(), 3);
    EXPECT_EQ(after->GetHitpoints(), 450) << "a wizard at full health is full of the new maximum";
    EXPECT_EQ(before->GetMaxHitpoints(), 415) << "a wizard already in the world keeps the stats it entered with";
}
