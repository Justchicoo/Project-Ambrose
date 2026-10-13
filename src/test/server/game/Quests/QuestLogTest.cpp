/*
 * Project Ambrose by Imjustchico
 * Tests a wizard's quest log without a database: a quest taken, its goal started and counted to 2 of 5, turned into rows and read back keeps its GIDs and count; completing a quest takes it out of the log and leaves 'Complete' = 1 in its registry, so the wizard has completed it; and on load a quest whose template is gone, with its goals, and a goal its quest no longer has are dropped with a warning each, while a quest store that is not there drops nothing; a completed goal hands out its GoalComplete event once, and a wizard's Dynamods mark its progress changed and come back whole from its rows.
 */

#include "ObjectGuid.h"
#include "QuestLog.h"
#include "QuestStore.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr uint64 Now = 1791650000;

    std::shared_ptr<QuestStore const> Templates()
    {
        QuestRows rows;
        rows.Objects = { { 1, "Player" }, { 38232, "Headmaster" } };
        QuestTemplateRow quest;
        quest.Name = "KeptQuest";
        quest.TitleKey = "QuestTitle_KeptQuest";
        quest.Level = 1;
        rows.Quests.push_back(quest);
        QuestGoalRow talk;
        talk.Quest = "KeptQuest";
        talk.Goal = "TalkToHeadmaster";
        talk.Type = "persona";
        talk.PersonaName = "Headmaster";
        rows.Goals.push_back(talk);
        QuestGoalRow defeat;
        defeat.Quest = "KeptQuest";
        defeat.Goal = "DefeatUndead";
        defeat.Type = "bounty";
        defeat.TallyCount = 5;
        rows.Goals.push_back(defeat);
        rows.StartGoals.push_back(QuestStartGoalRow{ "KeptQuest", "DefeatUndead" });
        return QuestStore::Build(rows);
    }
}

TEST(QuestLogTest, AQuestCountedToTwoOfFiveKeepsItsGidsAndCountThroughItsRows)
{
    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    QuestLogQuest const* const quest = log.Add("KeptQuest", gids, Now);
    ASSERT_NE(quest, nullptr);
    uint64 const questGid = quest->Gid;
    EXPECT_TRUE(ObjectGuid::IsQuest(questGid));
    EXPECT_EQ(log.Add("KeptQuest", gids, Now), nullptr) << "a quest the wizard holds is not taken twice";
    QuestLogGoal const* const goal = log.StartGoal("KeptQuest", "DefeatUndead", gids);
    ASSERT_NE(goal, nullptr);
    uint64 const goalGid = goal->Gid;
    EXPECT_NE(goalGid, questGid);
    EXPECT_EQ(log.IncrementGoal("KeptQuest", "DefeatUndead"), 1u);
    EXPECT_EQ(log.IncrementGoal("KeptQuest", "DefeatUndead"), 2u);
    EXPECT_TRUE(log.IsGoalActive("KeptQuest", "DefeatUndead"));
    EXPECT_TRUE(log.IsDirty());

    CharacterQuests const stored = log.ToStored(7);
    EXPECT_EQ(stored.Revision, 7u);
    ASSERT_EQ(stored.Quests.size(), 1u);
    EXPECT_EQ(stored.Quests[0], (CharacterQuestRow{ questGid, "KeptQuest", Now }));
    ASSERT_EQ(stored.Goals.size(), 1u);
    EXPECT_EQ(stored.Goals[0], (CharacterQuestGoalRow{ goalGid, questGid, "DefeatUndead", false, 2 }));

    std::vector<std::string> warnings;
    std::shared_ptr<QuestStore const> const templates = Templates();
    QuestLog const reloaded = QuestLog::FromStored(stored, templates.get(), warnings);
    EXPECT_TRUE(warnings.empty());
    EXPECT_FALSE(reloaded.IsDirty());
    QuestLogQuest const* const back = reloaded.Find("KeptQuest");
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(back->Gid, questGid);
    ASSERT_NE(back->FindGoal("DefeatUndead"), nullptr);
    EXPECT_EQ(back->FindGoal("DefeatUndead")->Gid, goalGid);
    EXPECT_EQ(back->FindGoal("DefeatUndead")->Count, 2u);
}

