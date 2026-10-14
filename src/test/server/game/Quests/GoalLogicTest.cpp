/*
 * Project Ambrose by Imjustchico
 * Tests goal logic on quests built from rows: a linear chain starts its next goal and completes the quest after its last; AND(A, B) starts C only once both are complete; OR(A, B, C) with a required count of 2 fires on the second; an entry that completes the quest wins over one that adds a goal; a goal whose school requirement the wizard fails is never started, at accept or by logic; and completing the last goal hands the quest book its goal completion, the quest's completion and its removal, in that order.
 */

#include "GoalLogic.h"
#include "ObjectGuid.h"
#include "RequirementMgr.h"
#include "WizardRequirementContext.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr uint64 Now = 1791650000;

    struct Logic
    {
        std::vector<std::string> All;
        std::vector<std::string> Any;
        std::vector<std::string> Adds;
        bool Completes = false;
        uint32 OrCount = 1;
    };

    void AddLogic(QuestRows& rows, std::string const& quest, uint32 position, Logic const& logic)
    {
        std::vector<std::string> owners = logic.All;
        owners.insert(owners.end(), logic.Any.begin(), logic.Any.end());
        for (std::string const& owner : owners)
        {
            rows.Logic.push_back(QuestGoalLogicRow{ quest, owner, position, logic.Completes, logic.OrCount });
            auto const members = [&](std::string const& kind, std::vector<std::string> const& goals)
            {
                for (std::size_t index = 0; index < goals.size(); ++index)
                    rows.LogicMembers.push_back(QuestGoalLogicMemberRow{ quest, owner, position, kind, static_cast<uint32>(index), goals[index] });
            };
            members("AND", logic.All);
            members("OR", logic.Any);
            members("ADD", logic.Adds);
        }
    }

    std::shared_ptr<QuestStore const> Store(std::vector<std::string> const& goals, std::vector<std::string> const& start, std::vector<Logic> const& logic)
    {
        QuestRows rows;
        rows.Objects = { { 1, "Player" } };
        QuestTemplateRow quest;
        quest.Name = "Chain";
        quest.TitleKey = "QuestTitle_Chain";
        quest.CompleteKey = "QuestComplete_Chain";
        quest.Level = 1;
        rows.Quests.push_back(quest);
        for (std::string const& name : goals)
        {
            QuestGoalRow goal;
            goal.Quest = "Chain";
            goal.Goal = name;
            goal.Type = "persona";
            goal.CompleteKey = "GoalComplete_" + name;
            rows.Goals.push_back(goal);
        }
        for (std::string const& name : start)
            rows.StartGoals.push_back(QuestStartGoalRow{ "Chain", name });
        for (std::size_t position = 0; position < logic.size(); ++position)
            AddLogic(rows, "Chain", static_cast<uint32>(position), logic[position]);
        return QuestStore::Build(rows);
    }

    bool Anyone(QuestGoal const&)
    {
        return true;
    }

    struct Wizard
    {
        GuidGenerator Gids{ ObjectGuid::QuestBase };
        QuestLog Log;

        void Take(QuestInfo const& quest)
        {
            ASSERT_TRUE(GoalLogic::Start(Log, quest, Gids, Now, Anyone));
        }

        GoalLogicOutcome Complete(QuestInfo const& quest, std::string const& goal)
        {
            EXPECT_TRUE(Log.CompleteGoal(quest.Row.Name, goal)) << goal;
            return GoalLogic::Advance(Log, quest, goal, Gids, Anyone);
        }

        bool IsActive(std::string const& goal) const
        {
            return Log.IsGoalActive("Chain", goal);
        }
    };
}

TEST(GoalLogicTest, ALinearChainStartsItsNextGoalAndCompletesTheQuestAfterItsLast)
{
    std::shared_ptr<QuestStore const> const quests = Store({ "A", "B" }, { "A" }, { Logic{ { "A" }, {}, { "B" } } });
    QuestInfo const& quest = *quests->Find("Chain");
    Wizard wizard;
    wizard.Take(quest);
    EXPECT_TRUE(wizard.IsActive("A"));
    EXPECT_FALSE(wizard.IsActive("B"));

    GoalLogicOutcome const first = wizard.Complete(quest, "A");
    EXPECT_EQ(first.GoalsToStart, std::vector<std::string>{ "B" });
    EXPECT_FALSE(first.CompleteQuest);
    EXPECT_TRUE(wizard.IsActive("B"));

    GoalLogicOutcome const last = wizard.Complete(quest, "B");
    EXPECT_TRUE(last.CompleteQuest);
    EXPECT_FALSE(wizard.Log.HasQuest("Chain"));
    EXPECT_TRUE(wizard.Log.HasCompletedQuest("Chain"));
}

