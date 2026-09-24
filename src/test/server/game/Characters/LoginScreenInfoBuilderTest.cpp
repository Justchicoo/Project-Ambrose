/*
 * Project Ambrose by Imjustchico
 * Tests the login screen info builder on the project's own copy of the character select classes: the blob starts with WizardCharacterCreationInfo's class hash 292458316, decodes back to the character's values with its appearance, carries the properties flagged Transmit and AuthorityTransmit only, and names the property that refuses an appearance value too wide for its bit field or a missing class.
 */

#include "CharacterTypeFixtures.h"
#include "LoginScreenInfoBuilder.h"
#include "ObjectViews.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <string>

namespace
{
    CharacterSummary MakeCharacter()
    {
        CharacterSummary character;
        character.Guid = 0x0123456789ABCDEFull;
        character.Account = 42;
        character.NameIndices = (12u << 16) | (34u << 8) | 56u;
        character.ShouldRename = true;
        character.SchoolId = 2343174;
        character.Level = 17;
        character.World = 3;
        character.ZoneDisplay = "WizardCity/WC_Ravenwood";
        character.LastLogout = 1800000000;
        CharacterAppearance& look = character.Appearance;
        look.BehaviorTemplateNameId = 77;
        look.Gender = 1;
        look.Race = 1234567;
        look.HeadHandsModel = 3;
        look.HairModel = 15;
        look.HatModel = 2;
        look.TorsoModel = 1;
        look.FeetModel = 0;
        look.WandModel = 3;
        look.SkinColor = 9;
        look.SkinDecal = 4;
        look.HairColor = 127;
        look.HatColor = 31;
        look.HatDecal = 17;
        look.TorsoColor = 5;
        look.TorsoDecal = 30;
        look.TorsoDecal2 = 1;
        look.FeetColor = 22;
        look.FeetDecal = 8;
        look.SkinDecal2 = 0xBEEF;
        look.ExtendedHairColor = 200;
        look.ExtendedSkinDecal = 0x1234;
        look.AfterCombatDance = 3;
        look.AfterCombatVictoryDance = 0xFFFFFFFFu;
        look.NewPlayerOptions = 0x80000001u;
        look.NewPlayerOptions2 = 5;
        return character;
    }

    class LoginScreenInfoBuilderTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(_registry.LoadFromText(CharacterTypeFixtures::Dump(), "characters.json")) << (_registry.GetErrors().empty() ? std::string() : _registry.GetErrors().front());
            _catalog = _registry.GetCatalog();
        }

        TypedViewRegistry _views;
        TypeRegistry _registry{ &_views };
        TypeCatalogPtr _catalog;
    };
}

