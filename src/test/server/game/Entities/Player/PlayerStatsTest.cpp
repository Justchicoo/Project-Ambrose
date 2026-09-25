/*
 * Project Ambrose by Imjustchico
 * Tests a wizard's stats: a new Fire wizard at level 1 gets its row's base health, mana and training points at full health and mana, one without a stats row at a higher level has earned every level's training points, stored values apply within their maximums and save back with a full health or mana as full, a level above the cap reads the cap's row, a wizard whose school has no rows or whose level is negative is refused with the reason, and the WizGameStats and ClientMagicSchoolBehavior the stats fill carry every value and read back unchanged through the transmit form.
 */

#include "CharacterTypeFixtures.h"
#include "ObjectSerializer.h"
#include "PlayerStats.h"
#include "PlayerStatsFixtures.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    using CharacterTypeFixtures::Detail::AddClass;
    using CharacterTypeFixtures::Detail::Json;
    using PlayerStatsFixtures::Fire;

    TypeCatalogPtr LoadCatalog()
    {
        Json dump = Json::parse(CharacterTypeFixtures::Dump());
        Json& classes = dump["classes"];
        AddClass(classes, "class WizGameStats", Json::array({ "PropertyClass" }), PlayerStatsFixtures::GameStatsProperties());
        AddClass(classes, "class ClientMagicSchoolBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), PlayerStatsFixtures::SchoolBehaviorProperties());
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(dump.dump(), "stats.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    class PlayerStatsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> errors;
            _levels = PlayerLevelSet::Build(PlayerStatsFixtures::FireLevels(5), errors);
            ASSERT_TRUE(_levels) << errors.front();
            _effects = StatEffectSet::Build({ { { "m_shadowPipMax", 2.0 } }, {}, {} }, errors);
            ASSERT_TRUE(_effects) << errors.front();
            _character.Guid = 7;
            _character.Account = 3;
            _character.SchoolId = Fire;
            _character.Level = 1;
            _character.Experience = 20;
        }

        std::optional<PlayerStats> Create(std::optional<CharacterStats> const& stored = std::nullopt)
        {
            std::string problem;
            std::optional<PlayerStats> stats = PlayerStats::Create(_character, stored, *_levels, *_effects, problem);
            EXPECT_TRUE(stats) << problem;
            return stats;
        }

        std::shared_ptr<PlayerLevelSet const> _levels;
        std::shared_ptr<StatEffectSet const> _effects;
        CharacterSummary _character;
    };
}

TEST_F(PlayerStatsTest, ANewFireWizardAtLevelOneTakesItsRowsBaseValuesAtFullHealthAndMana)
{
    std::optional<PlayerStats> const stats = Create();
    ASSERT_TRUE(stats);
    PlayerLevelInfo const* const row = _levels->GetInfo("Fire", 1);
    ASSERT_TRUE(row);
    EXPECT_EQ(stats->GetMaxHitpoints(), row->Hitpoints);
    EXPECT_EQ(stats->GetMaxHitpoints(), 415);
    EXPECT_EQ(stats->GetMaxMana(), row->Mana);
    EXPECT_EQ(stats->GetTrainingPoints(), row->TrainingPoints);
    EXPECT_EQ(stats->GetHitpoints(), row->Hitpoints);
    EXPECT_EQ(stats->GetMana(), row->Mana);
    EXPECT_EQ(stats->GetGold(), 0);
    EXPECT_EQ(stats->GetShadowPipMax(), 2);
    CharacterStats const saved = stats->ToStored();
    EXPECT_FALSE(saved.Health) << "a full wizard saves as full, not as a number";
    EXPECT_FALSE(saved.Mana);
    EXPECT_EQ(saved.TrainingPoints, 1);
}

