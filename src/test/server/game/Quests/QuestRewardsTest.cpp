/*
 * Project Ambrose by Imjustchico
 * Tests quest rewards: a quest's ResAddGold and ResAddMagicXP results read back as the gold and experience the data gives, the rewards a result list shows follow Rate.Gold.Quest and Rate.XP.Quest as live settings, changed without a restart, with every result's gold summed into one gold info, and a result of another class or one that gives nothing reads as no reward.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "QuestRewards.h"
#include "ResultFixtures.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace
{
    class QuestRewardsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::filesystem::path const file = _directory.Write("gameserver.conf", "");
            ASSERT_TRUE(_config.LoadInitial(file).Succeeded());
            sSettings.Clear();
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(_config, std::make_shared<MemorySettingStore>(), warnings));
        }

        void TearDown() override { sSettings.Clear(); }

        static void Set(std::string const& key, std::string const& value)
        {
            ASSERT_TRUE(sSettings.Set(key, value, SettingAuthor{ "test", 1, "unit_test" }, "quest reward rate").Ok());
        }

        LogTestDirectory _directory;
        ConfigMgr _config{ [](std::string const&) -> std::optional<std::string> { return std::nullopt; } };
    };

    std::vector<QuestWireEncoder::MagicXPLootInfo> Experience(QuestWireEncoder::LootInfoList const& loot)
    {
        std::vector<QuestWireEncoder::MagicXPLootInfo> found;
        for (QuestWireEncoder::LootInfo const& info : loot.Loot)
            if (QuestWireEncoder::MagicXPLootInfo const* const experience = std::get_if<QuestWireEncoder::MagicXPLootInfo>(&info))
                found.push_back(*experience);
        return found;
    }
}

TEST_F(QuestRewardsTest, AQuestsGoldAndExperienceFollowTheLiveQuestRatesWithoutARestart)
{
    ResultList const results{ ResultFixtures::Gold(100, 0), ResultFixtures::NotifyText("Quest_Done", 0), ResultFixtures::Experience(50, "Fire", 2), ResultFixtures::Gold(25, 3) };
    QuestWireEncoder::LootInfoList const plain = QuestRewards::Preview(results);
    ASSERT_TRUE(plain.GoldInfo);
    EXPECT_EQ(plain.GoldInfo->GoldAmount, 125) << "every result's gold goes into the one gold info";
    std::vector<QuestWireEncoder::MagicXPLootInfo> experience = Experience(plain);
    ASSERT_EQ(experience.size(), 1u);
    EXPECT_EQ(plain.Loot.size(), 1u) << "the notify text gives nothing";
    EXPECT_EQ(experience[0].MagicSchool, "Fire");
    EXPECT_EQ(experience[0].Experience, 50);

    Set("Rate.Gold.Quest", "2");
    Set("Rate.XP.Quest", "1.5");
    QuestWireEncoder::LootInfoList const scaled = QuestRewards::Preview(results);
    ASSERT_TRUE(scaled.GoldInfo);
    EXPECT_EQ(scaled.GoldInfo->GoldAmount, 250) << "the next reward reads the rate set a moment ago";
    experience = Experience(scaled);
    ASSERT_EQ(experience.size(), 1u);
    EXPECT_EQ(experience[0].Experience, 75);
    EXPECT_EQ(QuestRewards::ScaleGold(25), 50);

    Set("Rate.Gold.Quest", "0.5");
    EXPECT_EQ(QuestRewards::ScaleGold(25), 13) << "rounded to the nearest coin";
    EXPECT_EQ(QuestRewards::ScaleGold(-5), 0);
}

TEST_F(QuestRewardsTest, AResultOfAnotherClassOrOneThatGivesNothingReadsAsNoReward)
{
    ResultEntry const gold = ResultFixtures::Gold(40);
    ResultEntry const experience = ResultFixtures::Experience(10, "Ice");
    ASSERT_TRUE(gold.Object && experience.Object);
    std::optional<QuestWireEncoder::GoldLootInfo> const read = QuestRewards::GoldOf(*gold.Object);
    ASSERT_TRUE(read);
    EXPECT_EQ(read->GoldAmount, 40) << "the data's amount, before any rate";
    EXPECT_FALSE(QuestRewards::ExperienceOf(*gold.Object));
    EXPECT_FALSE(QuestRewards::GoldOf(*experience.Object));
    ResultEntry const notify = ResultFixtures::NotifyText("Quest_Done", 0);
    ASSERT_TRUE(notify.Object);
    EXPECT_FALSE(QuestRewards::GoldOf(*notify.Object));
    ResultEntry const none = ResultFixtures::Gold(0);
    ASSERT_TRUE(none.Object);
    EXPECT_FALSE(QuestRewards::GoldOf(*none.Object));
    QuestWireEncoder::LootInfoList const empty = QuestRewards::Preview({ none, notify });
    EXPECT_FALSE(empty.GoldInfo);
    EXPECT_TRUE(empty.Loot.empty());
}
