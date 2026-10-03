/*
 * Project Ambrose by Imjustchico
 * Extracts the level, school and stat tables of the user's own install, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it, with the counts and values recorded for the installed revision, r806919's below: the seven wizard schools have a row for every level from 0 to the cap of 180 and the nine other magic schools have none, sixteen school templates come out with Fire's five weaving badges, Fire's level 1 takes its own 415 hitpoints and the shared table's mana, gold pouch, energy and experience, the experience totals rise to the cap, the settings, factors, mob ranks and bands match the install's files, and with AMBROSE_TEST_DB set the rows fill a new world database that the level manager loads.
 */

#include "DBUpdater.h"
#include "TestDatabaseUpdates.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "InstalledRevision.h"
#include "KiwadArchive.h"
#include "LevelExtractor.h"
#include "LevelScript.h"
#include "LevelViews.h"
#include "LogConfig.h"
#include "PlayerLevelMgr.h"
#include "StringHash.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    class LevelExtractorClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                return;
            std::string error;
            std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(LogConfig::Utf8Path(*client) / "Data" / "GameData" / "Root.wad", error);
            ASSERT_TRUE(archive) << error;
            s_views = std::make_unique<TypedViewRegistry>();
            LevelViews::RegisterAll(*s_views);
            s_registry = std::make_unique<TypeRegistry>(s_views.get());
            ASSERT_TRUE(s_registry->LoadFromFile(LogConfig::Utf8Path(*dump)));
            s_extraction = std::make_unique<LevelExtraction>(LevelExtractor::Extract(*archive, s_registry->GetCatalog()));
        }

        static void TearDownTestSuite()
        {
            s_extraction.reset();
            s_registry.reset();
            s_views.reset();
        }

        void SetUp() override
        {
            if (!s_extraction)
                GTEST_SKIP() << "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            ASSERT_TRUE(s_extraction->Ok()) << s_extraction->Errors.front();
        }

        static inline std::unique_ptr<TypedViewRegistry> s_views;
        static inline std::unique_ptr<TypeRegistry> s_registry;
        static inline std::unique_ptr<LevelExtraction> s_extraction;
    };
}

TEST_F(LevelExtractorClientTest, EveryWizardSchoolHasEveryLevelUpToTheCap)
{
    LevelExtraction const& extraction = *s_extraction;
    if (InstalledRevision::Is("r806919"))
    {
        EXPECT_EQ(extraction.SchoolsWithTables, (std::vector<std::string>{ "Fire", "Ice", "Storm", "Life", "Myth", "Death", "Balance" }));
        EXPECT_EQ(extraction.SchoolsWithoutTables, (std::vector<std::string>{ "Gardening", "Moon", "Star", "Sun", "Shadow", "Cantrips", "CastleMagic", "Fishing", "WhirlyBurly" }));
    }
    InstalledRevision::Expect(extraction.Levels.Levels.size(), { { "r806919", 7u * 181u } }, "level rows");
    InstalledRevision::Expect(extraction.Levels.Schools.size(), { { "r806919", 16u } }, "schools");

    std::vector<std::string> errors;
    std::shared_ptr<PlayerLevelSet const> const set = PlayerLevelSet::Build(extraction.Levels, errors);
    ASSERT_TRUE(set) << errors.front();
    InstalledRevision::Expect(set->GetMaxLevel(), { { "r806919", 180u } }, "level cap");
    InstalledRevision::Expect(set->GetLevelTableCount(), { { "r806919", 7u } }, "level tables");

    PlayerLevelInfo const* const fire = set->GetInfo("Fire", 1);
    ASSERT_TRUE(fire);
    if (InstalledRevision::Is("r806919"))
    {
        EXPECT_EQ(fire->Hitpoints, 415);
        EXPECT_EQ(fire->Mana, 15);
        EXPECT_EQ(fire->Gold, 300000);
        EXPECT_EQ(fire->PetEnergy, 40);
        EXPECT_EQ(fire->XpToLevel, 45);
        EXPECT_EQ(fire->LevelName, "Levels_Level1");
        EXPECT_EQ(set->GetInfo("Fire", 180)->PipConversion[0], 434);
        EXPECT_EQ(set->GetInfo("Fire", 180)->PipConversion[1], 0);
        EXPECT_EQ(set->GetInfo("Fire", 180)->XpToLevel, 149660950);
    }
    for (std::string_view const school : PlayerLevelSet::PipSchools)
    {
        ASSERT_TRUE(set->FindSchool(school)) << school;
        int32 previous = -1;
        for (int64 level = 0; level <= int64{ set->GetMaxLevel() }; ++level)
        {
            PlayerLevelInfo const* const row = set->GetInfo(school, level);
            ASSERT_TRUE(row) << school << " " << level;
            EXPECT_GT(row->XpToLevel, previous) << school << " " << level;
            previous = row->XpToLevel;
        }
    }
}

