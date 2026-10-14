/*
 * Project Ambrose by Imjustchico
 * Tests that an added token goes out as its own object, enabled and in the state the server keeps, under the transmit mask the client reads it with, and that a moved or removed token is named with its delta.
 */

#include "ObjectSerializer.h"
#include "ZoneTokenFixtures.h"
#include "ZoneTokenMessages.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace
{
    ZoneTokenDefinition Definition(PropertyObjectPtr token)
    {
        std::string error;
        std::optional<ZoneTokenDefinition> definition = ZoneTokenStates::DefinitionOf(std::move(token), 1, error);
        EXPECT_TRUE(definition.has_value()) << error;
        return definition ? *definition : ZoneTokenDefinition{};
    }

    PropertyObjectPtr Decode(std::string const& bytes)
    {
        std::vector<uint8> const data(bytes.begin(), bytes.end());
        DecodeResult decoded = ObjectSerializer::Decode(ZoneTokenFixtures::Catalog(), data, SerializerOptions());
        EXPECT_TRUE(decoded.Ok()) << decoded.Detail;
        return std::move(decoded.Object);
    }
}

TEST(ZoneTokenMessagesTest, AnAddedTokenCarriesTheStateTheServerKeepsUnderTheTransmitMask)
{
    ZoneTokenDefinition const pages = Definition(ZoneTokenFixtures::Count("PageCount", 0, 0, 7, ZoneTokenFixtures::ZoneStorage));
    std::string problem;
    std::optional<GameMessages::AddZoneToken> const added = ZoneTokenMessages::Add(42, pages, ZoneTokenState{ true, false, 5 }, problem);
    ASSERT_TRUE(added) << problem;
    EXPECT_EQ(added->GlobalId, 42u);
    PropertyObjectPtr const token = Decode(added->Add);
    ASSERT_TRUE(token);
    EXPECT_EQ(token->GetClass().Name, "class ZoneTokenCount");
    EXPECT_EQ(*token->Get("m_enabled")->GetIf<bool>(), true);
    EXPECT_EQ(*token->Get("m_curValue")->GetIf<int32>(), 5);
    EXPECT_EQ(added->Add.find("PageCount"), std::string::npos) << "a field the transmit mask leaves out does not travel";

    ZoneTokenDefinition const flag = Definition(ZoneTokenFixtures::Boolean("GateOpen"));
    std::optional<GameMessages::AddZoneToken> const set = ZoneTokenMessages::Add(42, flag, ZoneTokenState{ true, true, 0 }, problem);
    ASSERT_TRUE(set) << problem;
    PropertyObjectPtr const gate = Decode(set->Add);
    ASSERT_TRUE(gate);
    EXPECT_EQ(*gate->Get("m_bSet")->GetIf<bool>(), true);
    EXPECT_FALSE(pages.Object->Get("m_enabled")->GetIf<bool>() && *pages.Object->Get("m_enabled")->GetIf<bool>()) << "the zone's own object is left as the data gives it";

    ZoneTokenDefinition bare = flag;
    bare.Object = nullptr;
    EXPECT_FALSE(ZoneTokenMessages::Add(42, bare, ZoneTokenState{}, problem));
    EXPECT_NE(problem.find("no object to send"), std::string::npos) << problem;
}

TEST(ZoneTokenMessagesTest, MovedAndRemovedTokensAreNamed)
{
    ZoneTokenChange const moved{ ZoneTokenChange::Kind::Modify, "PageCount", nullptr, ZoneTokenState{ true, false, 3 }, -2 };
    GameMessages::ModifyZoneToken const modify = ZoneTokenMessages::Modify(42, moved);
    EXPECT_EQ(modify.GlobalId, 42u);
    EXPECT_EQ(modify.TokenName, "PageCount");
    EXPECT_EQ(modify.Delta, -2);
    ZoneTokenChange const gone{ ZoneTokenChange::Kind::Remove, "GateOpen", nullptr, ZoneTokenState{}, 0 };
    GameMessages::RemoveZoneToken const remove = ZoneTokenMessages::Remove(42, gone);
    EXPECT_EQ(remove.GlobalId, 42u);
    EXPECT_EQ(remove.TokenName, "GateOpen");
}