TEST_F(PlayerStatsTest, AWizardWithoutAStatsRowHasEarnedEveryLevelsTrainingPoints)
{
    _character.Level = 4;
    std::optional<PlayerStats> const stats = Create();
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->GetTrainingPoints(), 2) << "levels 1 and 3 each give one";
    EXPECT_EQ(stats->GetMaxHitpoints(), 460);
    _character.Level = 9;
    std::optional<PlayerStats> const capped = Create();
    ASSERT_TRUE(capped);
    EXPECT_EQ(capped->GetMaxHitpoints(), _levels->GetInfo("Fire", 5)->Hitpoints) << "a level above the cap reads the cap's row, as the client's own lookup does";
    EXPECT_EQ(capped->GetTrainingPoints(), 3) << "levels past the cap earn nothing more";
}

TEST_F(PlayerStatsTest, StoredValuesApplyWithinTheirMaximumsAndSaveBack)
{
    _character.Level = 5;
    CharacterStats stored;
    stored.OverflowXp = 12;
    stored.SecondarySchoolId = 72777;
    stored.TrainingPoints = 7;
    stored.Gold = 1234;
    stored.Health = 100;
    stored.Mana = 5;
    stored.PotionCharge = 1.5f;
    stored.PotionMax = 2.0f;
    stored.ArenaPoints = 40;
    stored.LevelLocked = true;
    std::optional<PlayerStats> const stats = Create(stored);
    ASSERT_TRUE(stats);
    EXPECT_EQ(stats->GetHitpoints(), 100);
    EXPECT_EQ(stats->GetMana(), 5);
    EXPECT_EQ(stats->GetGold(), 1234);
    EXPECT_EQ(stats->GetTrainingPoints(), 7);
    EXPECT_EQ(stats->ToStored(), stored);

    stored.Health = 99999;
    stored.Mana = 99999;
    stored.Gold = 999999;
    std::optional<PlayerStats> const over = Create(stored);
    ASSERT_TRUE(over);
    EXPECT_EQ(over->GetHitpoints(), over->GetMaxHitpoints());
    EXPECT_EQ(over->GetMana(), over->GetMaxMana());
    EXPECT_EQ(over->GetGold(), 300000) << "gold stops at the pouch the wizard's level allows";
    EXPECT_FALSE(over->ToStored().Health);
    EXPECT_FALSE(over->ToStored().Mana);
}

TEST_F(PlayerStatsTest, AWizardWithoutALevelRowIsRefusedWithTheReason)
{
    std::string problem;
    _character.SchoolId = StringHash::KiStringHash("Ice");
    EXPECT_FALSE(PlayerStats::Create(_character, std::nullopt, *_levels, *_effects, problem));
    EXPECT_NE(problem.find("wizard 7's school"), std::string::npos) << problem;
    EXPECT_NE(problem.find("has no rows in player_level_stats"), std::string::npos) << problem;
    _character.SchoolId = Fire;
    _character.Level = -1;
    EXPECT_FALSE(PlayerStats::Create(_character, std::nullopt, *_levels, *_effects, problem));
    EXPECT_NE(problem.find("wizard 7 is at level -1"), std::string::npos) << problem;
    _character.Level = 1;
    EXPECT_FALSE(PlayerStats::Create(_character, std::nullopt, PlayerLevelSet(), *_effects, problem));
    EXPECT_NE(problem.find("run the extractor's levels command"), std::string::npos) << problem;
    std::optional<PlayerStats> const without = PlayerStats::Create(_character, std::nullopt, *_levels, StatEffectSet(), problem);
    ASSERT_TRUE(without) << problem;
    EXPECT_FALSE(without->GetShadowPipMax()) << "with no stat configuration the class's own default stays";
}

