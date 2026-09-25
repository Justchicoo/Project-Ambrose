/*
 * Project Ambrose by Imjustchico
 * Tests the level set: a complete set answers a school's row by id or name, the cap's row above the cap and nothing below 0 or for a school without rows, a mob rank's level with the highest rank's level for a rank it lacks and 0 for none, and MagicXPConfig's settings by name; an empty set is valid; and each kind of bad row is refused with its reason, all of them reported together.
 */

#include "PlayerLevels.h"
#include "StringHash.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace
{
    uint32 const Fire = StringHash::KiStringHash("Fire");
    uint32 const Ice = StringHash::KiStringHash("Ice");
    uint32 const Moon = StringHash::KiStringHash("Moon");

    PlayerLevelInfo Row(uint32 school, uint32 level, int32 hitpoints)
    {
        PlayerLevelInfo row;
        row.SchoolId = school;
        row.Level = level;
        row.XpToLevel = static_cast<int32>(level * 100);
        row.Hitpoints = hitpoints;
        row.Mana = 10 + static_cast<int32>(level);
        row.LevelName = "Levels_Level" + std::to_string(level);
        return row;
    }

    PlayerLevelData Complete()
    {
        PlayerLevelData data;
        for (uint32 level = 0; level <= 2; ++level)
        {
            data.Levels.push_back(Row(Fire, level, 400 + static_cast<int32>(level)));
            data.Levels.push_back(Row(Ice, level, 500 + static_cast<int32>(level)));
        }
        data.Schools = { { Fire, "Fire", 1, 1, { "FireWeaving-Level01" } }, { Ice, "Ice", 1, 2, {} }, { Moon, "Moon", 2, 10, {} } };
        data.XpConfig = { { "m_maxSchoolLevel", 2.0 }, { "m_experienceBonus", 3.0 } };
        data.EncounterXpFactors = { 1.0f, 0.5f, 0.0f };
        data.MobRanks = { { 1, 1 }, { 3, 11 }, { 2, 6 } };
        return data;
    }

    bool Mentions(std::vector<std::string> const& errors, std::string_view text)
    {
        return std::any_of(errors.begin(), errors.end(), [text](std::string const& error) { return error.find(text) != std::string::npos; });
    }

    std::string Report(std::vector<std::string> const& errors)
    {
        std::string report;
        for (std::string const& error : errors)
            report += error + "\n";
        return report;
    }
}

TEST(PlayerLevelsTest, ACompleteSetAnswersEverySchoolLevelRankAndSetting)
{
    std::vector<std::string> errors;
    std::shared_ptr<PlayerLevelSet const> const set = PlayerLevelSet::Build(Complete(), errors);
    ASSERT_TRUE(set) << Report(errors);
    EXPECT_TRUE(errors.empty());
    EXPECT_FALSE(set->IsEmpty());
    EXPECT_EQ(set->GetMaxLevel(), 2u);
    EXPECT_EQ(set->GetLevelTableCount(), 2u);

    PlayerLevelInfo const* const fire = set->GetInfo(Fire, 1);
    ASSERT_TRUE(fire);
    EXPECT_EQ(fire->Hitpoints, 401);
    EXPECT_EQ(fire->Mana, 11);
    EXPECT_EQ(set->GetInfo("Ice", 2)->Hitpoints, 502);
    EXPECT_EQ(set->GetInfo(Fire, 0)->Level, 0u);
    EXPECT_EQ(set->GetInfo(Fire, 3), set->GetInfo(Fire, 2));
    EXPECT_EQ(set->GetInfo(Fire, 70000)->Level, 2u);
    EXPECT_FALSE(set->GetInfo(Fire, -1));
    EXPECT_FALSE(set->GetInfo(Moon, 1));
    EXPECT_FALSE(set->GetInfo("Shadow", 1));
    EXPECT_FALSE(set->GetInfo(12345u, 1));

    ASSERT_TRUE(set->FindSchool("Moon"));
    EXPECT_EQ(set->FindSchool("Moon")->Index, 10);
    EXPECT_EQ(set->FindSchool(Fire)->Badges, (std::vector<std::string>{ "FireWeaving-Level01" }));
    EXPECT_FALSE(set->FindSchool("fire"));

    EXPECT_EQ(set->GetXpSetting("m_experienceBonus"), 3.0);
    EXPECT_FALSE(set->GetXpSetting("m_schoolOfFocusBonus"));
    EXPECT_EQ(set->GetLevelForRank(2), 6);
    EXPECT_EQ(set->GetLevelForRank(3), 11);
    EXPECT_EQ(set->GetLevelForRank(9), 11);
    EXPECT_EQ(set->GetLevelForRank(0), 0);
    EXPECT_EQ(set->GetLevelForRank(-4), 0);
    EXPECT_EQ(set->GetData().EncounterXpFactors, (std::vector<float>{ 1.0f, 0.5f, 0.0f }));
}