TEST_F(LevelExtractorClientTest, SchoolsSettingsAndBandsMatchTheInstall)
{
    LevelExtraction const& extraction = *s_extraction;
    std::vector<std::string> errors;
    std::shared_ptr<PlayerLevelSet const> const set = PlayerLevelSet::Build(extraction.Levels, errors);
    ASSERT_TRUE(set) << errors.front();
    MagicSchool const* const fire = set->FindSchool("Fire");
    ASSERT_TRUE(fire);
    EXPECT_EQ(fire->Id, StringHash::KiStringHash("Fire"));
    ASSERT_TRUE(set->FindSchool("Shadow"));
    std::shared_ptr<StatEffectSet const> const stats = StatEffectSet::Build(extraction.Stats, errors);
    ASSERT_TRUE(stats) << errors.front();
    InstalledRevision::Expect(extraction.Stats.Settings.size(), { { "r806919", 39u } }, "stat settings");
    InstalledRevision::Expect(extraction.Stats.PipConversion.size(), { { "r806919", 7u } }, "pip conversions");
    ASSERT_FALSE(extraction.Stats.CritAndBlock.empty());
    EXPECT_EQ(extraction.Stats.CritAndBlock.front().MinLevel, 0);
    if (!InstalledRevision::Is("r806919"))
        return;

    EXPECT_EQ(fire->MinLevel, 1u);
    EXPECT_EQ(fire->Index, 1);
    EXPECT_EQ(fire->Badges, (std::vector<std::string>{ "FireWeaving-Level01", "FireWeaving-Level02", "FireWeaving-Level03", "FireWeaving-Level04", "FireWeaving-Level05" }));
    EXPECT_EQ(set->FindSchool("Shadow")->MinLevel, 2u);
    EXPECT_EQ(set->FindSchool("CastleMagic")->Index, 15);

    EXPECT_EQ(set->GetXpSetting("m_maxSchoolLevel"), 180.0);
    EXPECT_EQ(set->GetXpSetting("m_experienceBonus"), 3.0);
    EXPECT_EQ(set->GetXpSetting("m_schoolOfFocusBonus"), 1.0);
    EXPECT_EQ(extraction.Levels.EncounterXpFactors, (std::vector<float>{ 1.0f, 0.5f, 0.0f }));
    EXPECT_EQ(set->GetLevelForRank(2), 6);
    EXPECT_EQ(set->GetLevelForRank(3), 11);

    EXPECT_EQ(stats->Get("m_shadowPipMax"), 2.0);
    EXPECT_EQ(stats->Get("m_criticalHitLevelThreshold"), 50.0);
    EXPECT_EQ(stats->Get("m_criticalDamageAddPercentPvP"), double{ 0.3f });
    EXPECT_EQ(stats->Get("m_blockScalarDivisorPvP"), 12.0);
    ASSERT_GE(extraction.Stats.PipConversion.size(), 2u);
    EXPECT_EQ(extraction.Stats.PipConversion[1].CapValue, 0.33f);
    EXPECT_EQ(extraction.Stats.CritAndBlock.back().MinLevel, 140);
    EXPECT_EQ(extraction.Stats.CritAndBlock.back().Position, 6u);
}

TEST_F(LevelExtractorClientTest, TheRowsFillAWorldDatabaseTheManagerLoads)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> server = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(server);
    MySQLConnectionInfo world = *server;
    world.Database = fmt::format("ambrose_client_levels_{:08x}", std::random_device()());
    server->Database.clear();
    struct Cleanup
    {
        MySQLConnectionInfo Server;
        std::string Name;
        ~Cleanup()
        {
            WorldDatabase.Close();
            MySQLConnection connection(Server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(Name)));
        }
    } const cleanup{ *server, world.Database };

    ASSERT_TRUE(AmbroseTestDatabase::RunWorldUpdates(world));
    std::string error;
    ASSERT_TRUE(LevelScript::Build(*s_extraction).Apply(world, error)) << error;
    ASSERT_TRUE(WorldDatabase.SetConnectionInfo(world.ToConnectionString(), 1, 1));
    ASSERT_EQ(WorldDatabase.Open(), 0u);
    PlayerLevelMgr manager;
    PlayerLevelLoadResult const loaded = manager.Load();
    ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
    EXPECT_EQ(loaded.Schools, s_extraction->Levels.Schools.size());
    EXPECT_EQ(loaded.LevelTables, s_extraction->SchoolsWithTables.size());
    InstalledRevision::Expect(loaded.MaxLevel, { { "r806919", 180u } }, "level cap");
    EXPECT_EQ(loaded.Levels, s_extraction->Levels.Levels.size());
    EXPECT_EQ(loaded.StatSettings, s_extraction->Stats.Settings.size());
    if (InstalledRevision::Is("r806919"))
    {
        EXPECT_EQ(manager.GetLevels()->GetInfo("Fire", 1)->Hitpoints, 415);
    }
    EXPECT_EQ(manager.GetStats()->GetData().CritAndBlock, s_extraction->Stats.CritAndBlock);
    WorldDatabase.Close();
}
