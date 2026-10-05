/*
 * Project Ambrose by Imjustchico
 * Uses the user's client type dump to check quest blob class hashes, madlib arguments, SerializerBinary headers and ObjectProperty round trips for the 7.02 wire models.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"
#include "QuestWireEncoder.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr uint32 ExpectedActorDialogHash = 0x39e3afabu;

    void ExpectRoundTrip(TypeCatalogPtr const& catalog, PropertyObject const& object, uint32 mask)
    {
        QuestWireEncoder::BlobEncodeResult const encoded = QuestWireEncoder::Encode(object, mask);
        ASSERT_TRUE(encoded.Ok()) << encoded.Error;
        ASSERT_GE(encoded.Bytes.size(), BlobEnvelope::HeaderSize);

        BlobEnvelope::UnwrapResult const unwrapped = BlobEnvelope::Unwrap(encoded.Bytes, std::size_t{ 16 } << 20);
        ASSERT_TRUE(unwrapped.Succeeded()) << BlobEnvelope::GetStatusName(unwrapped.Code);
        EXPECT_EQ(unwrapped.Packed, BlobEnvelope::Packing::Store);

        SerializerOptions options;
        options.Mask = mask;
        DecodeResult decoded = ObjectSerializer::Decode(catalog, unwrapped.Data, options);
        ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
        ASSERT_NE(decoded.Object, nullptr);
        EXPECT_EQ(decoded.Object->GetClass().Hash, object.GetClass().Hash);
        EXPECT_EQ(*decoded.Object, object);
    }

    PropertyObjectPtr Build(QuestWireEncoder::ObjectBuildResult result)
    {
        EXPECT_TRUE(result.Ok()) << result.Error;
        return std::move(result.Object);
    }
}

TEST(QuestWireEncoderTest, ClientFacingQuestBlobsRoundTripWithExpectedClassesAndEnvelope)
{
    std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    if (!client || client->empty())
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to your own client install to run this test";

    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to a type dump from your own client to run this test";

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*path)))
        << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_NE(catalog, nullptr);

    ClassInfo const* const actorDialog = catalog->FindClass("class ActorDialog");
    ASSERT_NE(actorDialog, nullptr);
    EXPECT_EQ(actorDialog->Hash, ExpectedActorDialogHash);
    for (auto const& [name, hash] : { std::pair<std::string_view, uint32>{ "class ServiceMementoBase", 1846355199u },
             { "class PrepEntry", 1653005502u },
             { "class GoalEntryFull", 1689163694u },
             { "class MadlibBlock", 1165770906u } })
    {
        ClassInfo const* const type = catalog->FindClass(name);
        ASSERT_NE(type, nullptr) << name;
        EXPECT_EQ(type->Hash, hash) << name;
    }

    Quests::GoalTemplate bounty;
    bounty.Name = "bounty";
    bounty.LocationName = "Ravenwood";
    Quests::TallyCounterTemplate tally;
    tally.Descriptor = "BOUNTY_TOTAL";
    bounty.TallyCounter = tally;
    QuestMadlibs::Block const bountyMadlib = QuestMadlibs::BuildGoal(bounty, 3, 5);
    ASSERT_EQ(bountyMadlib.BlockToken, "GOAL");
    ASSERT_EQ(bountyMadlib.Arguments.size(), 6u);
    EXPECT_EQ(bountyMadlib.Arguments[0].Token, "NAME");
    EXPECT_EQ(bountyMadlib.Arguments[2].Token, "TALLYTEXT");
    EXPECT_EQ(std::get<std::string>(bountyMadlib.Arguments[2].Value), "BOUNTY_TOTAL");
    EXPECT_EQ(bountyMadlib.Arguments[4].Token, "COUNT");
    EXPECT_EQ(std::get<int32>(bountyMadlib.Arguments[4].Value), 3);
    EXPECT_EQ(bountyMadlib.Arguments[5].Token, "TOTAL");
    EXPECT_EQ(std::get<int32>(bountyMadlib.Arguments[5].Value), 5);

    Quests::QuestTemplate quest;
    quest.Name = "quest";
    quest.Level = 4;
    QuestMadlibs::Block const questMadlib = QuestMadlibs::BuildQuest(quest);
    EXPECT_EQ(questMadlib.BlockToken, "QUEST");
    EXPECT_EQ(questMadlib.Arguments.size(), 2u);
    EXPECT_EQ(std::get<int32>(questMadlib.Arguments[1].Value), 4);
    QuestMadlibs::NpcFields npc;
    npc.Name = "NPC";
    npc.FirstName = "First";
    npc.LastName = "Last";
    QuestMadlibs::Block const npcMadlib = QuestMadlibs::BuildNpc(npc);
    EXPECT_EQ(npcMadlib.BlockToken, "NPC");
    EXPECT_EQ(npcMadlib.Arguments.size(), 6u);

    PropertyObjectPtr madlib = Build(QuestWireEncoder::BuildMadlibBlock(catalog, bountyMadlib));
    ASSERT_NE(madlib, nullptr);
    EXPECT_EQ(madlib->GetClass().Hash, 1165770906u);
    ExpectRoundTrip(catalog, *madlib, QuestWireEncoder::GoalsAndRewardsMask);

    QuestWireEncoder::GoalEntryFull fullGoal;
    fullGoal.ServiceName = "quest";
    fullGoal.IconKey = "icon";
    fullGoal.DisplayKey = "display";
    fullGoal.ServiceIndex = 2;
    fullGoal.QuestId = 100;
    fullGoal.GoalId = 200;
    fullGoal.GoalTitle = "goal";
    fullGoal.QuestTitle = "quest title";
    fullGoal.GoalNameId = 300;
    fullGoal.Type = Quests::GoalType::Bounty;
    fullGoal.Status = 1;
    fullGoal.Count = 3;
    fullGoal.Total = 5;
    fullGoal.UseTally = true;
    fullGoal.TallyText = "BOUNTY_TOTAL";
    fullGoal.Location = "Ravenwood";
    fullGoal.ClientTags = { "quest", "bounty" };
    fullGoal.Madlibs = bountyMadlib;
    PropertyObjectPtr goalEntryFull = Build(QuestWireEncoder::BuildGoalEntryFull(catalog, fullGoal));
    ASSERT_NE(goalEntryFull, nullptr);
    EXPECT_EQ(goalEntryFull->GetClass().Hash, 1689163694u);
    ExpectRoundTrip(catalog, *goalEntryFull, QuestWireEncoder::GoalsAndRewardsMask);

    PropertyObjectPtr compilation = Build(QuestWireEncoder::BuildGoalCompilation(catalog, { { fullGoal } }));
    ASSERT_NE(compilation, nullptr);
    ExpectRoundTrip(catalog, *compilation, QuestWireEncoder::GoalsAndRewardsMask);

    QuestWireEncoder::PrepEntry prep;
    prep.ServiceName = "quest";
    prep.IconKey = "icon";
    prep.DisplayKey = "display";
    prep.ServiceIndex = 0;
    prep.PrepText = "PREP_TEXT";
    PropertyObjectPtr prepObject = Build(QuestWireEncoder::BuildPrepEntry(catalog, prep));
    ASSERT_NE(prepObject, nullptr);
    EXPECT_EQ(prepObject->GetClass().Hash, 1653005502u);
    ExpectRoundTrip(catalog, *prepObject, QuestWireEncoder::ServiceMementoMask);

    QuestWireEncoder::GoalEntry goal;
    static_cast<QuestWireEncoder::ServiceOptionFields&>(goal) = prep;
    goal.QuestId = 100;
    goal.GoalId = 200;
    goal.GoalTitle = "goal";
    goal.QuestTitle = "quest title";
    goal.GoalNameId = 300;
    PropertyObjectPtr goalObject = Build(QuestWireEncoder::BuildGoalEntry(catalog, goal));
    ASSERT_NE(goalObject, nullptr);
    ExpectRoundTrip(catalog, *goalObject, QuestWireEncoder::ServiceMementoMask);

    QuestWireEncoder::InteractableOption interactable;
    static_cast<QuestWireEncoder::ServiceOptionFields&>(interactable) = prep;
    interactable.OptionIndex = 1;
    PropertyObjectPtr interactableObject = Build(QuestWireEncoder::BuildInteractableOption(catalog, interactable));
    ASSERT_NE(interactableObject, nullptr);
    ExpectRoundTrip(catalog, *interactableObject, QuestWireEncoder::ServiceMementoMask);

    QuestWireEncoder::ServiceMementoBase memento;
    memento.Options = { prep, goal, interactable };
    QuestMadlibs::NpcFields persona;
    persona.Name = "NPCFormats_Name";
    memento.PersonaMadlibs = QuestMadlibs::BuildNpc(persona);
    memento.NpcNameKey = "NPCFormats_Name";
    memento.NpcTextKey = "GUI_NPCInteractText";
    memento.NpcIcon = "portrait";
    memento.NpcGreetingSound = "greeting";
    memento.NpcFarewellSound = "farewell";
    memento.TurnPlayerToFace = true;
    PropertyObjectPtr service = Build(QuestWireEncoder::BuildServiceMementoBase(catalog, memento));
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->GetClass().Hash, 1846355199u);
    ExpectRoundTrip(catalog, *service, QuestWireEncoder::ServiceMementoMask);

    PropertyObjectPtr tags = Build(QuestWireEncoder::BuildClientTagList(catalog, { { "quest", "bounty" } }));
    ASSERT_NE(tags, nullptr);
    ExpectRoundTrip(catalog, *tags, QuestWireEncoder::GoalsAndRewardsMask);

    PropertyObjectPtr worlds = Build(QuestWireEncoder::BuildAssociatedWorldsList(catalog, { { "WizardCity", "Ravenwood" } }));
    ASSERT_NE(worlds, nullptr);
    ExpectRoundTrip(catalog, *worlds, QuestWireEncoder::GoalsAndRewardsMask);

    QuestWireEncoder::LootInfoList loot;
    loot.Loot = { QuestWireEncoder::GoldLootInfo{ 10 },
        QuestWireEncoder::MagicXPLootInfo{ "Fire", 25 },
        QuestWireEncoder::ItemLootInfo{ 500, 2 },
        QuestWireEncoder::AddSpellLootInfo{ "spell", "Spell_Internal", 600 } };
    loot.GoldInfo = QuestWireEncoder::GoldLootInfo{ 10 };
    PropertyObjectPtr lootList = Build(QuestWireEncoder::BuildLootInfoList(catalog, loot));
    ASSERT_NE(lootList, nullptr);
    ExpectRoundTrip(catalog, *lootList, QuestWireEncoder::GoalsAndRewardsMask);

    Dialogs::ActorDialog dialog;
    dialog.Tag = "QUEST_DIALOG";
    dialog.Entries.push_back({ .ActorTemplateId = 38232,
        .DialogKey = "DIALOG_KEY",
        .Picture = "portrait",
        .GuiDisplay = "gui",
        .Sound = "voice",
        .Action = "action",
        .DialogEvent = "event",
        .Animations = { "idle", "talk" },
        .PersonaName = "persona",
        .NameOverride = "name" });
    dialog.Madlibs.push_back({ bountyMadlib, 0 });
    PropertyObjectPtr actor = Build(QuestWireEncoder::BuildActorDialog(catalog, dialog));
    ASSERT_NE(actor, nullptr);
    EXPECT_EQ(actor->GetClass().Hash, ExpectedActorDialogHash);
    ExpectRoundTrip(catalog, *actor, QuestWireEncoder::ActorDialogMask);
}