TEST(QuestLogTest, CompletingAQuestRemovesItAndSetsItsCompleteEntry)
{
    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    ASSERT_NE(log.Add("KeptQuest", gids, Now), nullptr);
    ASSERT_NE(log.StartGoal("KeptQuest", "TalkToHeadmaster", gids), nullptr);
    EXPECT_TRUE(log.CompleteGoal("KeptQuest", "TalkToHeadmaster"));
    EXPECT_TRUE(log.IsGoalCompleted("KeptQuest", "TalkToHeadmaster"));
    EXPECT_FALSE(log.HasCompletedQuest("KeptQuest"));
    log.MarkSaved();

    std::string const name = log.GetQuests().front().Name;
    EXPECT_TRUE(log.CompleteQuest(log.GetQuests().front().Name)) << "the name may be the quest's own, which completing it frees";
    EXPECT_FALSE(log.HasQuest(name));
    EXPECT_EQ(log.GetQuestRegistry(name, QuestLog::CompleteEntry), 1.0);
    EXPECT_TRUE(log.HasCompletedQuest(name));
    EXPECT_TRUE(log.IsDirty());
    EXPECT_FALSE(log.CompleteQuest(name)) << "a quest no longer held cannot be completed again";

    CharacterQuests const stored = log.ToStored(2);
    EXPECT_TRUE(stored.Quests.empty());
    EXPECT_TRUE(stored.Goals.empty());
    EXPECT_EQ(stored.QuestRegistry, (std::vector<CharacterRegistryRow>{ { "KeptQuest", "Complete", 1.0 } }));
}

TEST(QuestLogTest, StateRevisionAdvancesForMutationsAndNotForNoOps)
{
    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    EXPECT_EQ(log.GetStateRevision(), 0u);
    ASSERT_NE(log.Add("KeptQuest", gids, Now), nullptr);
    uint64 revision = log.GetStateRevision();
    EXPECT_GT(revision, 0u);
    EXPECT_EQ(log.Add("KeptQuest", gids, Now), nullptr);
    EXPECT_EQ(log.GetStateRevision(), revision);

    ASSERT_NE(log.StartGoal("KeptQuest", "DefeatUndead", gids), nullptr);
    EXPECT_GT(log.GetStateRevision(), revision);
    revision = log.GetStateRevision();
    ASSERT_TRUE(log.IncrementGoal("KeptQuest", "DefeatUndead"));
    EXPECT_GT(log.GetStateRevision(), revision);
    revision = log.GetStateRevision();
    ASSERT_TRUE(log.CompleteGoal("KeptQuest", "DefeatUndead"));
    EXPECT_GT(log.GetStateRevision(), revision);
    revision = log.GetStateRevision();
    log.SetQuestRegistry("KeptQuest", "Seen", 1.0);
    EXPECT_GT(log.GetStateRevision(), revision);
    revision = log.GetStateRevision();
    log.SetRegistry("Visited", 1.0);
    EXPECT_GT(log.GetStateRevision(), revision);
    revision = log.GetStateRevision();
    log.SetHidden("KeptQuest", true);
    EXPECT_GT(log.GetStateRevision(), revision);
    revision = log.GetStateRevision();
    EXPECT_TRUE(log.CompleteQuest("KeptQuest"));
    EXPECT_GT(log.GetStateRevision(), revision);
}