TEST(GoalLogicTest, AndWaitsForEveryGoalAndOrForItsRequiredCount)
{
    std::shared_ptr<QuestStore const> const both = Store({ "A", "B", "C" }, { "A", "B" }, { Logic{ { "A", "B" }, {}, { "C" } } });
    QuestInfo const& parallel = *both->Find("Chain");
    Wizard first;
    first.Take(parallel);
    GoalLogicOutcome const one = first.Complete(parallel, "A");
    EXPECT_TRUE(one.GoalsToStart.empty());
    EXPECT_FALSE(one.CompleteQuest) << "B is still active";
    EXPECT_FALSE(first.IsActive("C"));
    EXPECT_EQ(first.Complete(parallel, "B").GoalsToStart, std::vector<std::string>{ "C" });
    EXPECT_TRUE(first.IsActive("C"));

    std::shared_ptr<QuestStore const> const any = Store({ "A", "B", "C", "D" }, { "A", "B", "C" }, { Logic{ {}, { "A", "B", "C" }, { "D" }, false, 2 } });
    QuestInfo const& choice = *any->Find("Chain");
    EXPECT_EQ(choice.Goals.at("A").Logic.front().RequiredOrCount, 2u);
    Wizard second;
    second.Take(choice);
    EXPECT_TRUE(second.Complete(choice, "B").GoalsToStart.empty());
    EXPECT_FALSE(second.IsActive("D"));
    EXPECT_EQ(second.Complete(choice, "C").GoalsToStart, std::vector<std::string>{ "D" });
    EXPECT_TRUE(second.IsActive("D"));
    EXPECT_TRUE(second.Complete(choice, "A").GoalsToStart.empty()) << "D is already held, so it is not started again";
}

TEST(GoalLogicTest, AnEntryThatCompletesTheQuestWinsOverOneThatAddsAGoal)
{
    std::shared_ptr<QuestStore const> const quests = Store({ "A", "B", "C" }, { "A", "B" }, { Logic{ { "A" }, {}, { "C" } }, Logic{ { "A" }, {}, {}, true } });
    QuestInfo const& quest = *quests->Find("Chain");
    Wizard wizard;
    wizard.Take(quest);
    GoalLogicOutcome const outcome = wizard.Complete(quest, "A");
    EXPECT_TRUE(outcome.CompleteQuest);
    EXPECT_TRUE(outcome.GoalsToStart.empty());
    EXPECT_FALSE(wizard.Log.HasQuest("Chain")) << "the quest completes though B was still active";
}

TEST(GoalLogicTest, AGoalWhoseSchoolRequirementTheWizardFailsIsNeverStarted)
{
    std::shared_ptr<QuestStore const> const quests = Store({ "A", "FireOnly", "IceOnly", "Fire2" }, { "A", "IceOnly" }, { Logic{ { "A" }, {}, { "FireOnly", "IceOnly" } } });
    QuestInfo quest = *quests->Find("Chain");
    auto const school = [](std::string const& name)
    {
        RequirementRow focus;
        focus.ListId = "root";
        focus.Type = "ReqSchoolOfFocus";
        focus.MagicSchool = name;
        return std::make_shared<RequirementTree const>(RequirementTree{ RequirementListRow{ "root", "AND", false, std::nullopt }, { focus }, {} });
    };
    quest.Goals.at("FireOnly").Requirements = school("Fire");
    quest.Goals.at("IceOnly").Requirements = school("Ice");

    Wizard wizard;
    GoalLogic::CanStart const fire = [&wizard](QuestGoal const& goal)
    {
        return !goal.Requirements || sRequirementMgr.EvaluateTree(*goal.Requirements, WizardRequirementContext(wizard.Log, "Fire", 5, "WizardCity/WC_Hub"));
    };
    ASSERT_TRUE(GoalLogic::Start(wizard.Log, quest, wizard.Gids, Now, fire));
    EXPECT_TRUE(wizard.IsActive("A"));
    EXPECT_EQ(wizard.Log.Find("Chain")->FindGoal("IceOnly"), nullptr) << "an Ice start goal is not given to a Fire wizard";

    ASSERT_TRUE(wizard.Log.CompleteGoal("Chain", "A"));
    GoalLogicOutcome const outcome = GoalLogic::Advance(wizard.Log, quest, "A", wizard.Gids, fire);
    EXPECT_EQ(outcome.GoalsToStart, std::vector<std::string>{ "FireOnly" });
    EXPECT_TRUE(wizard.IsActive("FireOnly"));
    EXPECT_EQ(wizard.Log.Find("Chain")->FindGoal("IceOnly"), nullptr);
}

TEST(GoalLogicTest, TheLastGoalHandsTheQuestBookItsCompletionThenTheQuestsThenItsRemoval)
{
    std::shared_ptr<QuestStore const> const quests = Store({ "A" }, { "A" }, {});
    QuestInfo const& quest = *quests->Find("Chain");
    Wizard wizard;
    wizard.Take(quest);
    wizard.Log.TakeChanges();
    EXPECT_TRUE(wizard.Complete(quest, "A").CompleteQuest);
    std::vector<QuestLogChange> const changes = wizard.Log.TakeChanges();
    ASSERT_EQ(changes.size(), 3u);
    EXPECT_EQ(changes[0].Kind, QuestLogChangeKind::GoalCompleted);
    EXPECT_EQ(changes[1].Kind, QuestLogChangeKind::QuestCompleted);
    EXPECT_EQ(changes[2].Kind, QuestLogChangeKind::QuestRemoved);
}
