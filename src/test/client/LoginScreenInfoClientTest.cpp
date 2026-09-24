/*
 * Project Ambrose by Imjustchico
 * Builds the character select blob for wizards against the user's own r806919 type dump, when AMBROSE_TYPE_DUMP_PATH names it, and checks it starts with WizardCharacterCreationInfo's class hash, decodes to an object equal to the one built, whose one property outside the mask is left at its default, re-encodes byte for byte, and with no custom name and an empty equipment list takes 96 bytes plus its location, where the reference server's 157 to 221 byte blobs carried equipped items.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "LoginScreenInfoBuilder.h"
#include "ObjectFields.h"

#include <gtest/gtest.h>

#include <iostream>
#include <string>

TEST(LoginScreenInfoClientTest, TheCharacterSelectBlobDecodesToTheBuiltWizardAndSizesByItsLocation)
{
    std::optional<std::string> const path = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
    if (!path || path->empty())
        GTEST_SKIP() << "set AMBROSE_TYPE_DUMP_PATH to the r806919 type dump (format v2) from your own client to run this test";

    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromFile(LogConfig::Utf8Path(*path)));
    TypeCatalogPtr const catalog = registry.GetCatalog();
    ObjectField const& field = *ObjectFields::Find("MSG_CHARACTERINFO", "CharacterInfo");

    for (std::string const& zone : { std::string("WizardCity/WC_Ravenwood"), std::string("WizardCity/WC_Hub"), std::string() })
    {
        CharacterSummary character;
        character.Guid = 5739324522485080744ull;
        character.Account = 123456;
        character.NameIndices = (120u << 16) | (45u << 8) | 200u;
        character.SchoolId = 2343174;
        character.Level = 50;
        character.World = 1;
        character.ZoneDisplay = zone;
        character.LastLogout = 1788748388;
        character.Appearance.Gender = 1;
        character.Appearance.HairColor = 12;
        character.Appearance.HatColor = 3;
        character.Appearance.BehaviorTemplateNameId = 0;

        EncodeResult const encoded = LoginScreenInfoBuilder::Encode(catalog, character);
        ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
        ASSERT_GE(encoded.Bytes.size(), 4u);
        EXPECT_EQ(uint32{ encoded.Bytes[0] } | (uint32{ encoded.Bytes[1] } << 8) | (uint32{ encoded.Bytes[2] } << 16) | (uint32{ encoded.Bytes[3] } << 24), 292458316u);
        std::cout << "[ BLOB     ] location '" << zone << "': " << encoded.Bytes.size() << " bytes" << std::endl;
        EXPECT_EQ(encoded.Bytes.size(), 96 + zone.size());

        DecodeResult const decoded = ObjectSerializer::DecodeField(catalog, field, encoded.Bytes);
        ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
        std::string problem;
        PropertyObjectPtr const built = LoginScreenInfoBuilder::Build(catalog, character, problem);
        ASSERT_TRUE(built) << problem;
        EXPECT_TRUE(*decoded.Object == *built);
        EncodeResult const again = ObjectSerializer::EncodeField(field, decoded.Object.get());
        ASSERT_TRUE(again.Ok()) << again.Detail;
        EXPECT_EQ(again.Bytes, encoded.Bytes);
    }
}
