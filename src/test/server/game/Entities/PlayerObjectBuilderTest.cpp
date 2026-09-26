/*
 * Project Ambrose by Imjustchico
 * Tests the object a wizard stands in the world as, on classes the test lays out the way the client's are: it opens with the pair the core object table gives WizClientObject and the player's template id, carries the wizard's id, place, facing and mobile id, holds one behavior for each the template names in the template's order with an empty slot where the client takes one or the template itself leaves one, fills the look and name from the stored wizard, the school behavior and stats from its stats and the spellbook with a tracker for each spell it knows in the order it learned them, and reads back equal through the CoreObject form; a behavior nothing maps, and a template never read, are refused rather than guessed.
 */

#include "CharacterTypeFixtures.h"
#include "CoreObjectSerializer.h"
#include "ObjectSchemaMgr.h"
#include "PlayerObjectBuilder.h"
#include "PlayerStatsFixtures.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using CharacterTypeFixtures::Detail::AddClass;
    using CharacterTypeFixtures::Detail::Json;
    using CharacterTypeFixtures::Detail::Property;

    constexpr uint32 Wire = 0x1F;
    constexpr uint32 Local = 0x27;
    constexpr uint32 Spellbook = 0x1B;

    std::vector<std::pair<std::string, Json>> ObjectProperties()
    {
        return {
            { "m_inactiveBehaviors", Property("class SharedPointer<class BehaviorInstance>", "m_inactiveBehaviors", 0, Wire, "List") },
            { "m_globalID.m_full", Property("unsigned __int64", "m_globalID.m_full", 1, Wire) },
            { "m_permID", Property("unsigned __int64", "m_permID", 2, Wire) },
            { "m_location", Property("class Vector3D", "m_location", 3, Wire) },
            { "m_orientation", Property("class Vector3D", "m_orientation", 4, Wire) },
            { "m_fScale", Property("float", "m_fScale", 5, Wire) },
            { "m_templateID.m_full", Property("unsigned __int64", "m_templateID.m_full", 6, Wire) },
            { "m_nMobileID", Property("unsigned short", "m_nMobileID", 7, Wire) } };
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json dump = Json::parse(CharacterTypeFixtures::Dump());
        Json& classes = dump["classes"];
        AddClass(classes, "class Vector3D", Json::array(), {});
        AddClass(classes, "class CoreObject", Json::array({ "PropertyClass" }), ObjectProperties());
        std::vector<std::pair<std::string, Json>> client = ObjectProperties();
        client.emplace_back("m_characterId", Property("gid", "m_characterId", 8, Wire));
        AddClass(classes, "class ClientObject", Json::array({ "CoreObject", "PropertyClass" }), client);
        std::vector<std::pair<std::string, Json>> player = client;
        player.emplace_back("m_gameStats", Property("class WizGameStats*", "m_gameStats", 9, Wire));
        AddClass(classes, "class WizClientObject", Json::array({ "ClientObject", "CoreObject", "PropertyClass" }), player);
        AddClass(classes, "class WizGameStats", Json::array({ "PropertyClass" }), PlayerStatsFixtures::GameStatsProperties());

        Json gender = Property("enum eGender", "m_eGender", 3, 0x20001F);
        gender["enum_options"] = Json{ { "Male", 1 }, { "Female", 0 }, { "Neutral", 2 } };
        Json race = Property("enum eRace", "m_eRace", 4, 0x20001F);
        race["enum_options"] = Json{ { "Human", 79806088 }, { "Frog", 2274918 } };
        AddClass(classes, "class ClientWizPlayerNameBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), {
            { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, Local) },
            { "m_wsNameOverride", Property("std::wstring", "m_wsNameOverride", 1, Wire) },
            { "m_nameKeys", Property("unsigned int", "m_nameKeys", 2, Wire) },
            { "m_eGender", gender },
            { "m_eRace", race } });
        AddClass(classes, "class ClientMagicSchoolBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), PlayerStatsFixtures::SchoolBehaviorProperties());
        AddClass(classes, "class SpellIDTracker", Json::array({ "PropertyClass" }), {
            { "m_spellID", Property("unsigned int", "m_spellID", 0, Wire) },
            { "m_isRetired", Property("bool", "m_isRetired", 1, Wire) },
            { "m_tieredSpellGroupIndex", Property("int", "m_tieredSpellGroupIndex", 2, Wire) } });
        AddClass(classes, "class ClientSpellbookBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), {
            { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, Local) },
            { "m_spellIDList", Property("class SharedPointer<class SpellIDTracker>", "m_spellIDList", 1, Spellbook, "List") } });
        AddClass(classes, "TestMobileBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), {
            { "m_behaviorTemplateNameID", Property("unsigned int", "m_behaviorTemplateNameID", 0, Local) } });

        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(dump.dump(), "player.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    class PlayerObjectBuilderTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _catalog = LoadCatalog();
            ASSERT_TRUE(_catalog);
            std::vector<std::string> errors;
            _types = CoreObjectTypeTable::Build({ { 104, 2, "class WizClientObject" } }, *_catalog, errors);
            ASSERT_TRUE(_types) << (errors.empty() ? std::string() : errors.front());
            _behaviors = BehaviorClientClasses::Build({
                { "WizardCharacterBehavior", "class WizardCharacterBehavior", 0 },
                { "BasicMobileBehavior", "TestMobileBehavior", 0 },
                { "PathMovementBehavior", std::nullopt, 0 },
                { "WizPlayerNameBehavior", "class ClientWizPlayerNameBehavior", 0 },
                { "BasicMagicSchoolBehavior", "class ClientMagicSchoolBehavior", 0 },
                { "BasicSpellbookBehavior", "class ClientSpellbookBehavior", 0 } }, *_catalog, errors);
            ASSERT_TRUE(_behaviors) << (errors.empty() ? std::string() : errors.front());
            _template.TemplateId = 1;
            _template.File = "ObjectData/PlayerObject.xml";
            _template.Behaviors = { "WizardCharacterBehavior", "BasicMobileBehavior", "PathMovementBehavior", "WizPlayerNameBehavior", "BasicMagicSchoolBehavior",
                "BasicSpellbookBehavior" };
            _spells = { { 103007158, false, 4 }, { 957065192, false, SpellTracker::NoGroup }, { 402787217, true, 4 } };

            _character.Guid = 7;
            _character.Account = 3;
            _character.NameIndices = (3u << 24) | (100u << 16) | (248u << 8) | 27u;
            _character.SchoolId = 2343174;
            _character.Level = 4;
            _character.Experience = 125;
            _character.Zone = "WizardCity/WC_Ravenwood";
            _character.Appearance.Gender = 1;
            _character.Appearance.Race = 79806088;
            _character.Appearance.HairModel = 7;
            _character.Appearance.HairColor = 75;

            _placement.X = -32.0f;
            _placement.Y = -552.0f;
            _placement.Z = -28.0f;
            _placement.Yaw = 1.5f;
            _placement.MobileId = 3277;

            _levels = PlayerLevelSet::Build(PlayerStatsFixtures::FireLevels(5), errors);
            ASSERT_TRUE(_levels) << (errors.empty() ? std::string() : errors.front());
            std::string problem;
            _stats = PlayerStats::Create(_character, std::nullopt, *_levels, StatEffectSet(), problem);
            ASSERT_TRUE(_stats) << problem;
        }

        TypeCatalogPtr _catalog;
        CoreObjectTypeTablePtr _types;
        std::shared_ptr<BehaviorClientClasses const> _behaviors;
        ObjectTemplate _template;
        CharacterSummary _character;
        PlayerPlacement _placement;
        std::shared_ptr<PlayerLevelSet const> _levels;
        std::optional<PlayerStats> _stats;
        std::vector<SpellTracker> _spells;
    };
}

