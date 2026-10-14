/*
 * Project Ambrose by Imjustchico
 * Proves the loot manager's tables against seeded rolls: a 10% entry lands near 10% and the same seed rolls the same, a live Rate.Drop.Item of 2 doubles it without a reload while a guaranteed entry is never scaled, a group gives exactly one of its entries when their chances fill it, a school-gated spell goes only to that school, creature templates name their tables, and a generation with a bad row is refused whole while the serving one stays.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LootMgr.h"
#include "MemorySettingStore.h"
#include "ReloadMgr.h"
#include "RequirementMgr.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr int Rolls = 100000;

    class SchoolContext : public RequirementContext
    {
    public:
        explicit SchoolContext(std::string school) : _school(std::move(school)) { }

        std::optional<bool> HasSchoolOfFocus(std::string_view school) const override
        {
            return school == _school;
        }

    private:
        std::string _school;
    };

    LootEntryRow Entry(std::string table, uint32 position, LootType type, uint32 templateId, double chance, uint8 group = 0)
    {
        LootEntryRow entry;
        entry.TableId = std::move(table);
        entry.Position = position;
        entry.Type = type;
        entry.TemplateId = templateId;
        entry.Chance = chance;
        entry.Group = group;
        return entry;
    }

    class LootMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            sRequirementMgr.Clear();
            sLootMgr.RegisterReloadTargets();
            std::filesystem::path const file = _directory.Write("gameserver.conf", "Realm.Name = Test\n");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(file).Succeeded());
            sSettings.Clear();
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
        }

        void TearDown() override
        {
            sLootMgr.SetRowSource([] { return LootRows{}; });
            std::vector<std::string> errors;
            sLootMgr.Load(errors);
            sLootMgr.SetRowSource({});
            sRequirementMgr.Clear();
            sReloadMgr.Clear();
            sSettings.Clear();
        }

        static bool Load(LootRows rows, std::vector<std::string>& errors)
        {
            auto const source = std::make_shared<LootRows>(std::move(rows));
            sLootMgr.SetRowSource([source] { return *source; });
            return sLootMgr.Load(errors);
        }

        static int CountGiven(std::string_view table, uint32 templateId, std::mt19937_64& random)
        {
            RequirementContext const nobody;
            int given = 0;
            for (int roll = 0; roll < Rolls; ++roll)
                for (LootDrop const& drop : sLootMgr.Roll(table, nobody, random))
                    if (drop.TemplateId == templateId)
                        ++given;
            return given;
        }

        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
    };
}

TEST_F(LootMgrTest, ATenPercentEntryLandsNearTenPercentAndTheSameSeedRollsTheSame)
{
    LootRows rows;
    rows.Tables = { "LT-Test" };
    rows.Entries = { Entry("LT-Test", 0, LootType::Item, 1001, 0.1) };
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(std::move(rows), errors)) << (errors.empty() ? "" : errors.front());

    std::mt19937_64 first(20261014);
    int const given = CountGiven("LT-Test", 1001, first);
    EXPECT_GE(given, 9500);
    EXPECT_LE(given, 10500);

    std::mt19937_64 again(20261014);
    EXPECT_EQ(CountGiven("LT-Test", 1001, again), given) << "a seeded roll is reproducible";
}

TEST_F(LootMgrTest, RateDropItemOfTwoDoublesAnItemEntryWithoutAReloadAndNeverScalesAGuaranteedOne)
{
    LootRows rows;
    rows.Tables = { "LT-Test" };
    rows.Entries = { Entry("LT-Test", 0, LootType::Item, 1001, 0.1), Entry("LT-Test", 1, LootType::Spell, 2002, 0.1), Entry("LT-Test", 2, LootType::Item, 3003, 1.0) };
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(std::move(rows), errors)) << (errors.empty() ? "" : errors.front());

    SettingAuthor const author{ "test", 1, "unit_test" };
    ASSERT_TRUE(sSettings.Set("Rate.Drop.Item", "2", author, "double item drops").Ok());
    std::mt19937_64 random(7);
    int const items = CountGiven("LT-Test", 1001, random);
    EXPECT_GE(items, 19000);
    EXPECT_LE(items, 21000);
    std::mt19937_64 spells(7);
    int const spellsGiven = CountGiven("LT-Test", 2002, spells);
    EXPECT_GE(spellsGiven, 9500) << "Rate.Drop.Item leaves a spell entry alone";
    EXPECT_LE(spellsGiven, 10500);

    ASSERT_TRUE(sSettings.Set("Rate.Drop.Item", "0", author, "no item drops").Ok());
    std::mt19937_64 none(7);
    EXPECT_EQ(CountGiven("LT-Test", 1001, none), 0);
    std::mt19937_64 guaranteed(7);
    EXPECT_EQ(CountGiven("LT-Test", 3003, guaranteed), Rolls) << "a guaranteed entry is never scaled";
}

TEST_F(LootMgrTest, AGroupWhoseChancesFillItGivesExactlyOneEntry)
{
    LootRows rows;
    rows.Tables = { "LT-Group" };
    rows.Entries = { Entry("LT-Group", 0, LootType::Item, 1001, 0.25, 1), Entry("LT-Group", 1, LootType::Item, 1002, 0.75, 1), Entry("LT-Group", 2, LootType::Gold, 0, 1.0) };
    rows.Entries[2].MinCount = 10;
    rows.Entries[2].MaxCount = 20;
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(std::move(rows), errors)) << (errors.empty() ? "" : errors.front());

    std::mt19937_64 random(99);
    RequirementContext const nobody;
    int first = 0;
    for (int roll = 0; roll < Rolls; ++roll)
    {
        std::vector<LootDrop> const drops = sLootMgr.Roll("LT-Group", nobody, random);
        ASSERT_EQ(drops.size(), 2u);
        EXPECT_EQ(drops[0].Type, LootType::Gold);
        EXPECT_GE(drops[0].Count, 10u);
        EXPECT_LE(drops[0].Count, 20u);
        EXPECT_EQ(drops[1].Type, LootType::Item);
        if (drops[1].TemplateId == 1001)
            ++first;
    }
    EXPECT_GE(first, 24000);
    EXPECT_LE(first, 26000);
}

TEST_F(LootMgrTest, ASchoolGatedSpellGoesOnlyToThatSchool)
{
    RequirementRows requirements;
    requirements.Lists = { { "school-fire", "AND", false, std::nullopt } };
    RequirementRow fire;
    fire.ListId = "school-fire";
    fire.Type = "ReqSchoolOfFocus";
    fire.MagicSchool = "Fire";
    requirements.Requirements = { fire };
    auto const source = std::make_shared<RequirementRows>(requirements);
    sRequirementMgr.SetRowSource([source] { return *source; });
    std::vector<std::string> errors;
    ASSERT_TRUE(sRequirementMgr.Load(errors)) << (errors.empty() ? "" : errors.front());

    LootRows rows;
    rows.Tables = { "quest:Test" };
    rows.Entries = { Entry("quest:Test", 0, LootType::Spell, 7001, 1.0), Entry("quest:Test", 1, LootType::Gold, 0, 1.0) };
    rows.Entries[0].RequirementListId = "school-fire";
    ASSERT_TRUE(Load(std::move(rows), errors)) << (errors.empty() ? "" : errors.front());

    std::mt19937_64 random(1);
    std::vector<LootDrop> const toFire = sLootMgr.Roll("quest:Test", SchoolContext("Fire"), random);
    ASSERT_EQ(toFire.size(), 2u);
    EXPECT_EQ(toFire[0].Type, LootType::Spell);
    EXPECT_EQ(toFire[0].TemplateId, 7001u);
    std::vector<LootDrop> const toIce = sLootMgr.Roll("quest:Test", SchoolContext("Ice"), random);
    ASSERT_EQ(toIce.size(), 1u);
    EXPECT_EQ(toIce[0].Type, LootType::Gold);
}

TEST_F(LootMgrTest, CreatureTemplatesNameTheirTables)
{
    LootRows rows;
    rows.Tables = { "LT-A", "LT-B" };
    rows.Creatures = { { 500, "LT-A" }, { 500, "LT-B" }, { 600, "LT-B" } };
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(std::move(rows), errors)) << (errors.empty() ? "" : errors.front());
    EXPECT_EQ(sLootMgr.GetCreatureTables(500), (std::vector<std::string>{ "LT-A", "LT-B" }));
    EXPECT_EQ(sLootMgr.GetCreatureTables(600), (std::vector<std::string>{ "LT-B" }));
    EXPECT_TRUE(sLootMgr.GetCreatureTables(700).empty());
}

TEST_F(LootMgrTest, AGenerationWithABadRowIsRefusedWholeAndTheServingOneStays)
{
    LootRows good;
    good.Tables = { "LT-Good" };
    good.Entries = { Entry("LT-Good", 0, LootType::Item, 1001, 0.5) };
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(good, errors)) << (errors.empty() ? "" : errors.front());

    LootRows bad;
    bad.Tables = { "LT-Bad", "LT-Bad" };
    bad.Entries = { Entry("LT-Missing", 0, LootType::Item, 1001, 0.5), Entry("LT-Bad", 0, LootType::Item, 1001, 0.0), Entry("LT-Bad", 1, LootType::Spell, 0, 0.5),
        Entry("LT-Bad", 2, LootType::Item, 1001, 1.5) };
    bad.Entries[2].MinCount = 3;
    bad.Entries[2].MaxCount = 2;
    bad.Creatures = { { 500, "LT-Nowhere" } };
    errors.clear();
    EXPECT_FALSE(Load(bad, errors));
    EXPECT_EQ(errors.size(), 7u);
    EXPECT_TRUE(sLootMgr.HasTable("LT-Good")) << "the serving generation stays";
    EXPECT_FALSE(sLootMgr.HasTable("LT-Bad"));

    LootRows fixed;
    fixed.Tables = { "LT-Fixed" };
    sLootMgr.SetRowSource([fixed] { return fixed; });
    EXPECT_TRUE(sReloadMgr.Reload(LootMgr::Target).Ok) << "reload loot builds the tables again";
    EXPECT_TRUE(sLootMgr.HasTable("LT-Fixed"));
    EXPECT_FALSE(sLootMgr.HasTable("LT-Good"));
}

TEST_F(LootMgrTest, TypesReadAndWriteByTheirDatabaseNames)
{
    for (std::string_view const name : { "GOLD", "XP", "ITEM", "SPELL", "REAGENT", "TREASURE_CARD" })
    {
        std::optional<LootType> const type = LootMgr::ParseType(name);
        ASSERT_TRUE(type) << name;
        EXPECT_EQ(LootMgr::TypeName(*type), name);
    }
    EXPECT_FALSE(LootMgr::ParseType("BADGE"));
}