TEST(QuestLogTest, AnOrphanQuestAndAnOrphanGoalArePrunedOnLoadWithAWarningEach)
{
    CharacterQuests stored;
    stored.Revision = 3;
    stored.Quests = { { ObjectGuid::QuestBase + 1, "KeptQuest", Now }, { ObjectGuid::QuestBase + 2, "RemovedQuest", Now } };
    stored.Goals = { { ObjectGuid::QuestBase + 3, ObjectGuid::QuestBase + 1, "DefeatUndead", false, 2 }, { ObjectGuid::QuestBase + 4, ObjectGuid::QuestBase + 1, "RemovedGoal", false, 0 },
        { ObjectGuid::QuestBase + 5, ObjectGuid::QuestBase + 2, "Whatever", true, 1 } };
    stored.QuestRegistry = { { "RemovedQuest", "Complete", 1.0 } };
    stored.Registry = { { {}, "Visited", 4.0 } };
    stored.Hidden = { "KeptQuest" };

    std::vector<std::string> warnings;
    std::shared_ptr<QuestStore const> const templates = Templates();
    QuestLog const log = QuestLog::FromStored(stored, templates.get(), warnings);
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_NE(warnings[0].find("RemovedGoal"), std::string::npos) << warnings[0];
    EXPECT_NE(warnings[1].find("RemovedQuest"), std::string::npos) << warnings[1];
    EXPECT_TRUE(log.IsDirty()) << "the pruned log is saved again without the orphans";
    ASSERT_EQ(log.GetQuests().size(), 1u);
    EXPECT_EQ(log.GetQuests()[0].Name, "KeptQuest");
    ASSERT_EQ(log.GetQuests()[0].Goals.size(), 1u);
    EXPECT_EQ(log.GetQuests()[0].Goals[0].Name, "DefeatUndead");
    EXPECT_TRUE(log.HasCompletedQuest("RemovedQuest")) << "a quest's completion outlives its template";
    EXPECT_EQ(log.GetRegistry("Visited"), 4.0);
    EXPECT_TRUE(log.IsHidden("KeptQuest"));

    std::vector<std::string> none;
    QuestLog const unchecked = QuestLog::FromStored(stored, nullptr, none);
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(unchecked.GetQuests().size(), 2u) << "with no quest store loaded, nothing can be called an orphan";
}

TEST(QuestLogTest, ACompletedGoalPublishesItsEventOnceAndAWizardsDynamodsComeBackFromItsRows)
{
    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    ASSERT_NE(log.Add("WC-MAIN-C01-003", gids, Now), nullptr);
    ASSERT_NE(log.StartGoal("WC-MAIN-C01-003", "WC-MAIN-C01-003_Goal0", gids), nullptr);
    EXPECT_TRUE(log.TakeEvents().empty());
    EXPECT_TRUE(log.CompleteGoal("WC-MAIN-C01-003", "WC-MAIN-C01-003_Goal0"));
    EXPECT_FALSE(log.CompleteGoal("WC-MAIN-C01-003", "WC-MAIN-C01-003_Goal0"));
    EXPECT_EQ(log.TakeEvents(), (std::vector<std::string>{ "GoalComplete_WC-MAIN-C01-003_WC-MAIN-C01-003_Goal0" })) << "the name the zone's OpenGateToUW trigger fires on";
    EXPECT_TRUE(log.TakeEvents().empty()) << "an event is handed out once";

    log.MarkSaved();
    uint64 const before = log.GetChangeCount();
    EXPECT_TRUE(log.ApplyDynaMod("WizardCity/WC_Hub", "WC_GateCommons_ToUnicornWay", "IdleOpen", false, 9).Changed());
    EXPECT_TRUE(log.IsDirty());
    EXPECT_GT(log.GetChangeCount(), before) << "a new mod moves the wizard's progress, so what it sees is asked again";
    uint64 const after = log.GetChangeCount();
    EXPECT_FALSE(log.ApplyDynaMod("WizardCity/WC_Hub", "WC_GateCommons_ToUnicornWay", "IdleOpen", false, 9).Changed());
    EXPECT_EQ(log.GetChangeCount(), after);
    log.ApplyDynaMod("WizardCity/WC_Hub", "PetParkCollision", "", true, 9);

    CharacterQuests const stored = log.ToStored(3);
    ASSERT_EQ(stored.DynaMods.size(), 2u);
    std::vector<std::string> warnings;
    QuestLog const loaded = QuestLog::FromStored(stored, nullptr, warnings);
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(loaded.GetDynaMods().All(), log.GetDynaMods().All());
    EXPECT_EQ(loaded.GetDynaMods().Visibility("WizardCity/WC_Hub", "PetParkCollision"), false);
}
