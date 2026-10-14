/*
 * Project Ambrose by Imjustchico
 * Tests the quest offer without a client: an accept is refused for a quest never offered, for one offered by an NPC the wizard has walked away from and for one declined since, and taken once for an offer that stands; an NPC whose template is only a later actor of a quest's Prep dialog offers nothing, while the first actor offers it until the wizard holds it or has completed it, unless it repeats, in the order of the quests' title keys, as Prep options; a quest held or completed answers ReqHasQuest; and the offer's Prep dialog keeps its lines in order with their actors and its start goals carry their keys, persona and tally.
 */

#include "NpcServiceMemento.h"
#include "ObjectGuid.h"
#include "QuestLog.h"
#include "QuestLogContext.h"
#include "QuestOfferCache.h"
#include "QuestOfferProvider.h"
#include "QuestOffers.h"
#include "QuestStore.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
    constexpr uint32 GiverId = 38232;
    constexpr uint32 LaterActorId = 40001;
    constexpr uint64 Npc = 4503599627370521;

    QuestTemplateRow Quest(std::string name, std::string titleKey, uint32 repeat = 0)
    {
        QuestTemplateRow row;
        row.Name = std::move(name);
        row.TitleKey = std::move(titleKey);
        row.InfoKey = "QuestInfo_" + row.Name;
        row.PrepKey = "QuestPrep_" + row.Name;
        row.Level = 1;
        row.Repeat = repeat;
        return row;
    }

    QuestDialogEntryRow Line(std::string quest, uint32 position, uint32 actor)
    {
        QuestDialogEntryRow row;
        row.Quest = std::move(quest);
        row.Owner = "quest";
        row.Tag = "Prep";
        row.Position = position;
        row.ActorTemplateId = actor;
        row.DialogKey = "Dialog_" + row.Quest + "_Prep_" + std::to_string(position);
        return row;
    }

    void AddQuest(QuestRows& rows, QuestTemplateRow quest, std::vector<uint32> const& prepActors)
    {
        std::string const name = quest.Name;
        rows.Quests.push_back(std::move(quest));
        QuestGoalRow talk;
        talk.Quest = name;
        talk.Goal = "TalkToGiver";
        talk.Type = "persona";
        talk.TitleKey = "WizardQuestGoals_TalkNPC";
        talk.PersonaName = "Giver";
        talk.DestinationZone = "WizardCity/WC_Ravenwood";
        rows.Goals.push_back(talk);
        QuestGoalRow defeat;
        defeat.Quest = name;
        defeat.Goal = "DefeatUndead";
        defeat.Type = "bounty";
        defeat.TitleKey = "WizardQuestGoals_00000003";
        defeat.TallyCount = 5;
        defeat.TallyDescriptorKey = "Undead";
        rows.Goals.push_back(defeat);
        rows.StartGoals.push_back(QuestStartGoalRow{ name, "TalkToGiver" });
        rows.StartGoals.push_back(QuestStartGoalRow{ name, "DefeatUndead" });
        rows.Dialogs.push_back(QuestDialogRow{ name, "quest", "", "Prep" });
        for (uint32 position = 0; position < prepActors.size(); ++position)
            rows.DialogEntries.push_back(Line(name, position, prepActors[position]));
    }

    std::shared_ptr<QuestStore const> Store()
    {
        QuestRows rows;
        rows.Objects = { { 1, "Player" }, { GiverId, "Giver" }, { LaterActorId, "Later" } };
        AddQuest(rows, Quest("Second", "QuestTitle_B"), { GiverId, LaterActorId });
        AddQuest(rows, Quest("First", "QuestTitle_A"), { GiverId });
        AddQuest(rows, Quest("Daily", "QuestTitle_C", 1), { GiverId });
        return QuestStore::Build(rows);
    }

    std::vector<std::string> Names(std::vector<QuestInfo const*> const& quests)
    {
        std::vector<std::string> names;
        for (QuestInfo const* const quest : quests)
            names.push_back(quest->Row.Name);
        return names;
    }
}

TEST(QuestOfferCacheTest, AnAcceptIsTakenOnlyForAnOfferThatStands)
{
    QuestOfferCache offers;
    EXPECT_EQ(offers.Check("First", Npc), QuestOfferVerdict::NeverOffered);
    EXPECT_FALSE(offers.Take("First", Npc));

    offers.Offer("First", Npc);
    EXPECT_EQ(offers.Check("First", Npc), QuestOfferVerdict::Standing);
    EXPECT_EQ(offers.Check("First", Npc + 1), QuestOfferVerdict::NeverOffered) << "an offer is the NPC's that made it";
    offers.LeaveRange(Npc);
    EXPECT_EQ(offers.Check("First", Npc), QuestOfferVerdict::LeftRange);
    EXPECT_FALSE(offers.Take("First", Npc)) << "walking away lapses the offer";

    offers.Offer("First", Npc);
    EXPECT_TRUE(offers.Decline("First"));
    EXPECT_EQ(offers.Check("First", Npc), QuestOfferVerdict::Declined);
    EXPECT_FALSE(offers.Take("First", Npc));
    EXPECT_FALSE(offers.Decline("Never")) << "declining a quest nobody offered changes nothing";

    offers.Offer("First", Npc);
    EXPECT_TRUE(offers.Take("First", Npc));
    EXPECT_EQ(offers.Check("First", Npc), QuestOfferVerdict::NeverOffered) << "an offer is taken once";
}

