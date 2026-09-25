/*
 * Project Ambrose by Imjustchico
 * Fills the user's own r806919 WizGameStats and ClientMagicSchoolBehavior from a level 5 Fire wizard's stats, when AMBROSE_TYPE_DUMP_PATH names the install's type dump, and checks that every property the client is sent reads back unchanged through the transmit form, the ones the stats set among them, so the object MSG_LOGINCOMPLETE carries holds what the stats hold.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "ObjectSerializer.h"
#include "PlayerStats.h"
#include "PlayerStatsFixtures.h"
#include "TypeRegistry.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    class PlayerStatsClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!dump || dump->empty())
                return;
            s_registry = std::make_unique<TypeRegistry>();
            ASSERT_TRUE(s_registry->LoadFromFile(LogConfig::Utf8Path(*dump)));
        }

        static void TearDownTestSuite()
        {
            s_registry.reset();
        }

        void SetUp() override
        {
            if (!s_registry)
                GTEST_SKIP() << "AMBROSE_TYPE_DUMP_PATH is not set";
        }

        static inline std::unique_ptr<TypeRegistry> s_registry;
    };
}

TEST_F(PlayerStatsClientTest, TheClientsOwnStatsClassesCarryEveryValueThroughTheTransmitForm)
{
    TypeCatalogPtr const catalog = s_registry->GetCatalog();
    std::vector<std::string> errors;
    std::shared_ptr<PlayerLevelSet const> const levels = PlayerLevelSet::Build(PlayerStatsFixtures::FireLevels(5), errors);
    ASSERT_TRUE(levels) << errors.front();
    std::shared_ptr<StatEffectSet const> const effects = StatEffectSet::Build({ { { "m_shadowPipMax", 2.0 } }, {}, {} }, errors);
    ASSERT_TRUE(effects) << errors.front();
    CharacterSummary wizard;
    wizard.Guid = 7;
    wizard.SchoolId = PlayerStatsFixtures::Fire;
    wizard.Level = 5;
    wizard.Experience = 900;
    CharacterStats stored;
    stored.Gold = 1234;
    stored.Health = 300;
    stored.Mana = 10;
    stored.TrainingPoints = 2;
    stored.SecondarySchoolId = 72777;
    stored.LevelLocked = true;
    std::string problem;
    std::optional<PlayerStats> const stats = PlayerStats::Create(wizard, stored, *levels, *effects, problem);
    ASSERT_TRUE(stats) << problem;

    PropertyObjectPtr const gameStats = PropertyObject::Create(catalog, "class WizGameStats");
    PropertyObjectPtr const school = PropertyObject::Create(catalog, "class ClientMagicSchoolBehavior");
    ASSERT_TRUE(gameStats) << "the type dump lists class WizGameStats";
    ASSERT_TRUE(school) << "the type dump lists class ClientMagicSchoolBehavior";
    ASSERT_TRUE(stats->WriteGameStats(*gameStats, problem)) << problem;
    ASSERT_TRUE(stats->WriteSchool(*school, problem)) << problem;
    EXPECT_EQ(*gameStats->Get("m_currentGold")->GetIf<int32>(), 1234);
    EXPECT_EQ(*gameStats->Get("m_baseHitpoints")->GetIf<int32>(), 475);
    EXPECT_EQ(*gameStats->Get("m_currentHitpoints")->GetIf<int32>(), 300);
    EXPECT_EQ(*school->Get("m_level")->GetIf<int32>(), 5);

    for (PropertyObject const* const object : { gameStats.get(), school.get() })
    {
        EncodeResult const encoded = ObjectSerializer::Encode(object);
        ASSERT_TRUE(encoded.Ok()) << object->GetClass().Name << ": " << encoded.Detail;
        DecodeResult const decoded = ObjectSerializer::Decode(catalog, encoded.Bytes);
        ASSERT_TRUE(decoded.Ok()) << object->GetClass().Name << ": " << decoded.Detail;
        ASSERT_TRUE(decoded.Object);
        std::size_t compared = 0;
        for (PropertyInfo const& property : object->GetClass().Properties)
        {
            if (!property.HasFlag(PropertyFlag::Transmit) && !property.HasFlag(PropertyFlag::AuthorityTransmit))
                continue;
            PropertyValue const* const sent = object->Get(property.Name);
            PropertyValue const* const read = decoded.Object->Get(property.Name);
            ASSERT_TRUE(sent && read) << property.Name;
            EXPECT_TRUE(*sent == *read) << object->GetClass().Name << " " << property.Name << " did not read back";
            ++compared;
        }
        EXPECT_GT(compared, object == gameStats.get() ? 100u : 7u) << "every transmitted property is compared, not a handful";
    }
}
