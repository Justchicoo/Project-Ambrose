/*
 * Project Ambrose by Imjustchico
 * Checks MSG_SENDQUEST on the wire against the QuestMessages.xml layout the fixtures copy from the r806919 client: every field is written in the XML's order and at its type, the quest's GID first and RequiresMagicWeavingSchool last, and the message reads back whole; and that completing a quest's last goal shows the quest book MSG_COMPLETEGOAL, MSG_COMPLETEQUEST and MSG_REMOVEQUEST in that order, with the GIDs the client knows and the complete texts.
 */

#include "GameMessages.h"
#include "GameTestHarness.h"
#include "GoalLogic.h"
#include "ObjectGuid.h"
#include "QuestBookMessages.h"

#include <gtest/gtest.h>

#include <string>
#include <variant>
#include <vector>

namespace
{
    std::string ReadStr(ByteBuffer& buffer)
    {
        uint16 const length = buffer.Read<uint16>();
        std::span<uint8 const> const bytes = buffer.ReadBytes(length);
        return std::string(bytes.begin(), bytes.end());
    }
}

TEST(QuestBookWireTest, SendQuestIsWrittenInTheFieldOrderAndTypesOfQuestMessagesXml)
{
    GameTesting::GameDefinitions definitions;
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    ASSERT_TRUE(catalog);
    ASSERT_TRUE(catalog->IsDeclared<GameMessages::SendQuest>());

    GameMessages::SendQuest sent;
    sent.QuestId = 0x0123456789ABCDEFull;
    sent.QuestNameId = 120047678;
    sent.QuestType = 3;
    sent.QuestLevel = -2;
    sent.QuestTitle = "QuestTitle_00001718";
    sent.QuestInfo = "QuestInfo";
    sent.New = 1;
    sent.QuestMadlibs = "M";
    sent.GoalData = "";
    sent.Rewards = "RW";
    sent.ClientTags = "CT";
    sent.AssociatedWorlds = "AW";
    sent.NoQuestHelper = 2;
    sent.Mainline = 3;
    sent.ReadyToTurnIn = 4;
    sent.SkipQhAutoSelect = 5;
    sent.PetOnlyQuest = 6;
    sent.ActivityType = 7;
    sent.RequiresMagicWeavingSchool = "Fire";
    ByteBuffer buffer;
    catalog->Encode(sent, buffer);

    ByteBuffer wire(buffer.GetData());
    EXPECT_EQ(wire.Read<uint64>(), sent.QuestId);
    EXPECT_EQ(wire.Read<uint32>(), 120047678u);
    EXPECT_EQ(wire.Read<uint32>(), 3u);
    EXPECT_EQ(wire.Read<int32>(), -2);
    EXPECT_EQ(ReadStr(wire), "QuestTitle_00001718");
    EXPECT_EQ(ReadStr(wire), "QuestInfo");
    EXPECT_EQ(wire.Read<uint8>(), 1);
    EXPECT_EQ(ReadStr(wire), "M");
    EXPECT_EQ(ReadStr(wire), "");
    EXPECT_EQ(ReadStr(wire), "RW");
    EXPECT_EQ(ReadStr(wire), "CT");
    EXPECT_EQ(ReadStr(wire), "AW");
    for (uint8 flag = 2; flag <= 7; ++flag)
        EXPECT_EQ(wire.Read<uint8>(), flag);
    EXPECT_EQ(ReadStr(wire), "Fire");
    EXPECT_EQ(wire.GetRemaining(), 0u);

    ByteBuffer again(buffer.GetData());
    GameMessages::SendQuest read;
    ASSERT_TRUE(catalog->Decode(again, read));
    EXPECT_EQ(read.QuestId, sent.QuestId);
    EXPECT_EQ(read.AssociatedWorlds, "AW");
    EXPECT_EQ(read.RequiresMagicWeavingSchool, "Fire");
}

TEST(QuestBookWireTest, TheLastGoalShowsItsCompletionThenTheQuestsThenTakesTheQuestOut)
{
    GameTesting::GameDefinitions definitions;
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    ASSERT_TRUE(catalog);
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::CompleteGoal>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::CompleteQuest>());

    QuestRows rows;
    rows.Objects = { { 1, "Player" } };
    QuestTemplateRow quest;
    quest.Name = "Errand";
    quest.CompleteKey = "QuestComplete_Errand";
    rows.Quests.push_back(quest);
    QuestGoalRow goal;
    goal.Quest = "Errand";
    goal.Goal = "Talk";
    goal.Type = "persona";
    goal.CompleteKey = "GoalComplete_Talk";
    rows.Goals.push_back(goal);
    rows.StartGoals.push_back(QuestStartGoalRow{ "Errand", "Talk" });
    std::shared_ptr<QuestStore const> const quests = QuestStore::Build(rows);
    QuestInfo const& info = *quests->Find("Errand");

    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    auto const anyone = [](QuestGoal const&) { return true; };
    ASSERT_TRUE(GoalLogic::Start(log, info, gids, 1791650000, anyone));
    uint64 const questGid = log.Find("Errand")->Gid;
    uint64 const goalGid = log.Find("Errand")->FindGoal("Talk")->Gid;
    log.TakeChanges();
    ASSERT_TRUE(log.CompleteGoal("Errand", "Talk"));
    EXPECT_TRUE(GoalLogic::Advance(log, info, "Talk", gids, anyone).CompleteQuest);

    QuestBookUpdate const update = QuestBookMessages::Changes(nullptr, *quests, log, log.TakeChanges());
    EXPECT_TRUE(update.Problems.empty());
    ASSERT_EQ(update.Messages.size(), 3u);
    GameMessages::CompleteGoal const* const completedGoal = std::get_if<GameMessages::CompleteGoal>(&update.Messages[0]);
    ASSERT_NE(completedGoal, nullptr);
    EXPECT_EQ(completedGoal->QuestId, questGid);
    EXPECT_EQ(completedGoal->GoalId, goalGid);
    EXPECT_EQ(completedGoal->CompleteText, "GoalComplete_Talk");
    GameMessages::CompleteQuest const* const completedQuest = std::get_if<GameMessages::CompleteQuest>(&update.Messages[1]);
    ASSERT_NE(completedQuest, nullptr);
    EXPECT_EQ(completedQuest->QuestId, questGid);
    EXPECT_EQ(completedQuest->CompleteText, "QuestComplete_Errand");
    GameMessages::RemoveQuest const* const removed = std::get_if<GameMessages::RemoveQuest>(&update.Messages[2]);
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed->QuestId, questGid);

    ByteBuffer buffer;
    catalog->Encode(*completedGoal, buffer);
    ByteBuffer wire(buffer.GetData());
    EXPECT_EQ(wire.Read<uint64>(), questGid);
    EXPECT_EQ(wire.Read<uint64>(), goalGid);
    EXPECT_EQ(ReadStr(wire), "GoalComplete_Talk");
    EXPECT_EQ(wire.GetRemaining(), 0u);
}