TEST(QuestOfferTest, OnlyTheFirstPrepActorOffersAQuestAndOnlyWhileTheWizardCanTakeIt)
{
    std::shared_ptr<QuestStore const> const quests = Store();
    ASSERT_NE(quests, nullptr);
    QuestLog log;
    QuestLogContext const context(log);

    EXPECT_EQ(Names(QuestOffers::Available(*quests, GiverId, log, context)), (std::vector<std::string>{ "First", "Second", "Daily" })) << "in the order of their title keys";
    EXPECT_TRUE(QuestOffers::Available(*quests, LaterActorId, log, context).empty()) << "a later Prep actor offers nothing";
    EXPECT_FALSE(QuestOfferProvider::Serves(*quests, LaterActorId));
    EXPECT_TRUE(QuestOfferProvider::Serves(*quests, GiverId));

    GuidGenerator gids(ObjectGuid::QuestBase);
    ASSERT_NE(log.Add("First", gids, 1), nullptr);
    ASSERT_NE(log.Add("Daily", gids, 1), nullptr);
    ASSERT_TRUE(log.CompleteQuest("Daily"));
    ASSERT_NE(log.Add("Second", gids, 1), nullptr);
    ASSERT_TRUE(log.CompleteQuest("Second"));
    EXPECT_EQ(Names(QuestOffers::Available(*quests, GiverId, log, context)), (std::vector<std::string>{ "Daily" }))
        << "a quest held is not offered again, nor one completed, unless it repeats";
    EXPECT_EQ(context.HasQuest("First"), true);
    EXPECT_EQ(context.HasQuest("Second"), true) << "a completed quest still answers ReqHasQuest, which the client's data pairs with its Complete entry";
    EXPECT_EQ(context.HasQuest("Third"), false);

    std::vector<QuestInfo const*> offered;
    QuestOfferProvider provider(quests, log, [&offered](NpcServiceTarget const&, QuestInfo const& quest) { offered.push_back(&quest); });
    std::vector<NpcServiceOption> const options = provider.GetServiceOptions({ 7, Npc, GiverId });
    ASSERT_EQ(options.size(), 1u);
    EXPECT_EQ(options[0].ServiceName, QuestOffers::ServiceName);
    EXPECT_EQ(options[0].IconKey, QuestOffers::PrepIcon);
    EXPECT_EQ(options[0].DisplayKey, "QuestTitle_C");
    EXPECT_EQ(options[0].PrepText, "QuestPrep_Daily");
    EXPECT_EQ(provider.GetPriority(), QuestOfferProvider::Priority);
    provider.OnServiceInteraction({ 7, Npc, GiverId }, 0);
    ASSERT_EQ(offered.size(), 1u);
    EXPECT_EQ(offered[0]->Row.Name, "Daily");
    provider.OnServiceInteraction({ 7, Npc, GiverId }, 1);
    EXPECT_EQ(offered.size(), 1u) << "an index past the quests offered offers nothing";
    EXPECT_TRUE(provider.GetServiceOptions({ 7, Npc, LaterActorId }).empty());

    std::vector<NpcServiceProvider*> const providers{ &provider };
    QuestWireEncoder::ServiceMementoBase const memento = NpcServiceMemento::Build(NpcServiceMenu::Build(providers, { 7, Npc, GiverId }), { "WC-NPCs_00000125", "" });
    ASSERT_EQ(memento.Options.size(), 1u);
    QuestWireEncoder::PrepEntry const* const prep = std::get_if<QuestWireEncoder::PrepEntry>(&memento.Options[0]);
    ASSERT_NE(prep, nullptr) << "a quest is offered in the menu as a PrepEntry";
    EXPECT_EQ(prep->DisplayKey, "QuestTitle_C");
    EXPECT_EQ(prep->PrepText, "QuestPrep_Daily");
    EXPECT_EQ(prep->ServiceIndex, 0u);
}

TEST(QuestOfferTest, TheOfferCarriesThePrepLinesInOrderAndTheStartGoals)
{
    std::shared_ptr<QuestStore const> const quests = Store();
    QuestInfo const* const second = quests->Find("Second");
    ASSERT_NE(second, nullptr);

    Dialogs::ActorDialog const dialog = QuestOffers::BuildPrepDialog(*second);
    EXPECT_EQ(dialog.Tag, "Prep");
    ASSERT_EQ(dialog.Entries.size(), 2u);
    EXPECT_EQ(dialog.Entries[0].ActorTemplateId, GiverId);
    EXPECT_EQ(dialog.Entries[0].DialogKey, "Dialog_Second_Prep_0");
    EXPECT_EQ(dialog.Entries[1].ActorTemplateId, LaterActorId);
    EXPECT_EQ(dialog.Entries[1].DialogKey, "Dialog_Second_Prep_1");

    QuestWireEncoder::GoalCompilation const goals = QuestOffers::BuildStartGoals(*second);
    ASSERT_EQ(goals.Goals.size(), 2u);
    EXPECT_EQ(goals.Goals[0].DisplayKey, "WizardQuestGoals_TalkNPC");
    EXPECT_EQ(goals.Goals[0].PersonaName, "Giver");
    EXPECT_EQ(goals.Goals[0].Type, Quests::GoalType::Persona);
    EXPECT_EQ(goals.Goals[0].DestinationZone, "WizardCity/WC_Ravenwood");
    EXPECT_FALSE(goals.Goals[0].UseTally);
    EXPECT_EQ(goals.Goals[1].Type, Quests::GoalType::Bounty);
    EXPECT_TRUE(goals.Goals[1].UseTally);
    EXPECT_EQ(goals.Goals[1].Total, 5);
    EXPECT_EQ(goals.Goals[1].TallyText, "Undead");
    ASSERT_TRUE(goals.Goals[1].Madlibs.has_value());
    EXPECT_EQ(goals.Goals[1].Madlibs->BlockToken, "GOAL");
}
