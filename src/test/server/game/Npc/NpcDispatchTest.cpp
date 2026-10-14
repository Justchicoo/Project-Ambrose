/*
 * Project Ambrose by Imjustchico
 * Checks the NPC service menu's dispatch: MSG_INTERACTNPC, GAME's MSG_INTERACTOBJECT and MSG_INTERACTOPTION, WIZARD's MSG_INTERACTAVAILABLEQUEST and QUEST's MSG_DECLINEQUEST and MSG_ACCEPTQUEST are queued for the world thread once a wizard is in the world, and MSG_SENDNPCOPTIONS, MSG_LEAVESERVICERANGE, MSG_SENDINTERACTOPTIONS, MSG_ACTORDIALOG, MSG_QUESTOFFER, MSG_SENDQUEST, MSG_SENDGOAL and MSG_REMOVEQUEST are declared as ones the server sends and refused from clients.
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
             { GameMessages::QuestService, "MSG_ACCEPTQUEST" } })
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
             { GameMessages::QuestService, "MSG_SENDQUEST" }, { GameMessages::QuestService, "MSG_SENDGOAL" }, { GameMessages::QuestService, "MSG_REMOVEQUEST" } })
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
}