TEST_F(LoginScreenInfoBuilderTest, TheBlobStartsWithTheClassHashAndDecodesBackToTheCharacter)
{
    CharacterSummary const character = MakeCharacter();
    EncodeResult const encoded = LoginScreenInfoBuilder::Encode(_catalog, character);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    ASSERT_GE(encoded.Bytes.size(), 4u);
    uint32 const hash = uint32{ encoded.Bytes[0] } | (uint32{ encoded.Bytes[1] } << 8) | (uint32{ encoded.Bytes[2] } << 16) | (uint32{ encoded.Bytes[3] } << 24);
    EXPECT_EQ(hash, 292458316u);
    EXPECT_EQ(StringHash::KiStringHash("class WizardCharacterCreationInfo"), 292458316u);

    ObjectField const& field = *ObjectFields::Find("MSG_CHARACTERINFO", "CharacterInfo");
    DecodeResult const decoded = ObjectSerializer::DecodeField(_catalog, field, encoded.Bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    std::string problem;
    PropertyObjectPtr const built = LoginScreenInfoBuilder::Build(_catalog, character, problem);
    ASSERT_TRUE(built) << problem;
    PropertyObjectPtr expected = built->Clone();
    ASSERT_EQ(expected->EditObjectAt(expected->GetClass().FindProperty("m_avatarBehavior")->Id)->Set("m_behaviorTemplateNameID", uint32{ 0 }), PropertySetResult::Ok);
    EXPECT_TRUE(*decoded.Object == *expected);

    PropertyObject const& info = *decoded.Object;
    EXPECT_EQ(*info.Get("m_templateID")->GetIf<int32>(), 1);
    EXPECT_TRUE(info.Get("m_name")->GetIf<std::u16string>()->empty());
    EXPECT_EQ(*info.Get("m_globalID")->GetIf<uint64>(), character.Guid);
    EXPECT_EQ(*info.Get("m_userID")->GetIf<uint64>(), 42u);
    EXPECT_TRUE(*info.Get("m_shouldRename")->GetIf<bool>());
    EXPECT_EQ(*info.Get("m_lastLoginTime")->GetIf<uint32>(), 1800000000u);
    EXPECT_EQ(*info.Get("m_location")->GetIf<std::string>(), "WizardCity/WC_Ravenwood");
    EXPECT_EQ(*info.Get("m_level")->GetIf<int32>(), 17);
    EXPECT_EQ(*info.Get("m_schoolOfFocus")->GetIf<uint32>(), 2343174u);
    EXPECT_EQ(*info.Get("m_nameIndices")->GetIf<uint32>(), character.NameIndices);
    PropertyObject const* const behavior = info.Get("m_avatarBehavior")->AsObject();
    ASSERT_NE(behavior, nullptr);
    EXPECT_EQ(*behavior->Get("m_nHairColor")->GetIf<uint32>(), 127u);
    EXPECT_EQ(*behavior->Get("m_eGender")->GetIf<int64>(), 1);
    EXPECT_EQ(*behavior->Get("m_nSkinDecal2")->GetIf<uint16>(), 0xBEEF);
    EXPECT_EQ(*behavior->Get("m_afterCombatVictoryDance")->GetIf<uint32>(), 0xFFFFFFFFu);
    EXPECT_EQ(*behavior->Get("m_behaviorTemplateNameID")->GetIf<uint32>(), 0u);
    PropertyObject const* const equipment = info.Get("m_equipmentInfoList")->AsObject();
    ASSERT_NE(equipment, nullptr);
    EXPECT_TRUE(equipment->Get("m_infoList")->GetList()->empty());
}

TEST_F(LoginScreenInfoBuilderTest, CustomNamesTravelAsUtf16AndBadAppearanceIsNamed)
{
    CharacterSummary character = MakeCharacter();
    character.CustomName = "M\xC3\xA9rlin";
    std::string problem;
    PropertyObjectPtr const named = LoginScreenInfoBuilder::Build(_catalog, character, problem);
    ASSERT_TRUE(named) << problem;
    EXPECT_EQ(*named->Get("m_name")->GetIf<std::u16string>(), (std::u16string{ u'M', char16_t{ 0xE9 }, u'r', u'l', u'i', u'n' }));

    character.Appearance.HatModel = 4;
    EXPECT_FALSE(LoginScreenInfoBuilder::Build(_catalog, character, problem));
    EXPECT_EQ(problem, "class WizardCharacterBehavior property m_nHatModel refused its value: the value does not fit the property's bit width or 32-bit enum range");
    EncodeResult const refused = LoginScreenInfoBuilder::Encode(_catalog, character);
    EXPECT_FALSE(refused.Ok());
    EXPECT_EQ(refused.Detail, problem);

    EXPECT_FALSE(LoginScreenInfoBuilder::Build(nullptr, MakeCharacter(), problem));
    EXPECT_EQ(problem, "no type dump is loaded");
    TypeRegistry empty;
    uint32 const root = StringHash::KiStringHash("class PropertyClass");
    nlohmann::json classes = nlohmann::json::object();
    classes[std::to_string(root)] = nlohmann::json{ { "name", "class PropertyClass" }, { "bases", nlohmann::json::array() }, { "hash", root }, { "properties", nlohmann::json::object() } };
    ASSERT_TRUE(empty.LoadFromText(nlohmann::json{ { "version", 2 }, { "classes", classes } }.dump(), "empty.json")) << (empty.GetErrors().empty() ? std::string() : empty.GetErrors().front());
    EXPECT_FALSE(LoginScreenInfoBuilder::Build(empty.GetCatalog(), MakeCharacter(), problem));
    EXPECT_EQ(problem, "the type dump has no property class class WizardCharacterCreationInfo");
}