TEST(PlayerLevelsTest, AnEmptySetIsValidAndAnswersNothing)
{
    std::vector<std::string> errors;
    std::shared_ptr<PlayerLevelSet const> const set = PlayerLevelSet::Build({}, errors);
    ASSERT_TRUE(set) << Report(errors);
    EXPECT_TRUE(set->IsEmpty());
    EXPECT_EQ(set->GetMaxLevel(), 0u);
    EXPECT_FALSE(set->GetInfo(Fire, 1));
    EXPECT_EQ(set->GetLevelForRank(1), 0);
    PlayerLevelSet const constructed;
    EXPECT_TRUE(constructed.IsEmpty());
    EXPECT_FALSE(constructed.GetInfo(Fire, 0));
}

TEST(PlayerLevelsTest, EachBadRowIsRefusedWithItsReason)
{
    auto const refused = [](PlayerLevelData data, std::string_view expected)
    {
        std::vector<std::string> errors;
        EXPECT_FALSE(PlayerLevelSet::Build(std::move(data), errors)) << expected;
        EXPECT_TRUE(Mentions(errors, expected)) << "wanted: " << expected << "\n" << Report(errors);
    };

    PlayerLevelData data = Complete();
    data.Schools[1].Id = 7;
    refused(data, "magic_school_template gives Ice the id 7, where the client's hash of that name is");
    data = Complete();
    data.Schools.push_back({ Fire, "Fire", 1, 1, {} });
    refused(data, "magic_school_template lists the school Fire twice");
    data = Complete();
    data.Schools[2].Name.clear();
    refused(data, "magic_school_template has a school");
    data = Complete();
    data.Schools[0].Badges.push_back("");
    refused(data, "magic_school_badge gives Fire a badge at position 1 that is 0 bytes");
    data = Complete();
    data.Schools[0].Badges.assign(PlayerLevelSet::MaxBadges + 1, "Badge");
    refused(data, "magic_school_badge gives Fire 257 badges");
    data = Complete();
    data.XpConfig.erase(data.XpConfig.begin());
    refused(data, "magic_xp_config does not give m_maxSchoolLevel");
    data = Complete();
    data.XpConfig[0].Value = 2.5;
    refused(data, "magic_xp_config gives m_maxSchoolLevel as 2.5");
    data = Complete();
    data.XpConfig.push_back({ "m_experienceBonus", 1.0 });
    refused(data, "magic_xp_config lists m_experienceBonus twice");
    data = Complete();
    data.XpConfig[1].Value = std::numeric_limits<double>::quiet_NaN();
    refused(data, "magic_xp_config gives m_experienceBonus a value that is not a finite number");
    data = Complete();
    data.EncounterXpFactors[1] = std::numeric_limits<float>::infinity();
    refused(data, "magic_xp_encounter_factor gives position 1 a factor that is not a finite number");
    data = Complete();
    data.MobRanks.push_back({ 2, 9 });
    refused(data, "mob_rank_level lists rank 2 twice");
    data = Complete();
    data.Levels.push_back(Row(Moon + 1, 0, 1));
    refused(data, "player_level_stats has a row for school id");
    data = Complete();
    data.Levels.push_back(Row(Fire, 1, 1));
    refused(data, "player_level_stats lists Fire at level 1 twice");
    data = Complete();
    data.Levels.push_back(Row(Fire, 3, 1));
    refused(data, "player_level_stats gives Fire a row at level 3, above the level cap of 2");
    data = Complete();
    data.Levels.erase(data.Levels.begin() + 2);
    refused(data, "player_level_stats has no row for Fire at level 1, and every school's rows run from 0 to the cap of 2");
    data = Complete();
    data.Levels[0].PipChance = std::numeric_limits<float>::quiet_NaN();
    refused(data, "player_level_stats gives Fire at level 0 a pip chance, shadow pip rating or archmastery that is not a finite number");
    data = Complete();
    data.Levels[0].LevelName.assign(PlayerLevelSet::MaxLevelNameBytes + 1, 'x');
    refused(data, "a level name of 129 bytes");
    data = Complete();
    data.Levels.clear();
    refused(data, "player_level_stats has no rows");
    data = Complete();
    data.XpConfig.clear();
    data.Levels.push_back(Row(Fire, 70000, 1));
    refused(data, "player_level_stats gives Fire a row at level 70000, above the level cap of 65535");

    data = Complete();
    data.Schools[1].Id = 7;
    data.MobRanks.push_back({ 1, 2 });
    data.Levels.push_back(Row(Fire, 2, 1));
    std::vector<std::string> errors;
    EXPECT_FALSE(PlayerLevelSet::Build(std::move(data), errors));
    EXPECT_GE(errors.size(), 3u) << Report(errors);
}
