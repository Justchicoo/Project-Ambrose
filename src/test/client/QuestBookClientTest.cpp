/*
 * Project Ambrose by Imjustchico
 * Uses the user's client type dump to check the quest book a wizard is shown for a quest it accepted: MSG_SENDQUEST names it by its GID, title and level, new, with bare madlib, reward and associated-world blobs naming the world its goal leads to, and MSG_SENDGOAL names the started goal by its GID with its title, location, destination, persona and the patron icon of the quest's first Prep line, and a bare madlib blob.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "ObjectGuid.h"
#include "ObjectSerializer.h"
#include "QuestBook.h"
#include "QuestBookMessages.h"
#include "QuestLog.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    PropertyObjectPtr ExpectBare(TypeCatalogPtr const& catalog, std::string const& blob, std::string_view className)
    {
        ClassInfo const* const info = catalog->FindClass(className);
        EXPECT_NE(info, nullptr) << className;
        if (!info || blob.size() < 4)
        {
            ADD_FAILURE() << className << " blob is " << blob.size() << " bytes";
            return nullptr;
        }
        uint32 first = 0;
        std::memcpy(&first, blob.data(), sizeof(first));
        EXPECT_EQ(first, info->Hash) << className;
        SerializerOptions options;
        options.Mask = QuestWireEncoder::GoalsAndRewardsMask;
        std::vector<uint8> const bytes(blob.begin(), blob.end());
        DecodeResult decoded = ObjectSerializer::Decode(catalog, bytes, options);
        EXPECT_TRUE(decoded.Ok()) << className << ": " << decoded.Detail;
        return std::move(decoded.Object);
    }
}

TEST(QuestBookClientTest, AnAcceptedQuestAndItsStartGoalAreShownAsTheClientReadsThem)
{
    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to a type dump from your own client to run this test";

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*path)))
        << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_NE(catalog, nullptr);

    QuestRows rows;
    rows.Objects = { { 1, "Player" }, { 38168, "WC-HUB-NPC01" }, { 39088, "WC-GTW-Registrar" } };
    QuestTemplateRow quest;
    quest.Name = "WC-MAIN-C01-001";
    quest.NameId = 120047678;
    quest.TitleKey = "QuestTitle_00001718";
    quest.Level = 1;
    quest.Mainline = true;
    rows.Quests.push_back(quest);
    QuestGoalRow talk;
    talk.Quest = quest.Name;
    talk.Goal = "WC-MAIN-C01-001_Goal0";
    talk.Type = "persona";
    talk.TitleKey = "WizardQuestGoals_TalkNPC";
    talk.LocationKey = "ZoneLocName_1451497";
    talk.DestinationZone = "WizardCity/WC_Ravenwood";
    talk.PersonaName = "WC-GTW-Registrar";
    rows.Goals.push_back(talk);
    rows.StartGoals.push_back(QuestStartGoalRow{ quest.Name, talk.Goal });
    rows.Dialogs.push_back(QuestDialogRow{ quest.Name, "quest", "", "Prep" });
    QuestDialogEntryRow line;
    line.Quest = quest.Name;
    line.Owner = "quest";
    line.Tag = "Prep";
    line.ActorTemplateId = 38168;
    line.DialogKey = "WizQst1625D7_00000001";
    line.Picture = "GUI/NpcPortraits/Art_Portrait_Ambrose.dds";
    rows.DialogEntries.push_back(std::move(line));
    std::shared_ptr<QuestStore const> const store = QuestStore::Build(rows);
    QuestInfo const* const info = store->Find(quest.Name);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(QuestBook::AssociatedWorlds(*info), std::vector<std::string>{ "WizardCity" });

    GuidGenerator gids(ObjectGuid::QuestBase);
    QuestLog log;
    QuestLogQuest const* const added = log.Add(quest.Name, gids, 1791650000);
    ASSERT_NE(added, nullptr);
    QuestLogGoal const* const started = log.StartGoal(quest.Name, talk.Goal, gids);
    ASSERT_NE(started, nullptr);
    QuestLogQuest const& held = *log.Find(quest.Name);

    std::string error;
    std::optional<GameMessages::SendQuest> const sent = QuestBookMessages::Quest(catalog, *info, held, true, error);
    ASSERT_TRUE(sent.has_value()) << error;
    EXPECT_EQ(sent->QuestId, held.Gid);
    EXPECT_EQ(sent->QuestNameId, 120047678u);
    EXPECT_EQ(sent->QuestTitle, "QuestTitle_00001718");
    EXPECT_EQ(sent->QuestLevel, 1);
    EXPECT_EQ(sent->New, 1);
    EXPECT_EQ(sent->Mainline, 1);
    EXPECT_TRUE(sent->GoalData.empty());
    ExpectBare(catalog, sent->QuestMadlibs, "class MadlibBlock");
    ExpectBare(catalog, sent->Rewards, "class LootInfoList");
    PropertyObjectPtr const worlds = ExpectBare(catalog, sent->AssociatedWorlds, "class AssociatedWorldsList");
    ASSERT_NE(worlds, nullptr);

    std::optional<GameMessages::SendGoal> const goal = QuestBookMessages::Goal(catalog, *info, held, *started, QuestBook::SendStart, error);
    ASSERT_TRUE(goal.has_value()) << error;
    EXPECT_EQ(goal->QuestId, held.Gid);
    EXPECT_EQ(goal->GoalId, started->Gid);
    EXPECT_EQ(goal->GoalTitle, "WizardQuestGoals_TalkNPC");
    EXPECT_EQ(goal->GoalLocation, "ZoneLocName_1451497");
    EXPECT_EQ(goal->GoalDestinationZone, "WizardCity/WC_Ravenwood");
    EXPECT_EQ(goal->PersonaName, "WC-GTW-Registrar");
    EXPECT_EQ(goal->GoalType, static_cast<uint8>(Quests::GoalType::Persona));
    EXPECT_EQ(goal->GoalStatus, QuestBook::GoalActive);
    EXPECT_EQ(goal->SendType, QuestBook::SendStart);
    EXPECT_EQ(goal->UseTally, 0);
    EXPECT_EQ(goal->PatronIcon, "GUI/NpcPortraits/Art_Portrait_Ambrose.dds");
    EXPECT_TRUE(goal->ClientTags.empty());
    ExpectBare(catalog, goal->GoalMadlibs, "class MadlibBlock");

    QuestLogGoal const missing{ started->Gid, "NotAGoal", false, 0 };
    error.clear();
    EXPECT_FALSE(QuestBookMessages::Goal(catalog, *info, held, missing, QuestBook::SendStart, error).has_value());
    EXPECT_FALSE(error.empty());
}