TEST_F(PlayerObjectBuilderTest, TheWizardsObjectCarriesItsHeaderIdsPlaceAndBehaviorsInTemplateOrder)
{
    std::string problem;
    PropertyObjectPtr const player = PlayerObjectBuilder::Build(_catalog, *_types, *_behaviors, _template, _character, *_stats, _spells, _placement, problem);
    ASSERT_TRUE(player) << problem;
    ASSERT_TRUE(player->GetCoreHeader());
    EXPECT_EQ(*player->GetCoreHeader(), (CoreObjectHeader{ 104, 2, 1 }));
    EXPECT_EQ(*player->Get("m_globalID.m_full")->GetIf<uint64>(), 7u);
    EXPECT_EQ(*player->Get("m_characterId")->GetIf<uint64>(), 7u);
    EXPECT_EQ(*player->Get("m_templateID.m_full")->GetIf<uint64>(), 1u);
    EXPECT_EQ(*player->Get("m_nMobileID")->GetIf<uint16>(), 3277u);
    EXPECT_FLOAT_EQ(player->Get("m_fScale")->GetIf<float>() ? *player->Get("m_fScale")->GetIf<float>() : 0.0f, 1.0f);
    PropertyTypes::Vector3D const* const location = player->Get("m_location")->GetIf<PropertyTypes::Vector3D>();
    ASSERT_NE(location, nullptr);
    EXPECT_FLOAT_EQ(location->X, -32.0f);
    EXPECT_FLOAT_EQ(location->Z, -28.0f);
    PropertyTypes::Vector3D const* const facing = player->Get("m_orientation")->GetIf<PropertyTypes::Vector3D>();
    ASSERT_NE(facing, nullptr);
    EXPECT_FLOAT_EQ(facing->Z, 1.5f) << "the yaw sits in the third component, as the client's own player object carries it";

    PropertyValue::List const& behaviors = *player->Get("m_inactiveBehaviors")->GetList();
    ASSERT_EQ(behaviors.size(), 6u);
    ASSERT_NE(behaviors[0].AsObject(), nullptr);
    EXPECT_TRUE(behaviors[0].AsObject()->IsA("class WizardCharacterBehavior"));
    EXPECT_EQ(*behaviors[0].AsObject()->Get("m_nHairModel")->GetIf<uint32>(), 7u);
    ASSERT_NE(behaviors[1].AsObject(), nullptr);
    EXPECT_TRUE(behaviors[1].AsObject()->IsA("TestMobileBehavior"));
    EXPECT_TRUE(behaviors[2].IsNullObject()) << "the client takes the path movement slot empty";
    ASSERT_NE(behaviors[3].AsObject(), nullptr);
    EXPECT_EQ(*behaviors[3].AsObject()->Get("m_nameKeys")->GetIf<uint32>(), _character.NameIndices);
    ASSERT_NE(behaviors[4].AsObject(), nullptr);
    EXPECT_EQ(*behaviors[4].AsObject()->Get("m_schoolOfFocus")->GetIf<uint32>(), 2343174u);
    EXPECT_EQ(*behaviors[4].AsObject()->Get("m_level")->GetIf<int32>(), 4);
    EXPECT_EQ(*behaviors[4].AsObject()->Get("m_experiencePoints")->GetIf<int32>(), 125);

    PropertyObject const* const stats = player->Get("m_gameStats")->AsObject();
    ASSERT_NE(stats, nullptr);
    EXPECT_EQ(*stats->Get("m_highestCharacterLevelOnAccount")->GetIf<int32>(), 4);
    EXPECT_EQ(*stats->Get("m_baseHitpoints")->GetIf<int32>(), 460);
    EXPECT_EQ(*stats->Get("m_currentHitpoints")->GetIf<int32>(), 460);
    EXPECT_EQ(*stats->Get("m_schoolID")->GetIf<uint32>(), 2343174u);
    EXPECT_EQ(*behaviors[4].AsObject()->Get("m_trainingPoints")->GetIf<int32>(), 2);

    ASSERT_NE(behaviors[5].AsObject(), nullptr);
    EXPECT_TRUE(behaviors[5].AsObject()->IsA("class ClientSpellbookBehavior"));
    PropertyValue::List const& trackers = *behaviors[5].AsObject()->Get("m_spellIDList")->GetList();
    ASSERT_EQ(trackers.size(), 3u) << "one tracker for each spell the wizard knows";
    std::vector<SpellTracker> read;
    for (PropertyValue const& entry : trackers)
    {
        PropertyObject const* const tracker = entry.AsObject();
        ASSERT_NE(tracker, nullptr);
        read.push_back({ *tracker->Get("m_spellID")->GetIf<uint32>(), *tracker->Get("m_isRetired")->GetIf<bool>(), *tracker->Get("m_tieredSpellGroupIndex")->GetIf<int32>() });
    }
    EXPECT_EQ(read, _spells) << "in the order the wizard learned them, each retired and grouped as its tracker says";
}

