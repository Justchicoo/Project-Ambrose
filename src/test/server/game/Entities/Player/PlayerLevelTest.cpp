/*
 * Project Ambrose by Imjustchico
 * Tests experience and levels: enough experience for two and a half levels from level 1 ends at level 3 with the remainder kept and each level's training points gained, at the level cap every point goes to overflow and the level stands, a locked level fills its bar to the next level and sends the rest to overflow until it is unlocked, a quest's grant follows Rate.XP.Quest as a live setting while a command's does not, and a game master's level change moves the wizard to that level's start at full health, refusing a level its school does not reach.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "Player.h"
#include "PlayerLevel.h"
#include "PlayerStatsFixtures.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using PlayerStatsFixtures::Fire;

    class PlayerLevelTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> errors;
            _levels = PlayerLevelSet::Build(PlayerStatsFixtures::FireLevels(5), errors);
            ASSERT_TRUE(_levels) << errors.front();
            _effects = StatEffectSet::Build({ {}, {}, {} }, errors);
            ASSERT_TRUE(_effects) << errors.front();
        }

        Player Enter(int32 level, int32 experience, std::optional<CharacterStats> const& stored = std::nullopt)
        {
            CharacterSummary character;
            character.Guid = 7;
            character.SchoolId = Fire;
            character.Level = level;
            character.Experience = experience;
            std::string problem;
            std::optional<PlayerStats> stats = PlayerStats::Create(character, stored, *_levels, *_effects, problem);
            EXPECT_TRUE(stats) << problem;
            return Player(std::move(*stats));
        }

        std::shared_ptr<PlayerLevelSet const> _levels;
        std::shared_ptr<StatEffectSet const> _effects;
    };
}

TEST_F(PlayerLevelTest, TwoAndAHalfLevelsFromLevelOneEndAtLevelThreeWithTheRemainderAndEachLevelsTrainingPoints)
{
    Player wizard = Enter(1, 0);
    ASSERT_EQ(wizard.GetStats().GetTrainingPoints(), 1);
    LevelProgress const progress = wizard.GiveExperience(292, *_levels);
    EXPECT_EQ(progress.OldLevel, 1);
    EXPECT_EQ(progress.NewLevel, 3);
    EXPECT_EQ(progress.OldExperience, 0);
    EXPECT_EQ(progress.NewExperience, 292) << "experience is the total, so the remainder past level 3's 180 is 112";
    EXPECT_EQ(progress.TrainingPointsGained, 1) << "level 2 gives none and level 3 gives one in the fixture";
    EXPECT_EQ(progress.OverflowGained, 0);
    EXPECT_TRUE(progress.LeveledUp());
    EXPECT_EQ(wizard.GetStats().GetLevel(), 3);
    EXPECT_EQ(wizard.GetStats().GetTrainingPoints(), 2);
    EXPECT_EQ(wizard.GetStats().GetMaxHitpoints(), 445);
    EXPECT_EQ(wizard.GetStats().GetHitpoints(), 445) << "a new level fills health";
    EXPECT_EQ(wizard.GetStats().GetMana(), wizard.GetStats().GetMaxMana());
    EXPECT_TRUE(wizard.HasDirtyProgress());
    EXPECT_TRUE(wizard.HasDirtyStats());
}

TEST_F(PlayerLevelTest, AtTheLevelCapEveryPointGoesToOverflowAndTheLevelStands)
{
    Player wizard = Enter(5, 720, CharacterStats{});
    LevelProgress const progress = wizard.GiveExperience(100, *_levels);
    EXPECT_FALSE(progress.LeveledUp());
    EXPECT_EQ(progress.NewExperience, 720);
    EXPECT_EQ(progress.OverflowGained, 100);
    EXPECT_EQ(wizard.GetStats().GetOverflowXp(), 100);
    EXPECT_EQ(wizard.GetStats().GetLevel(), 5);
    EXPECT_FALSE(wizard.HasDirtyProgress()) << "nothing the character row keeps moved";
    EXPECT_TRUE(wizard.HasDirtyStats()) << "the overflow is kept in character_stats";
}

TEST_F(PlayerLevelTest, ALockedLevelFillsItsBarAndSendsTheRestToOverflowUntilUnlocked)
{
    Player wizard = Enter(1, 0);
    ASSERT_TRUE(wizard.SetLevelLocked(true));
    LevelProgress const locked = wizard.GiveExperience(1000, *_levels);
    EXPECT_FALSE(locked.LeveledUp());
    EXPECT_EQ(locked.NewExperience, 45) << "the bar fills to the next level and stops";
    EXPECT_EQ(locked.OverflowGained, 955);
    EXPECT_EQ(wizard.GetStats().GetLevel(), 1);

    ASSERT_TRUE(wizard.SetLevelLocked(false));
    EXPECT_FALSE(wizard.SetLevelLocked(false));
    LevelProgress const unlocked = wizard.GiveExperience(1, *_levels);
    EXPECT_EQ(unlocked.NewLevel, 2);
    EXPECT_EQ(unlocked.NewExperience, 46);
    EXPECT_EQ(unlocked.OverflowGained, 0);
}

TEST_F(PlayerLevelTest, AQuestGrantFollowsRateXpQuestLiveWhileACommandGrantDoesNot)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("gameserver.conf", "");
    ConfigMgr config([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    sSettings.Clear();
    std::vector<std::string> errors;
    ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
    std::vector<std::string> warnings;
    ASSERT_TRUE(sSettings.Start(config, std::make_shared<MemorySettingStore>(), warnings));

    EXPECT_EQ(PlayerLevel::ScaleExperience(100, XpSource::Quest), 100);
    SettingAuthor const author{ "test", 1, "unit_test" };
    ASSERT_TRUE(sSettings.Set("Rate.XP.Quest", "2", author, "double quest experience").Ok());
    EXPECT_EQ(PlayerLevel::ScaleExperience(100, XpSource::Quest), 200);
    EXPECT_EQ(PlayerLevel::ScaleExperience(100, XpSource::Kill), 100);
    EXPECT_EQ(PlayerLevel::ScaleExperience(100, XpSource::Command), 100);
    ASSERT_TRUE(sSettings.Set("Rate.XP.Quest", "0.5", author, "half").Ok());
    EXPECT_EQ(PlayerLevel::ScaleExperience(75, XpSource::Quest), 38) << "rounded to the nearest point";
    EXPECT_EQ(PlayerLevel::ScaleExperience(-5, XpSource::Quest), 0);
    sSettings.Clear();
}

TEST_F(PlayerLevelTest, AGameMasterMovesAWizardToALevelsStartAtFullHealthAndIsRefusedOneItsSchoolDoesNotReach)
{
    Player wizard = Enter(1, 20);
    ASSERT_TRUE(wizard.SetHealth(1));
    std::optional<LevelProgress> const raised = wizard.SetLevel(4, *_levels);
    ASSERT_TRUE(raised);
    EXPECT_EQ(raised->NewLevel, 4);
    EXPECT_EQ(raised->NewExperience, 405) << "level 4 starts where level 3's total ends";
    EXPECT_EQ(raised->TrainingPointsGained, 1);
    EXPECT_EQ(wizard.GetStats().GetTrainingPoints(), 2);
    EXPECT_EQ(wizard.GetStats().GetHitpoints(), 460);
    EXPECT_TRUE(wizard.HasDirtyProgress());

    std::optional<LevelProgress> const lowered = wizard.SetLevel(2, *_levels);
    ASSERT_TRUE(lowered);
    EXPECT_EQ(lowered->NewExperience, 45);
    EXPECT_EQ(lowered->TrainingPointsGained, -1);
    EXPECT_EQ(wizard.GetStats().GetTrainingPoints(), 1);
    EXPECT_FALSE(wizard.SetLevel(9, *_levels));
    EXPECT_FALSE(wizard.SetLevel(0, *_levels));
}
