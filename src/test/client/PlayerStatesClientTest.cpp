/*
 * Project Ambrose by Imjustchico
 * Reads the player's object states from the user's own r806919 install, when AMBROSE_CLIENT_DIR names it and AMBROSE_TYPE_DUMP_PATH its type dump: the player object template's object state behavior names the PlayerMobileStates set, whose Jump category holds the Jumping state that plays the Jump animation and falls back to NotJumping on its own, so the id the server sends for a jump is that state's; its Expression category holds Emoting, an EmoteGameState whose own animations name nothing, so the animation its override names is the one that plays, and which leaves only for Unremarkable, the category's base state, with no state it returns to on its own; and the NotShopping state of the same set hashes to the id a retail server sent in MSG_ENTERSTATE, which pins the client's string hash as the one states are named by.
 */

#include "BindFile.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "PlayerStates.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "XmlObjectReader.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    std::string TextOf(PropertyObject const& object, std::string_view property)
    {
        PropertyValue const* const value = object.Get(property);
        std::string const* const text = value ? value->GetIf<std::string>() : nullptr;
        return text ? *text : std::string();
    }

    PropertyObject const* Named(PropertyValue const* list, std::string_view property, std::string_view name)
    {
        if (!list || !list->GetList())
            return nullptr;
        for (PropertyValue const& item : *list->GetList())
            if (item.AsObject() && TextOf(*item.AsObject(), property) == name)
                return item.AsObject();
        return nullptr;
    }

    class PlayerStatesClientTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to your own r806919 install and AMBROSE_TYPE_DUMP_PATH to its type dump to run this test";
            ASSERT_TRUE(_registry.LoadFromFile(LogConfig::Utf8Path(*dump))) << (_registry.GetErrors().empty() ? std::string() : _registry.GetErrors().front());
            std::string error;
            _archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / "Root.wad", error);
            ASSERT_TRUE(_archive) << error;
        }

        PropertyObjectPtr ReadEntry(std::string_view name)
        {
            KiwadReadResult const read = _archive->Read(name);
            EXPECT_TRUE(read.Succeeded()) << name << ": " << read.Error;
            if (BindFile::IsBind(read.Data))
            {
                BindReadResult bind = BindFile::Read(_registry.GetCatalog(), read.Data);
                EXPECT_TRUE(bind.Ok()) << name << ": " << bind.Detail;
                return std::move(bind.Decoded.Object);
            }
            XmlReadResult xml = XmlObjectReader::Read(_registry.GetCatalog(), std::string_view(reinterpret_cast<char const*>(read.Data.data()), read.Data.size()));
            EXPECT_TRUE(xml.Ok() && !xml.Objects.empty()) << name << ": " << xml.Detail;
            return xml.Objects.empty() ? nullptr : std::move(xml.Objects.front());
        }

        TypeRegistry _registry;
        std::unique_ptr<KiwadArchive> _archive;
    };
}

TEST_F(PlayerStatesClientTest, ThePlayerObjectUsesTheStateSetWhoseJumpingStatePlaysTheJump)
{
    PropertyObjectPtr const player = ReadEntry("ObjectData/PlayerObject.xml");
    ASSERT_TRUE(player);
    PropertyObject const* const behavior = Named(player->Get("m_behaviors"), "m_behaviorName", "BasicObjectStateBehavior");
    ASSERT_NE(behavior, nullptr);
    EXPECT_EQ(TextOf(*behavior, "m_stateSetName"), PlayerStates::StateSet);

    PropertyObjectPtr const states = ReadEntry("StateData/" + std::string(PlayerStates::StateSet) + ".xml");
    ASSERT_TRUE(states);
    PropertyObject const* const jump = Named(states->Get("m_categories"), "m_categoryName", PlayerStates::JumpCategory);
    ASSERT_NE(jump, nullptr);
    PropertyObject const* const jumping = Named(jump->Get("m_states"), "m_stateName", PlayerStates::JumpingName);
    ASSERT_NE(jumping, nullptr);
    EXPECT_NE(Named(jumping->Get("m_animations"), "m_assetName", "Jump"), nullptr);
    EXPECT_EQ(TextOf(*jumping, "m_autoState"), TextOf(*jump, "m_baseState"));
    EXPECT_EQ(PlayerStates::Jumping, StringHash::StringId(TextOf(*jumping, "m_stateName")));
}

TEST_F(PlayerStatesClientTest, TheExpressionCategorysEmotingStatePlaysTheAnimationItsOverrideNames)
{
    PropertyObjectPtr const states = ReadEntry("StateData/" + std::string(PlayerStates::StateSet) + ".xml");
    ASSERT_TRUE(states);
    PropertyObject const* const expression = Named(states->Get("m_categories"), "m_categoryName", PlayerStates::ExpressionCategory);
    ASSERT_NE(expression, nullptr);
    PropertyObject const* const emoting = Named(expression->Get("m_states"), "m_stateName", PlayerStates::EmotingName);
    ASSERT_NE(emoting, nullptr);
    EXPECT_EQ(TextOf(*emoting, "m_stateType"), "EmoteGameState");
    PropertyValue const* const animations = emoting->Get("m_animations");
    ASSERT_TRUE(animations && animations->GetList());
    for (PropertyValue const& animation : *animations->GetList())
        EXPECT_TRUE(animation.AsObject() && TextOf(*animation.AsObject(), "m_assetName").empty()) << "the state names no animation of its own";
    EXPECT_EQ(TextOf(*expression, "m_baseState"), PlayerStates::UnremarkableName);
    EXPECT_NE(Named(emoting->Get("m_transitions"), "m_targetState", PlayerStates::UnremarkableName), nullptr);
    EXPECT_TRUE(TextOf(*emoting, "m_autoState").empty()) << "nothing takes the state back to Unremarkable on its own";
    EXPECT_EQ(PlayerStates::Emoting, StringHash::StringId(TextOf(*emoting, "m_stateName")));
    EXPECT_EQ(PlayerStates::Unremarkable, StringHash::StringId(TextOf(*expression, "m_baseState")));
}

TEST_F(PlayerStatesClientTest, AStateIsNamedByTheClientsStringHashAsARetailServerSentIt)
{
    PropertyObjectPtr const states = ReadEntry("StateData/" + std::string(PlayerStates::StateSet) + ".xml");
    ASSERT_TRUE(states);
    PropertyObject const* const shopping = Named(states->Get("m_categories"), "m_categoryName", "Shopping");
    ASSERT_NE(shopping, nullptr);
    ASSERT_NE(Named(shopping->Get("m_states"), "m_stateName", "NotShopping"), nullptr);
    EXPECT_EQ(StringHash::StringId("NotShopping"), 1685237158u);
}