TEST_F(PlayerObjectBuilderTest, AWizardThatKnowsNoSpellCarriesAnEmptySpellbook)
{
    std::string problem;
    PropertyObjectPtr const player = PlayerObjectBuilder::Build(_catalog, *_types, *_behaviors, _template, _character, *_stats, {}, _placement, problem);
    ASSERT_TRUE(player) << problem;
    PropertyValue::List const& behaviors = *player->Get("m_inactiveBehaviors")->GetList();
    ASSERT_NE(behaviors[5].AsObject(), nullptr);
    EXPECT_TRUE(behaviors[5].AsObject()->Get("m_spellIDList")->GetList()->empty());
}

TEST_F(PlayerObjectBuilderTest, ASlotTheTemplateLeavesEmptyStaysEmpty)
{
    std::string problem;
    ObjectTemplate withEmpty = _template;
    withEmpty.Behaviors.insert(withEmpty.Behaviors.begin() + 1, std::string());
    PropertyObjectPtr const player = PlayerObjectBuilder::Build(_catalog, *_types, *_behaviors, withEmpty, _character, *_stats, _spells, _placement, problem);
    ASSERT_TRUE(player) << problem;
    PropertyValue::List const& behaviors = *player->Get("m_inactiveBehaviors")->GetList();
    ASSERT_EQ(behaviors.size(), 7u);
    EXPECT_TRUE(behaviors[1].IsNullObject()) << "the empty slot is kept, so every later behavior stays at its position";
    ASSERT_NE(behaviors[2].AsObject(), nullptr);
    EXPECT_TRUE(behaviors[2].AsObject()->IsA("TestMobileBehavior"));
}