TEST_F(PlayerStatsTest, TheGameStatsAndSchoolBehaviorCarryEveryValueAndReadBackThroughTheTransmitForm)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    _character.Level = 5;
    _character.Experience = 1200;
    CharacterStats stored;
    stored.OverflowXp = 3;
    stored.SecondarySchoolId = 72777;
    stored.TrainingPoints = 2;
    stored.Gold = 1234;
    stored.Health = 300;
    stored.PotionCharge = 1.0f;
    stored.PotionMax = 2.0f;
    stored.ArenaPoints = 9;
    stored.LevelLocked = true;
    std::optional<PlayerStats> const stats = Create(stored);
    ASSERT_TRUE(stats);

    PropertyObjectPtr const gameStats = PropertyObject::Create(catalog, "class WizGameStats");
    PropertyObjectPtr const school = PropertyObject::Create(catalog, "class ClientMagicSchoolBehavior");
    ASSERT_TRUE(gameStats);
    ASSERT_TRUE(school);
    std::string problem;
    ASSERT_TRUE(stats->WriteGameStats(*gameStats, problem)) << problem;
    ASSERT_TRUE(stats->WriteSchool(*school, problem)) << problem;

    EXPECT_EQ(*gameStats->Get("m_baseHitpoints")->GetIf<int32>(), 475);
    EXPECT_EQ(*gameStats->Get("m_currentHitpoints")->GetIf<int32>(), 300);
    EXPECT_EQ(*gameStats->Get("m_baseMana")->GetIf<int32>(), 23);
    EXPECT_EQ(*gameStats->Get("m_currentMana")->GetIf<int32>(), 23);
    EXPECT_EQ(*gameStats->Get("m_baseGoldPouch")->GetIf<int32>(), 300000);
    EXPECT_EQ(*gameStats->Get("m_currentGold")->GetIf<int32>(), 1234);
    EXPECT_EQ(*gameStats->Get("m_energyMax")->GetIf<int32>(), 44);
    EXPECT_EQ(*gameStats->Get("m_currentArenaPoints")->GetIf<int32>(), 9);
    EXPECT_EQ(*gameStats->Get("m_potionMax")->GetIf<float>(), 2.0f);
    EXPECT_EQ(*gameStats->Get("m_potionCharge")->GetIf<float>(), 1.0f);
    EXPECT_EQ(*gameStats->Get("m_referenceLevel")->GetIf<int32>(), 5);
    EXPECT_EQ(*gameStats->Get("m_shadowPipMax")->GetIf<int32>(), 2);
    EXPECT_EQ(*gameStats->Get("m_shadowPipRating")->GetIf<float>(), 5.0f);
    EXPECT_EQ(*gameStats->Get("m_archmasteryBase")->GetIf<float>(), 40.0f);
    EXPECT_EQ(*gameStats->Get("m_schoolID")->GetIf<uint32>(), Fire);
    EXPECT_EQ(*gameStats->Get("m_secondarySchool")->GetIf<uint32>(), 72777u);
    EXPECT_EQ(*school->Get("m_schoolOfFocus")->GetIf<uint32>(), Fire);
    EXPECT_EQ(*school->Get("m_level")->GetIf<int32>(), 5);
    EXPECT_EQ(*school->Get("m_experiencePoints")->GetIf<int32>(), 1200);
    EXPECT_EQ(*school->Get("m_trainingPoints")->GetIf<int32>(), 2);
    EXPECT_EQ(*school->Get("m_overflowXP")->GetIf<int32>(), 3);
    EXPECT_EQ(*school->Get("m_levelLocked")->GetIf<int32>(), 1);
    EXPECT_EQ(*school->Get("m_secondarySchool")->GetIf<uint32>(), 72777u);

    for (PropertyObject const* const object : { gameStats.get(), school.get() })
    {
        EncodeResult const encoded = ObjectSerializer::Encode(object);
        ASSERT_TRUE(encoded.Ok()) << encoded.Detail;
        DecodeResult const decoded = ObjectSerializer::Decode(catalog, encoded.Bytes);
        ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
        ASSERT_TRUE(decoded.Object);
        for (PropertyInfo const& property : object->GetClass().Properties)
        {
            if (!property.HasFlag(PropertyFlag::Transmit))
                continue;
            PropertyValue const* const sent = object->Get(property.Name);
            PropertyValue const* const read = decoded.Object->Get(property.Name);
            ASSERT_TRUE(sent && read) << property.Name;
            EXPECT_TRUE(*sent == *read) << property.Name << " did not read back";
        }
    }
}
