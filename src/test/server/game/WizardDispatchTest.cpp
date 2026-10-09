/*
 * Project Ambrose by Imjustchico
 * Checks the in-world MSG_PLAYERWIZBANG dispatch and state mapping, and that both client patch notices are handled while logged in.
 */

#include "GameMessageTable.h"
#include "GameTestHarness.h"
#include "PlayerWizBang.h"
#include "StringHash.h"

#include <gtest/gtest.h>

TEST(WizardDispatchTest, PlayerWizBangIsQueuedForInWorldAndItsReplyIsServerSent)
{
    GameTesting::GameDefinitions definitions;
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    ASSERT_TRUE(catalog);

    MessageInfo const* const playerWizBang = catalog->Find(GameMessages::WizardService, "MSG_PLAYERWIZBANG");
    ASSERT_NE(playerWizBang, nullptr);
    MessageRule const* const handler = GameMessageTable::Get().FindRule(catalog, GameMessages::WizardService, playerWizBang->Definition->Order);
    ASSERT_NE(handler, nullptr);
    EXPECT_EQ(handler->Kind, MessageRuleKind::Handled);
    EXPECT_EQ(handler->Statuses, SessionStatuses::InWorld);
    EXPECT_EQ(handler->Processing, MessageProcessing::Queued);

    MessageInfo const* const wizBang = catalog->Find(GameMessages::GameService, "MSG_WIZBANG");
    ASSERT_NE(wizBang, nullptr);
    MessageRule const* const senderRule = GameMessageTable::Get().FindRule(catalog, GameMessages::GameService, wizBang->Definition->Order);
    ASSERT_NE(senderRule, nullptr);
    EXPECT_EQ(senderRule->Kind, MessageRuleKind::Refused);
    EXPECT_TRUE(catalog->IsDeclared<GameMessages::WizBang>());
}

TEST(WizardDispatchTest, OnlyTheSpellbookStateHasANonzeroWizBangId)
{
    EXPECT_NE(PlayerWizBang::SpellbookId, 0u);
    EXPECT_EQ(PlayerWizBang::SpellbookId, StringHash::KiStringHash("SpellbookWizbang"));
    EXPECT_EQ(PlayerWizBang::IdForState("SpellbookWizbang"), PlayerWizBang::SpellbookId);
    EXPECT_EQ(PlayerWizBang::IdForState(""), 0u);
    EXPECT_EQ(PlayerWizBang::IdForState("Jumping"), 0u);
}

TEST(WizardDispatchTest, PatchNoticesAreHandledForLoggedInWizards)
{
    GameTesting::GameDefinitions definitions;
    MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
    ASSERT_TRUE(catalog);
    MessageHandlerTable<GameSession> const& table = GameMessageTable::Get();

    for (std::string_view const tag : { "MSG_PATCHINGBLOCKED", "MSG_LOGPATCHCLIENTPATCHTIME" })
    {
        MessageInfo const* const info = catalog->Find(GameMessages::WizardService, tag);
        ASSERT_NE(info, nullptr) << tag;
        MessageRule const* const rule = table.FindRule(catalog, GameMessages::WizardService, info->Definition->Order);
        ASSERT_NE(rule, nullptr) << tag;
        EXPECT_EQ(rule->Kind, MessageRuleKind::Handled) << tag;
        EXPECT_EQ(rule->Statuses, SessionStatuses::LoggedIn | SessionStatuses::InWorld) << tag;
        EXPECT_EQ(rule->Processing, MessageProcessing::InPlace) << tag;
    }
}