TEST_F(PlayerObjectBuilderTest, TheObjectReadsBackEqualThroughTheCoreObjectForm)
{
    std::string problem;
    PropertyObjectPtr const player = PlayerObjectBuilder::Build(_catalog, *_types, *_behaviors, _template, _character, *_stats, _spells, _placement, problem);
    ASSERT_TRUE(player) << problem;
    EncodeResult const encoded = CoreObjectSerializer::Encode(*player, *_types);
    ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
    ASSERT_GE(encoded.Bytes.size(), 6u);
    EXPECT_EQ(std::vector<uint8>(encoded.Bytes.begin(), encoded.Bytes.begin() + 6), (std::vector<uint8>{ 0x68, 0x02, 0x01, 0x00, 0x00, 0x00 }));
    DecodeResult const decoded = CoreObjectSerializer::Decode(_catalog, encoded.Bytes, *_types);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(decoded.BytesRead, encoded.Bytes.size());
    EXPECT_EQ(encoded.Bytes, CoreObjectSerializer::Encode(*decoded.Object, *_types).Bytes) << "what is read back writes the same bytes again";
}

TEST_F(PlayerObjectBuilderTest, ABehaviorNothingMapsAndATemplateNeverReadAreRefused)
{
    std::string problem;
    ObjectTemplate unmapped = _template;
    unmapped.Behaviors.insert(unmapped.Behaviors.begin() + 2, "LadderBehavior");
    EXPECT_FALSE(PlayerObjectBuilder::Build(_catalog, *_types, *_behaviors, unmapped, _character, *_stats, _spells, _placement, problem));
    EXPECT_NE(problem.find("names behavior LadderBehavior, which behavior_client_class does not list"), std::string::npos) << problem;

    EXPECT_FALSE(PlayerObjectBuilder::Build(_catalog, *_types, *_behaviors, ObjectTemplate(), _character, *_stats, _spells, _placement, problem));
    EXPECT_NE(problem.find("has not been read from the install"), std::string::npos) << problem;

    EXPECT_FALSE(PlayerObjectBuilder::Build(_catalog, CoreObjectTypeTable(), *_behaviors, _template, _character, *_stats, _spells, _placement, problem));
    EXPECT_NE(problem.find("core_object_type gives class WizClientObject no block and type"), std::string::npos) << problem;
}
