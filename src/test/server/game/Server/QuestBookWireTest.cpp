/*
 * Project Ambrose by Imjustchico
 * Checks MSG_SENDQUEST on the wire against the QuestMessages.xml layout the fixtures copy from the r806919 client: every field is written in the XML's order and at its type, the quest's GID first and RequiresMagicWeavingSchool last, and the message reads back whole.
 */

#include "GameMessages.h"
#include "GameTestHarness.h"

#include <gtest/gtest.h>

#include <string>

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
