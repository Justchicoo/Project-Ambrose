/*
 * Project Ambrose by Imjustchico
 * Tests quest results: a quest's and each goal's result rows read into lists in position order, a row that does not read and a goal the quest does not hold are left out with the bad row named, and a quest log keeps each goal and quest it completes, once, until its session takes them to run their results.
 */

#include "ObjectGuid.h"
#include "QuestLog.h"
#include "QuestStore.h"
#include "ResultFixtures.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr uint64 Now = 1700000000;

    std::string TextOf(ResultEntry const& result)
    {
        PropertyValue const* const value = result.Object->Get("m_text");
        std::string const* const text = value ? value->GetIf<std::string>() : nullptr;
        return text ? *text : std::string();
    }

    QuestResultRow Row(std::string goal, uint32 position, std::string_view className, std::vector<uint8> data)
    {
        return QuestResultRow{ "Chain", std::move(goal), position, std::string(className), std::move(data) };
    }
}

TEST(QuestResultTest, AQuestAndItsGoalsReadTheirResultsInOrderAndNameARowThatDoesNotRead)
{
    QuestRows rows;
    QuestTemplateRow quest;
    quest.Name = "Chain";
    rows.Quests.push_back(quest);
    QuestGoalRow goal;
    goal.Quest = "Chain";
    goal.Goal = "TalkToHeadmaster";
    goal.Type = "persona";
    rows.Goals.push_back(goal);
    std::vector<uint8> const second = ResultFixtures::Encode(ResultFixtures::Make(ResultFixtures::SecondTestClass, "next"));
    std::vector<uint8> const first = ResultFixtures::Encode(ResultFixtures::Make(ResultFixtures::FirstTestClass, "open"));
    std::vector<uint8> cut = first;
    cut.resize(cut.size() / 2);
    rows.Results = { Row("", 1, ResultFixtures::SecondTestClass, second), Row("", 0, ResultFixtures::FirstTestClass, first), Row("TalkToHeadmaster", 0, ResultFixtures::FirstTestClass, first),
        Row("TalkToHeadmaster", 1, ResultFixtures::FirstTestClass, cut), Row("NoSuchGoal", 0, ResultFixtures::FirstTestClass, first) };

    std::shared_ptr<QuestStore const> const store = QuestStore::Build(rows, {}, ResultFixtures::Catalog());
    ResultList const* const onQuest = store->FindResults("Chain", "");
    ASSERT_NE(onQuest, nullptr);
    ASSERT_EQ(onQuest->size(), 2u);
    EXPECT_EQ(TextOf((*onQuest)[0]), "open");
    EXPECT_EQ(TextOf((*onQuest)[1]), "next");
    ResultList const* const onGoal = store->FindResults("Chain", "TalkToHeadmaster");
    ASSERT_NE(onGoal, nullptr);
    ASSERT_EQ(onGoal->size(), 1u) << "the cut row does not read, so only the whole one runs";
    EXPECT_EQ(store->FindResults("Chain", "NoSuchGoal"), nullptr);
    EXPECT_EQ(store->FindResults("Missing", ""), nullptr);
    ASSERT_EQ(store->GetProblems().size(), 1u);
    EXPECT_NE(store->GetProblems().front().find("goal TalkToHeadmaster of quest Chain"), std::string::npos) << store->GetProblems().front();
}

TEST(QuestResultTest, AQuestLogKeepsEachCompletionOnceUntilItIsTaken)
{
    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    ASSERT_NE(log.Add("Chain", gids, Now), nullptr);
    ASSERT_NE(log.StartGoal("Chain", "TalkToHeadmaster", gids), nullptr);
    ASSERT_TRUE(log.CompleteGoal("Chain", "TalkToHeadmaster"));
    EXPECT_FALSE(log.CompleteGoal("Chain", "TalkToHeadmaster"));
    ASSERT_TRUE(log.CompleteQuest("Chain"));

    std::vector<QuestCompletion> const taken = log.TakeCompletions();
    ASSERT_EQ(taken.size(), 2u);
    EXPECT_EQ(taken[0].Quest, "Chain");
    EXPECT_EQ(taken[0].Goal, "TalkToHeadmaster");
    EXPECT_EQ(taken[1].Quest, "Chain");
    EXPECT_TRUE(taken[1].Goal.empty()) << "the quest's own completion names no goal";
    EXPECT_TRUE(log.TakeCompletions().empty());
}
