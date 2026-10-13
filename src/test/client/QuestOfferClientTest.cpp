/*
 * Project Ambrose by Imjustchico
 * Uses the user's client type dump to check that the blobs of a quest offer, the Prep dialog MSG_ACTORDIALOG carries and the goals and rewards of MSG_QUESTOFFER, are each the bare object, its class hash first, as the client reads them, and decode back to their classes.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"
#include "QuestOffers.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    void ExpectBare(TypeCatalogPtr const& catalog, std::string const& blob, std::string_view className, uint32 mask)
    {
        ClassInfo const* const info = catalog->FindClass(className);
        ASSERT_NE(info, nullptr) << className;
        ASSERT_GE(blob.size(), 4u) << className;
        uint32 first = 0;
        std::memcpy(&first, blob.data(), sizeof(first));
        EXPECT_EQ(first, info->Hash) << className;
        SerializerOptions options;
        options.Mask = mask;
        std::vector<uint8> const bytes(blob.begin(), blob.end());
        DecodeResult const decoded = ObjectSerializer::Decode(catalog, bytes, options);
        ASSERT_TRUE(decoded.Ok()) << className << ": " << decoded.Detail;
        ASSERT_NE(decoded.Object, nullptr);
        EXPECT_EQ(decoded.Object->GetClass().Hash, info->Hash) << className;
    }
}

TEST(QuestOfferClientTest, TheOfferBlobsAreTheBareObjectsTheClientReads)
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
    quest.TitleKey = "QuestTitle_1625D7";
    quest.Level = 1;
    quest.Mainline = true;
    rows.Quests.push_back(quest);
    QuestGoalRow talk;
    talk.Quest = quest.Name;
    talk.Goal = "WC-MAIN-C01-001_Goal0";
    talk.Type = "persona";
    talk.TitleKey = "WizardQuestGoals_TalkNPC";
    talk.LocationKey = "ZoneLocName_1451497";
    talk.PersonaName = "WC-GTW-Registrar";
    rows.Goals.push_back(talk);
    rows.StartGoals.push_back(QuestStartGoalRow{ quest.Name, talk.Goal });
    rows.Dialogs.push_back(QuestDialogRow{ quest.Name, "quest", "", "Prep" });
    for (uint32 position = 0; position < 2; ++position)
    {
        QuestDialogEntryRow line;
        line.Quest = quest.Name;
        line.Owner = "quest";
        line.Tag = "Prep";
        line.Position = position;
        line.ActorTemplateId = 38168;
        line.DialogKey = position == 0 ? "WizQst1625D7_00000001" : "WizQst1625D7_00000002";
        line.Picture = "GUI/NpcPortraits/Art_Portrait_Ambrose.dds";
        rows.DialogEntries.push_back(std::move(line));
    }
    std::shared_ptr<QuestStore const> const store = QuestStore::Build(rows);
    QuestInfo const* const info = store->Find(quest.Name);
    ASSERT_NE(info, nullptr);

    QuestOffers::Encoded const encoded = QuestOffers::Encode(catalog, *info);
    ASSERT_TRUE(encoded.Ok()) << encoded.Error;
    ExpectBare(catalog, encoded.Dialog, "class ActorDialog", QuestWireEncoder::ActorDialogMask);
    ExpectBare(catalog, encoded.GoalData, "class GoalCompilation", QuestWireEncoder::GoalsAndRewardsMask);
    ExpectBare(catalog, encoded.Rewards, "class LootInfoList", QuestWireEncoder::GoalsAndRewardsMask);
}
