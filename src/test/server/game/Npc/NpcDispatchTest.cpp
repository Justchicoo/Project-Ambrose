/*
 * Project Ambrose by Imjustchico
 * Checks the NPC service menu's dispatch: MSG_INTERACTNPC and GAME's MSG_INTERACTOBJECT and MSG_INTERACTOPTION are queued for the world thread once a wizard is in the world, and MSG_SENDNPCOPTIONS, MSG_LEAVESERVICERANGE and MSG_SENDINTERACTOPTIONS are declared as ones the server sends and refused from clients.
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
             { GameMessages::GameService, "MSG_INTERACTOPTION" } })
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
             { GameMessages::GameService, "MSG_SENDINTERACTOPTIONS" } })
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
}
