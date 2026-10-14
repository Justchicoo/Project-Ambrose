/*
 * Project Ambrose by Imjustchico
 * Tests that a token's definition is read from its object and refused when it is unnamed, of a kind the server does not keep or bounded wrongly, that states move as the client's own tokens do and count only real changes, and that a client's view adds, moves and removes tokens as their states change and its zone changes.
 */

#include "ZoneTokenFixtures.h"
#include "ZoneTokens.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    ZoneTokenDefinition Definition(PropertyObjectPtr token)
    {
        std::string error;
        std::optional<ZoneTokenDefinition> definition = ZoneTokenStates::DefinitionOf(std::move(token), 1, error);
        EXPECT_TRUE(definition.has_value()) << error;
        return definition ? *definition : ZoneTokenDefinition{};
    }
}

TEST(ZoneTokensTest, ADefinitionReadsTheKindStorageAndBoundsAndRefusesWhatTheServerCannotKeep)
{
    ZoneTokenDefinition const flag = Definition(ZoneTokenFixtures::Boolean("GateOpen", ZoneTokenFixtures::ZoneStorage));
    EXPECT_EQ(flag.Name, "GateOpen");
    EXPECT_EQ(flag.Kind, ZoneTokenKind::Boolean);
    EXPECT_EQ(flag.Storage, ZoneTokenStorage::Zone);
    EXPECT_EQ(flag.ObjectId, 1u);
    ZoneTokenDefinition const pages = Definition(ZoneTokenFixtures::Count("PageCount", 0, 2, 7));
    EXPECT_EQ(pages.Kind, ZoneTokenKind::Count);
    EXPECT_EQ(pages.Storage, ZoneTokenStorage::Player);
    EXPECT_EQ(pages.MinValue, 0);
    EXPECT_EQ(pages.StartValue, 2);
    EXPECT_EQ(pages.MaxValue, 7);
    ASSERT_TRUE(pages.Object);
    EXPECT_EQ(pages.Object->GetClass().Name, "class ZoneTokenCount");

    std::string error;
    EXPECT_FALSE(ZoneTokenStates::DefinitionOf(nullptr, 1, error).has_value());
    EXPECT_EQ(error, "a token does not decode");
    EXPECT_FALSE(ZoneTokenStates::DefinitionOf(ZoneTokenFixtures::Boolean(""), 1, error).has_value());
    EXPECT_NE(error.find("no m_debugName"), std::string::npos) << error;
    EXPECT_FALSE(ZoneTokenStates::DefinitionOf(ZoneTokenFixtures::Count("Wrong", 0, 9, 7), 1, error).has_value());
    EXPECT_NE(error.find("counts from 0 to 7 but starts at 9"), std::string::npos) << error;
    PropertyObjectPtr timer = PropertyObject::Create(ZoneTokenFixtures::Catalog(), "class ZoneTokenTimer");
    ASSERT_TRUE(timer);
    timer->Set("m_debugName", std::string("Countdown"));
    EXPECT_FALSE(ZoneTokenStates::DefinitionOf(std::move(timer), 1, error).has_value());
    EXPECT_NE(error.find("is a class ZoneTokenTimer, which the server does not keep"), std::string::npos) << error;
}

TEST(ZoneTokensTest, StatesMoveAsTheClientsOwnTokensDoAndCountOnlyRealChanges)
{
    ZoneTokenDefinition const flag = Definition(ZoneTokenFixtures::Boolean("GateOpen"));
    ZoneTokenDefinition const pages = Definition(ZoneTokenFixtures::Count("PageCount", 0, 2, 7));
    ZoneTokenStates states;
    EXPECT_EQ(states.Get(flag), (ZoneTokenState{ false, false, 0 }));
    EXPECT_EQ(states.Get(pages), (ZoneTokenState{ false, false, 2 })) << "a counter starts at its start value";

    EXPECT_TRUE(states.Enable(flag));
    EXPECT_FALSE(states.Enable(flag)) << "enabling an enabled token changes nothing";
    EXPECT_EQ(states.GetChangeCount(), 1u);
    EXPECT_TRUE(states.Modify(flag, 1));
    EXPECT_TRUE(states.Get(flag).Set);
    EXPECT_FALSE(states.Modify(flag, 5)) << "a flag is set by any delta of one or more";
    EXPECT_TRUE(states.Modify(flag, 0));
    EXPECT_FALSE(states.Get(flag).Set);
    EXPECT_TRUE(states.Disable(flag));
    EXPECT_EQ(states.Get(flag), (ZoneTokenState{ false, false, 0 }));

    EXPECT_TRUE(states.Modify(pages, 10));
    EXPECT_EQ(states.Get(pages).Value, 7) << "a counter is clamped to its top";
    EXPECT_FALSE(states.Modify(pages, 1));
    EXPECT_TRUE(states.Modify(pages, -20));
    EXPECT_EQ(states.Get(pages).Value, 0) << "and to its bottom";
    EXPECT_EQ(states.GetChangeCount(), 6u);
}

TEST(ZoneTokensTest, AViewAddsMovesAndRemovesTokensAsStatesChangeAndDropsThemAllWhenTheZoneChanges)
{
    ZoneTokenSet tokens;
    tokens.Tokens.push_back(Definition(ZoneTokenFixtures::Boolean("GateOpen")));
    tokens.Tokens.push_back(Definition(ZoneTokenFixtures::Count("PageCount", 0, 0, 7)));
    ZoneTokenStates states;
    auto const stateOf = [&states](ZoneTokenDefinition const& token) { return states.Get(token); };
    ZoneTokenView view;
    EXPECT_TRUE(view.Update("WizardCity/WC_Hub", &tokens, stateOf).empty()) << "a disabled token is not shown";

    states.Enable(*tokens.Find("GateOpen"));
    states.Enable(*tokens.Find("PageCount"));
    std::vector<ZoneTokenChange> changes = view.Update("WizardCity/WC_Hub", &tokens, stateOf);
    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0].Change, ZoneTokenChange::Kind::Add);
    EXPECT_EQ(changes[0].Name, "GateOpen");
    EXPECT_EQ(changes[0].Token, tokens.Find("GateOpen"));
    EXPECT_EQ(changes[1].Name, "PageCount");
    EXPECT_TRUE(view.Update("WizardCity/WC_Hub", &tokens, stateOf).empty()) << "a shown token is added once";

    states.Modify(*tokens.Find("GateOpen"), 1);
    states.Modify(*tokens.Find("PageCount"), 3);
    changes = view.Update("WizardCity/WC_Hub", &tokens, stateOf);
    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0].Change, ZoneTokenChange::Kind::Modify);
    EXPECT_EQ(changes[0].Delta, 1);
    EXPECT_EQ(changes[1].Delta, 3);
    states.Modify(*tokens.Find("PageCount"), -2);
    changes = view.Update("WizardCity/WC_Hub", &tokens, stateOf);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].Delta, -2) << "a counter moves by the difference since it was last shown";

    states.Disable(*tokens.Find("GateOpen"));
    changes = view.Update("WizardCity/WC_Hub", &tokens, stateOf);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].Change, ZoneTokenChange::Kind::Remove);
    EXPECT_EQ(changes[0].Name, "GateOpen");

    changes = view.Update("Krokotopia/KT_Hub", nullptr, stateOf);
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].Change, ZoneTokenChange::Kind::Remove);
    EXPECT_EQ(changes[0].Name, "PageCount") << "leaving the zone takes every shown token away";
    EXPECT_TRUE(view.Update("Krokotopia/KT_Hub", nullptr, stateOf).empty());
}
