/*
 * Project Ambrose by Imjustchico
 * Tests what the login server is willing to make a wizard out of, on the project's own copy of the creation classes: a request with every field in order becomes a wizard standing where the world rows say, carrying the school, the look and the name it asked for; and a school no row knows, the neutral gender, a race that is not human, a name index past the end of its table, a seventh wizard on an account allowed six, a school with no starting state and a name of its own on a server that does not allow one are each refused by name, with nothing filled in. The slot limit is read from the limits handed to each call, so lowering it refuses the next request without anything being restarted. A value too wide for its property is refused where the refusal bites, which is the moment it is asked for: a bui4 model above fifteen cannot be put into the object at all, so no client can encode one, and a blob that is not a creation info at all is refused by the decoder rather than by this reader.
 */

#include "CharacterCreateStore.h"
#include "CharacterNames.h"
#include "CharacterTypeFixtures.h"
#include "CreationInfoReader.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr uint32 FireSchool = 2343174;
    constexpr uint32 IceSchool = 72777;
    constexpr uint32 HumanRace = 79806088;
    constexpr uint32 Male = 1;
    constexpr uint32 Neutral = 2;

    TypeCatalogPtr Catalog()
    {
        static TypedViewRegistry views;
        static TypeRegistry registry{ &views };
        static TypeCatalogPtr const catalog = []() -> TypeCatalogPtr
        {
            if (!registry.LoadFromText(CharacterTypeFixtures::Dump(), "characters.json"))
                return nullptr;
            return registry.GetCatalog();
        }();
        return catalog;
    }

    std::shared_ptr<CharacterCreateSet const> Rows(bool startForEverySchool = true)
    {
        std::map<uint32, CharacterSchool> schools;
        schools.emplace(FireSchool, CharacterSchool{ FireSchool, "Fire", 0 });
        schools.emplace(IceSchool, CharacterSchool{ IceSchool, "Ice", 1 });

        std::map<uint32, CharacterStartState> starts;
        if (startForEverySchool)
        {
            CharacterStartState start;
            start.SchoolId = 0;
            start.World = 3;
            start.Zone = "WizardCity/WC_Ravenwood";
            start.ZoneDisplay = "Ravenwood";
            start.PositionX = 12.5f;
            start.PositionY = -3.0f;
            start.PositionZ = 400.0f;
            start.Orientation = 1.5f;
            start.Level = 1;
            start.Experience = 0;
            starts.emplace(0, start);
        }
        return std::make_shared<CharacterCreateSet const>(std::move(schools), std::move(starts));
    }

    std::shared_ptr<CharacterNameSet const> Names()
    {
        auto table = [](std::string name, std::vector<std::pair<std::string, std::string>> parts)
        {
            CharacterNameTable made{ std::move(name), "en-US", {} };
            for (auto& [key, text] : parts)
                made.Parts.push_back(CharacterNamePart{ std::move(key), std::move(text) });
            return made;
        };
        std::vector<CharacterNameTable> tables{
            table("FirstName_HumanMale", { { "First_Boy_0", "Aaron" }, { "First_Boy_1", "Blaze" } }),
            table("FirstName_HumanFemale", { { "First_Girl_0", "Abby" } }),
            table("MiddleName_Human", { { "", "" }, { "Middle_0", "Storm" } }),
            table("LastName_Human", { { "", "" }, { "Last_0", "Blade" } }),
        };
        std::vector<std::string> errors;
        std::shared_ptr<CharacterNameSet const> names = CharacterNameSet::Build(std::move(tables), {}, errors);
        EXPECT_TRUE(errors.empty()) << errors.front();
        return names;
    }

    PropertyObjectPtr Appearance(uint32 gender = Male, uint32 race = HumanRace)
    {
        PropertyObjectPtr behavior = PropertyObject::Create(Catalog(), "class WizardCharacterBehavior");
        EXPECT_TRUE(behavior);
        EXPECT_EQ(behavior->Set("m_behaviorTemplateNameID", uint32{ 4242 }), PropertySetResult::Ok);
        for (char const* name : { "m_nHeadHandsModel", "m_nHatModel", "m_nTorsoModel", "m_nFeetModel", "m_nWandModel" })
            EXPECT_EQ(behavior->Set(name, uint32{ 2 }), PropertySetResult::Ok) << name;
        EXPECT_EQ(behavior->Set("m_nHairModel", uint32{ 9 }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_nSkinColor", uint32{ 3 }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_nSkinDecal", uint32{ 4 }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_nHairColor", uint32{ 70 }), PropertySetResult::Ok);
        for (char const* name : { "m_nHatColor", "m_nHatDecal", "m_nTorsoColor", "m_nTorsoDecal", "m_nTorsoDecal2", "m_nFeetColor", "m_nFeetDecal" })
            EXPECT_EQ(behavior->Set(name, uint32{ 11 }), PropertySetResult::Ok) << name;
        EXPECT_EQ(behavior->Set("m_eGender", int64{ gender }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_eRace", int64{ race }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_afterCombatDance", uint8{ 1 }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_nSkinDecal2", uint16{ 300 }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_extendedHairColor", uint8{ 7 }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_extendedSkinDecal", uint16{ 900 }), PropertySetResult::Ok);
        EXPECT_EQ(behavior->Set("m_afterCombatVictoryDance", uint32{ 5 }), PropertySetResult::Ok);
        return behavior;
    }

    PropertyObjectPtr Request(uint32 school = FireSchool, uint32 nameIndices = (1u << 16) | (1u << 8) | 1u, uint32 gender = Male,
        uint32 race = HumanRace)
    {
        PropertyObjectPtr info = PropertyObject::Create(Catalog(), "class WizardCharacterCreationInfo");
        EXPECT_TRUE(info);
        EXPECT_EQ(info->Set("m_templateID", int32{ 1 }), PropertySetResult::Ok);
        EXPECT_EQ(info->Set("m_schoolOfFocus", school), PropertySetResult::Ok);
        EXPECT_EQ(info->Set("m_nameIndices", nameIndices), PropertySetResult::Ok);
        EXPECT_EQ(info->Set("m_avatarBehavior", PropertyValue(Appearance(gender, race))), PropertySetResult::Ok);
        return info;
    }

    CreationOutcome ReadRequest(PropertyObject const& info, CreationLimits limits = {}, CreationRules rules = {},
        std::shared_ptr<CharacterCreateSet const> rows = Rows())
    {
        rules.Locale = "en-US";
        return CreationInfoReader::Read(info, *rows, *Names(), limits, rules);
    }
}

TEST(CreationInfoReaderTest, AWizardIsMadeFromTheRequestAndTheWorldRows)
{
    PropertyObjectPtr const info = Request();
    ASSERT_TRUE(info);

    CreationOutcome const outcome = ReadRequest(*info);
    ASSERT_TRUE(outcome.Ok) << outcome.Refusal;

    CharacterSummary const& made = outcome.Character;
    EXPECT_EQ(made.Guid, 0u) << "which id this wizard gets is not this reader's to decide";
    EXPECT_EQ(made.SchoolId, FireSchool);
    EXPECT_EQ(made.NameIndices, (1u << 16) | (1u << 8) | 1u);
    EXPECT_FALSE(made.CustomName.has_value());
    EXPECT_EQ(made.Level, 1);
    EXPECT_EQ(made.Experience, 0);
    EXPECT_EQ(made.World, 3);
    EXPECT_EQ(made.Zone, "WizardCity/WC_Ravenwood");
    EXPECT_EQ(made.ZoneDisplay, "Ravenwood");
    EXPECT_FLOAT_EQ(made.PositionX, 12.5f);
    EXPECT_FLOAT_EQ(made.Orientation, 1.5f);
    EXPECT_EQ(made.Appearance.Gender, Male);
    EXPECT_EQ(made.Appearance.Race, HumanRace);
    EXPECT_EQ(made.Appearance.BehaviorTemplateNameId, 4242u);
    EXPECT_EQ(made.Appearance.HairModel, 9);
    EXPECT_EQ(made.Appearance.HairColor, 70);
    EXPECT_EQ(made.Appearance.SkinDecal2, 300);
    EXPECT_EQ(made.Appearance.ExtendedSkinDecal, 900);
    EXPECT_EQ(made.Appearance.AfterCombatVictoryDance, 5u);
}

TEST(CreationInfoReaderTest, ASchoolNoRowKnowsIsRefused)
{
    PropertyObjectPtr const info = Request(1234567);
    ASSERT_TRUE(info);

    CreationOutcome const outcome = ReadRequest(*info);
    EXPECT_FALSE(outcome.Ok);
    EXPECT_NE(outcome.Refusal.find("1234567"), std::string::npos) << outcome.Refusal;
    EXPECT_EQ(outcome.Character.SchoolId, 0u) << "a refused request fills nothing in";
}

TEST(CreationInfoReaderTest, TheNeutralGenderAndANonHumanRaceAreRefused)
{
    PropertyObjectPtr const neutral = Request(FireSchool, (1u << 16) | (1u << 8) | 1u, Neutral);
    ASSERT_TRUE(neutral);
    CreationOutcome const genderOutcome = ReadRequest(*neutral);
    EXPECT_FALSE(genderOutcome.Ok);
    EXPECT_NE(genderOutcome.Refusal.find("neutral"), std::string::npos) << genderOutcome.Refusal;

    PropertyObjectPtr const frog = Request(FireSchool, (1u << 16) | (1u << 8) | 1u, Male, 2274918);
    ASSERT_TRUE(frog);
    CreationOutcome const raceOutcome = ReadRequest(*frog);
    EXPECT_FALSE(raceOutcome.Ok);
    EXPECT_NE(raceOutcome.Refusal.find("Frog"), std::string::npos) << raceOutcome.Refusal;
}

TEST(CreationInfoReaderTest, ANameIndexPastTheEndOfItsTableIsRefused)
{
    PropertyObjectPtr const info = Request(FireSchool, (200u << 16) | (1u << 8) | 1u);
    ASSERT_TRUE(info);

    CreationOutcome const outcome = ReadRequest(*info);
    EXPECT_FALSE(outcome.Ok);
    EXPECT_NE(outcome.Refusal.find("refused"), std::string::npos) << outcome.Refusal;
    EXPECT_EQ(outcome.Character.NameIndices, 0u);
}

TEST(CreationInfoReaderTest, TheSeventhWizardOnAnAccountAllowedSixIsRefused)
{
    PropertyObjectPtr const info = Request();
    ASSERT_TRUE(info);

    CreationLimits limits;
    limits.MaxPerAccount = 6;
    limits.ExistingCharacters = 6;
    CreationOutcome const outcome = ReadRequest(*info, limits);
    EXPECT_FALSE(outcome.Ok);
    EXPECT_NE(outcome.Refusal.find("already holds 6"), std::string::npos) << outcome.Refusal;

    limits.PurchasedSlots = 1;
    EXPECT_TRUE(ReadRequest(*info, limits).Ok) << "a purchased slot is a slot";
}

TEST(CreationInfoReaderTest, LoweringTheLimitRefusesTheNextRequestWithNothingRestarted)
{
    PropertyObjectPtr const info = Request();
    ASSERT_TRUE(info);

    CreationLimits limits;
    limits.MaxPerAccount = 6;
    limits.ExistingCharacters = 3;
    EXPECT_TRUE(ReadRequest(*info, limits).Ok);

    limits.MaxPerAccount = 3;
    CreationOutcome const after = ReadRequest(*info, limits);
    EXPECT_FALSE(after.Ok) << "the limit is read for every request, so a change takes hold at the next one";
    EXPECT_NE(after.Refusal.find("allowed 3"), std::string::npos) << after.Refusal;
}

TEST(CreationInfoReaderTest, ASchoolWithNowhereToStandIsRefused)
{
    PropertyObjectPtr const info = Request();
    ASSERT_TRUE(info);

    CreationOutcome const outcome = ReadRequest(*info, {}, {}, Rows(false));
    EXPECT_FALSE(outcome.Ok);
    EXPECT_NE(outcome.Refusal.find("nowhere to stand"), std::string::npos) << outcome.Refusal;
}

TEST(CreationInfoReaderTest, ANameOfItsOwnIsOnlyTakenWhenTheServerAllowsOne)
{
    PropertyObjectPtr const info = Request();
    ASSERT_TRUE(info);
    ASSERT_EQ(info->Set("m_name", std::u16string(u"Wolf")), PropertySetResult::Ok);

    CreationOutcome const refused = ReadRequest(*info);
    EXPECT_FALSE(refused.Ok);
    EXPECT_NE(refused.Refusal.find("name of its own"), std::string::npos) << refused.Refusal;

    CreationRules rules;
    rules.AllowCustomName = true;
    CreationOutcome const taken = ReadRequest(*info, {}, rules);
    ASSERT_TRUE(taken.Ok) << taken.Refusal;
    ASSERT_TRUE(taken.Character.CustomName.has_value());
    EXPECT_EQ(*taken.Character.CustomName, "Wolf");
}

TEST(CreationInfoReaderTest, AnAppearanceValueTooWideForItsPropertyCannotBeAskedFor)
{
    PropertyObjectPtr const behavior = Appearance();
    ASSERT_TRUE(behavior);

    EXPECT_EQ(behavior->Set("m_nHairModel", uint32{ 16 }), PropertySetResult::OutOfRange)
        << "m_nHairModel is four bits wide, so no client can encode a sixteenth model and no request can carry one";
    EXPECT_EQ(behavior->Set("m_nHeadHandsModel", uint32{ 4 }), PropertySetResult::OutOfRange);
    EXPECT_EQ(behavior->Set("m_nHairColor", uint32{ 128 }), PropertySetResult::OutOfRange);
    EXPECT_EQ(behavior->Set("m_nHairModel", uint32{ 15 }), PropertySetResult::Ok);
}

TEST(CreationInfoReaderTest, ABlobThatIsNotACreationInfoIsRefusedByTheDecoder)
{
    ObjectField const* const field = ObjectFields::Find("MSG_CREATECHARACTER", "CreationInfo");
    ASSERT_NE(field, nullptr);

    std::string const garbage(64, '\x7F');
    DecodeResult const nonsense = ObjectSerializer::DecodeField(Catalog(), *field,
        std::span<uint8 const>(reinterpret_cast<uint8 const*>(garbage.data()), garbage.size()));
    EXPECT_FALSE(nonsense.Ok()) << "a blob that names no class this server knows is refused rather than guessed at";

    PropertyObjectPtr const info = Request();
    ASSERT_TRUE(info);
    EncodeResult const encoded = ObjectSerializer::EncodeField(*field, info.get());
    ASSERT_EQ(encoded.Status, SerializerStatus::Ok) << encoded.Detail;
    ASSERT_FALSE(encoded.Bytes.empty());

    std::string cut(reinterpret_cast<char const*>(encoded.Bytes.data()), encoded.Bytes.size());
    cut.resize(cut.size() / 2);
    DecodeResult const truncated = ObjectSerializer::DecodeField(Catalog(), *field,
        std::span<uint8 const>(reinterpret_cast<uint8 const*>(cut.data()), cut.size()));
    EXPECT_FALSE(truncated.Ok()) << "half a request is not a request";

    DecodeResult const whole = ObjectSerializer::DecodeField(Catalog(), *field, encoded.Bytes);
    ASSERT_TRUE(whole.Ok()) << whole.Detail;
    ASSERT_TRUE(whole.Object);
    CreationOutcome const outcome = ReadRequest(*whole.Object);
    EXPECT_TRUE(outcome.Ok) << "a request that went over the wire and back is the same request: " << outcome.Refusal;
    EXPECT_EQ(outcome.Character.SchoolId, FireSchool);
}
