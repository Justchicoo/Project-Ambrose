/*
 * Project Ambrose by Imjustchico
 * Checks the NPC service menu's dispatch: MSG_INTERACTNPC, GAME's MSG_INTERACTOBJECT and MSG_INTERACTOPTION, WIZARD's MSG_INTERACTAVAILABLEQUEST, MSG_INTERACTCOMPLETEGOAL, MSG_COMPLETEDIALOG, MSG_INTERACTUNDERWAYQUEST and MSG_REQUESTQUESTDIALOG and QUEST's MSG_DECLINEQUEST, MSG_ACCEPTQUEST and MSG_COMPLETEPERSONA are queued for the world thread once a wizard is in the world, and MSG_SENDNPCOPTIONS, MSG_LEAVESERVICERANGE, MSG_SENDINTERACTOPTIONS, MSG_ACTORDIALOG, MSG_QUESTDIALOG, MSG_QUESTOFFER, MSG_SENDQUEST, MSG_SENDGOAL, MSG_REMOVEQUEST, MSG_COMPLETEGOAL and MSG_COMPLETEQUEST are declared as ones the server sends and refused from clients.
 */

#include "GameMessageTable.h"
#include "GameTestHarness.h"

#include <gtest/gtest.h>

#include <string_view>
#include <utility>

TEST(NpcDispatchTest, ClicksAreQueuedInWorldAndTheMenuMessagesAreServerSent)
{
    GameTesting::GameDefinitions definitions;
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    ASSERT_TRUE(catalog);
    MessageHandlerTable<GameSession> const& table = GameMessageTable::Get();

    for (auto const& [service, tag] : { std::pair<uint8, std::string_view>{ GameMessages::QuestService, "MSG_INTERACTNPC" }, { GameMessages::GameService, "MSG_INTERACTOBJECT" },
             { GameMessages::GameService, "MSG_INTERACTOPTION" }, { GameMessages::WizardService, "MSG_INTERACTAVAILABLEQUEST" }, { GameMessages::QuestService, "MSG_DECLINEQUEST" },
             { GameMessages::QuestService, "MSG_ACCEPTQUEST" },
             { GameMessages::WizardService, "MSG_INTERACTCOMPLETEGOAL" }, { GameMessages::QuestService, "MSG_COMPLETEPERSONA" }, { GameMessages::WizardService, "MSG_COMPLETEDIALOG" },
             { GameMessages::WizardService, "MSG_INTERACTUNDERWAYQUEST" }, { GameMessages::WizardService, "MSG_REQUESTQUESTDIALOG" } })
    {
        MessageInfo const* const info = catalog->Find(service, tag);
        ASSERT_NE(info, nullptr) << tag;
        MessageRule const* const rule = table.FindRule(catalog, service, info->Definition->Order);
        ASSERT_NE(rule, nullptr) << tag;
        EXPECT_EQ(rule->Kind, MessageRuleKind::Handled) << tag;
        EXPECT_EQ(rule->Statuses, SessionStatuses::InWorld) << tag;
        EXPECT_EQ(rule->Processing, MessageProcessing::Queued) << tag;
    }

    for (auto const& [service, tag] : { std::pair<uint8, std::string_view>{ GameMessages::QuestService, "MSG_SENDNPCOPTIONS" }, { GameMessages::GameService, "MSG_LEAVESERVICERANGE" },
             { GameMessages::GameService, "MSG_SENDINTERACTOPTIONS" }, { GameMessages::WizardService, "MSG_ACTORDIALOG" }, { GameMessages::QuestService, "MSG_QUESTOFFER" },
             { GameMessages::QuestService, "MSG_SENDQUEST" }, { GameMessages::QuestService, "MSG_SENDGOAL" }, { GameMessages::QuestService, "MSG_REMOVEQUEST" },
             { GameMessages::QuestService, "MSG_COMPLETEGOAL" }, { GameMessages::QuestService, "MSG_COMPLETEQUEST" }, { GameMessages::WizardService, "MSG_QUESTDIALOG" } })
    {
        MessageInfo const* const info = catalog->Find(service, tag);
        ASSERT_NE(info, nullptr) << tag;
        MessageRule const* const rule = table.FindRule(catalog, service, info->Definition->Order);
        ASSERT_NE(rule, nullptr) << tag;
        EXPECT_EQ(rule->Kind, MessageRuleKind::Refused) << tag;
    }
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::SendNpcOptions>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::LeaveServiceRange>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::SendInteractOptions>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::ActorDialog>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::QuestOffer>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::SendQuest>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::SendGoal>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::RemoveQuest>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::CompleteGoal>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::CompleteQuest>());
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::QuestDialog>());
}
