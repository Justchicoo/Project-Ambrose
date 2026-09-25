/*
 * Project Ambrose by Imjustchico
 * Tests the level extractor on a Root.wad the test builds with a BINd MagicXPConfig, school templates and a WizStatisticEffectConfig encoded through a type dump it writes: each school's rows take its own values where its table sets them and the shared table's otherwise, a school without a table is named but gives no rows, the settings, encounter factors, mob ranks, badges and bands all come out in order, the SQL script replaces the nine tables with floats that read back unchanged, each kind of broken input is reported, and with AMBROSE_TEST_DB set the script applies twice to a new world database and loads in the level manager with the rows it extracted.
 */

#include "BindFile.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LevelExtractor.h"
#include "LevelScript.h"
#include "LevelViews.h"
#include "LogTestDirectory.h"
#include "PlayerLevelMgr.h"
#include "StringHash.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 1 | 2 | 4;

    struct Level
    {
        int32 Number = 0;
        int32 Xp = 0;
        int32 Hitpoints = 0;
        int32 Mana = 0;
        int32 Gold = 0;
        std::string Name;
        float PipChance = 0.0f;
        int32 TrainingPoints = 0;
        int32 PetEnergy = 0;
        int32 FireConversion = 0;
        float ShadowPipRating = 0.0f;
    };

    Level Shared(int32 number, int32 xp, int32 hitpoints, int32 mana, int32 gold, std::string name, float pipChance, int32 trainingPoints, int32 petEnergy, float shadowPipRating)
    {
        return Level{ number, xp, hitpoints, mana, gold, std::move(name), pipChance, trainingPoints, petEnergy, 0, shadowPipRating };
    }

    Level Own(int32 number, int32 hitpoints, int32 fireConversion)
    {
        return Level{ number, -1, hitpoints, 0, 0, std::string(), 0.0f, 0, 0, fireConversion, 0.0f };
    }

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, std::vector<std::pair<std::string, std::string>> const& fields, std::vector<std::string> const& lists = {})
    {
        Json properties = Json::object();
        uint32 id = 0;
        for (auto const& [type, field] : fields)
        {
            bool const list = std::find(lists.begin(), lists.end(), field) != lists.end();
            properties[field] = Property(type, field, id++, list ? "List" : "Static");
        }
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", Json::array({ "PropertyClass" }) }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    std::string LevelDump()
    {
        Json classes = Json::object();
        classes[std::to_string(StringHash::KiStringHash("class PropertyClass"))] = Json{ { "name", "class PropertyClass" }, { "bases", Json::array() },
            { "hash", StringHash::KiStringHash("class PropertyClass") }, { "properties", Json::object() } };
        AddClass(classes, "class MagicLevelInfo", { { "int", "m_level" }, { "int", "m_xpToLevel" }, { "int", "m_hitpoints" }, { "int", "m_mana" }, { "int", "m_gold" },
            { "std::string", "m_levelName" }, { "float", "m_pipChance" }, { "int", "m_trainingPoints" }, { "int", "m_craftingSlots" }, { "int", "m_petEnergy" },
            { "int", "m_pipConversionRatingAllSchools" }, { "int", "m_pipConversionRatingFire" }, { "int", "m_pipConversionRatingIce" }, { "int", "m_pipConversionRatingStorm" },
            { "int", "m_pipConversionRatingLife" }, { "int", "m_pipConversionRatingMyth" }, { "int", "m_pipConversionRatingDeath" }, { "int", "m_pipConversionRatingBalance" },
            { "float", "m_shadowPipRating" }, { "float", "m_archmastery" } });
        AddClass(classes, "class ClassInfo", { { "std::string", "m_className" }, { "class MagicLevelInfo*", "m_classLevelInfo" } }, { "m_classLevelInfo" });
        AddClass(classes, "class MobRankLevel", { { "int", "m_rank" }, { "int", "m_level" } });
        AddClass(classes, "class MagicXPConfig", { { "float", "m_encounterXPFactors" }, { "class MagicLevelInfo*", "m_levelInfo" }, { "class ClassInfo*", "m_classInfo" },
            { "int", "m_maxSchoolLevel" }, { "float", "m_experienceBonus" }, { "float", "m_schoolOfFocusBonus" }, { "class MobRankLevel*", "m_levelsConfig" } },
            { "m_encounterXPFactors", "m_levelInfo", "m_classInfo", "m_levelsConfig" });
        AddClass(classes, "class BehaviorTemplate", {});
        AddClass(classes, "class MagicSchoolTemplate", { { "class BehaviorTemplate*", "m_behaviors" }, { "std::string", "m_schoolName" }, { "unsigned int", "m_minLevel" },
            { "int", "m_schoolIndex" }, { "std::string", "m_secondarySchoolBadgeList" } }, { "m_behaviors", "m_secondarySchoolBadgeList" });
        AddClass(classes, "class CritAndBlockValues", { { "float", "m_capValue" }, { "float", "m_criticalHitScalarBase" }, { "float", "m_criticalHitScalingFactor" },
            { "float", "m_blockScalarBase" }, { "float", "m_blockScalingFactor" } });
        AddClass(classes, "class CritAndBlockLevelData", { { "int", "m_minLevel" }, { "class SharedPointer<class CritAndBlockValues>", "m_critAndBlockValues" } }, { "m_critAndBlockValues" });
        AddClass(classes, "class PipConversionValues", { { "float", "m_capValue" }, { "float", "m_scalarBase" }, { "float", "m_scalingFactor" } });
        AddClass(classes, "class PipConversionLevelData", { { "int", "m_minLevel" }, { "class SharedPointer<class PipConversionValues>", "m_levelValues" } }, { "m_levelValues" });
        AddClass(classes, "class WizStatisticEffectConfig", { { "int", "m_criticalHitLevelThreshold" }, { "float", "m_criticalDamageAddPercent" }, { "int", "m_shadowPipMax" },
            { "class SharedPointer<class CritAndBlockLevelData>", "m_critAndBlockLevelData" }, { "class SharedPointer<class PipConversionLevelData>", "m_pipConversionLevelData" } },
            { "m_critAndBlockLevelData", "m_pipConversionLevelData" });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    class LevelExtractorTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            LevelViews::RegisterAll(_views);
            _registry = std::make_unique<TypeRegistry>(&_views);
            ASSERT_TRUE(_registry->LoadFromText(LevelDump(), "levels.json")) << _registry->GetErrors().front();
            _catalog = _registry->GetCatalog();

            _shared = { Shared(0, 0, 0, 0, 0, "Levels_Level0", 0.1f, 0, 0, 0.0f), Shared(1, 45, 500, 15, 300000, "Levels_Level1", 0.0f, 0, 40, 0.0f),
                Shared(2, 160, 510, 17, 300000, "Levels_Level2", 0.1f, 1, 41, 5.0f) };
            _schools = { { "Fire", { Own(0, 0, 0), Own(1, 415, 0), Own(2, 430, 12) } }, { "Moon", {} } };
            _cap = 2;
            _files["MagicXPConfig.xml"] = MagicXP();
            _files["MagicSchools/FireSchool.xml"] = School("Fire", 1, 1, { "FireWeaving-Level01", "FireWeaving-Level02" });
            _files["MagicSchools/MoonSchool.xml"] = School("Moon", 2, 10, {});
            _files["WizStatisticEffectConfig.xml"] = StatConfig();
        }

        PropertyObjectPtr Create(std::string const& type)
        {
            PropertyObjectPtr object = PropertyObject::Create(_catalog, type);
            EXPECT_TRUE(object) << type;
            return object;
        }

        std::vector<uint8> Encode(PropertyObject const& object)
        {
            EncodeResult encoded = BindFile::Write(&object);
            EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
            return std::move(encoded.Bytes);
        }

        PropertyObjectPtr LevelInfo(Level const& level)
        {
            PropertyObjectPtr info = Create("class MagicLevelInfo");
            EXPECT_EQ(info->Set("m_level", level.Number), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_xpToLevel", level.Xp), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_hitpoints", level.Hitpoints), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_mana", level.Mana), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_gold", level.Gold), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_levelName", level.Name), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_pipChance", level.PipChance), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_trainingPoints", level.TrainingPoints), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_petEnergy", level.PetEnergy), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_pipConversionRatingFire", level.FireConversion), PropertySetResult::Ok);
            EXPECT_EQ(info->Set("m_shadowPipRating", level.ShadowPipRating), PropertySetResult::Ok);
            return info;
        }

        std::vector<uint8> MagicXP()
        {
            PropertyObjectPtr config = Create("class MagicXPConfig");
            PropertyValue::List factors{ PropertyValue(1.0f), PropertyValue(0.5f), PropertyValue(0.0f) };
            EXPECT_EQ(config->Set("m_encounterXPFactors", std::move(factors)), PropertySetResult::Ok);
            PropertyValue::List shared;
            for (Level const& level : _shared)
                shared.emplace_back(LevelInfo(level));
            EXPECT_EQ(config->Set("m_levelInfo", std::move(shared)), PropertySetResult::Ok);
            PropertyValue::List classes;
            for (auto const& [name, levels] : _schools)
            {
                PropertyObjectPtr school = Create("class ClassInfo");
                EXPECT_EQ(school->Set("m_className", name), PropertySetResult::Ok);
                PropertyValue::List table;
                for (Level const& level : levels)
                    table.emplace_back(LevelInfo(level));
                EXPECT_EQ(school->Set("m_classLevelInfo", std::move(table)), PropertySetResult::Ok);
                classes.emplace_back(std::move(school));
            }
            EXPECT_EQ(config->Set("m_classInfo", std::move(classes)), PropertySetResult::Ok);
            EXPECT_EQ(config->Set("m_maxSchoolLevel", _cap), PropertySetResult::Ok);
            EXPECT_EQ(config->Set("m_experienceBonus", 3.0f), PropertySetResult::Ok);
            EXPECT_EQ(config->Set("m_schoolOfFocusBonus", 1.0f), PropertySetResult::Ok);
            PropertyValue::List ranks;
            for (auto const& [rank, level] : { std::pair{ 0, 0 }, std::pair{ 1, 1 }, std::pair{ 2, 6 } })
            {
                PropertyObjectPtr entry = Create("class MobRankLevel");
                EXPECT_EQ(entry->Set("m_rank", rank), PropertySetResult::Ok);
                EXPECT_EQ(entry->Set("m_level", level), PropertySetResult::Ok);
                ranks.emplace_back(std::move(entry));
            }
            EXPECT_EQ(config->Set("m_levelsConfig", std::move(ranks)), PropertySetResult::Ok);
            return Encode(*config);
        }

        std::vector<uint8> School(std::string const& name, uint32 minLevel, int32 index, std::vector<std::string> const& badges, bool withBehavior = false)
        {
            PropertyObjectPtr school = Create("class MagicSchoolTemplate");
            if (withBehavior)
            {
                PropertyValue::List behaviors;
                behaviors.emplace_back(Create("class BehaviorTemplate"));
                EXPECT_EQ(school->Set("m_behaviors", std::move(behaviors)), PropertySetResult::Ok);
            }
            EXPECT_EQ(school->Set("m_schoolName", name), PropertySetResult::Ok);
            EXPECT_EQ(school->Set("m_minLevel", minLevel), PropertySetResult::Ok);
            EXPECT_EQ(school->Set("m_schoolIndex", index), PropertySetResult::Ok);
            PropertyValue::List list;
            for (std::string const& badge : badges)
                list.emplace_back(badge);
            EXPECT_EQ(school->Set("m_secondarySchoolBadgeList", std::move(list)), PropertySetResult::Ok);
            return Encode(*school);
        }

        std::vector<uint8> StatConfig()
        {
            PropertyObjectPtr config = Create("class WizStatisticEffectConfig");
            EXPECT_EQ(config->Set("m_criticalHitLevelThreshold", int32{ 50 }), PropertySetResult::Ok);
            EXPECT_EQ(config->Set("m_criticalDamageAddPercent", 0.3f), PropertySetResult::Ok);
            EXPECT_EQ(config->Set("m_shadowPipMax", int32{ 2 }), PropertySetResult::Ok);
            PropertyValue::List crit;
            for (auto const& [minLevel, caps] : std::vector<std::pair<int32, std::vector<float>>>{ { 0, { 1.0f } }, { 50, { 1.0f, 0.03f } } })
            {
                PropertyObjectPtr band = Create("class CritAndBlockLevelData");
                EXPECT_EQ(band->Set("m_minLevel", minLevel), PropertySetResult::Ok);
                PropertyValue::List values;
                for (float const cap : caps)
                {
                    PropertyObjectPtr entry = Create("class CritAndBlockValues");
                    EXPECT_EQ(entry->Set("m_capValue", cap), PropertySetResult::Ok);
                    EXPECT_EQ(entry->Set("m_criticalHitScalarBase", 0.00205f), PropertySetResult::Ok);
                    EXPECT_EQ(entry->Set("m_blockScalingFactor", 0.002f), PropertySetResult::Ok);
                    values.emplace_back(std::move(entry));
                }
                EXPECT_EQ(band->Set("m_critAndBlockValues", std::move(values)), PropertySetResult::Ok);
                crit.emplace_back(std::move(band));
            }
            EXPECT_EQ(config->Set("m_critAndBlockLevelData", std::move(crit)), PropertySetResult::Ok);
            PropertyObjectPtr band = Create("class PipConversionLevelData");
            PropertyValue::List values;
            for (float const cap : { 1.0f, 0.33f })
            {
                PropertyObjectPtr entry = Create("class PipConversionValues");
                EXPECT_EQ(entry->Set("m_capValue", cap), PropertySetResult::Ok);
                EXPECT_EQ(entry->Set("m_scalingFactor", 0.009f), PropertySetResult::Ok);
                values.emplace_back(std::move(entry));
            }
            EXPECT_EQ(band->Set("m_levelValues", std::move(values)), PropertySetResult::Ok);
            PropertyValue::List pips;
            pips.emplace_back(std::move(band));
            EXPECT_EQ(config->Set("m_pipConversionLevelData", std::move(pips)), PropertySetResult::Ok);
            return Encode(*config);
        }

        LevelExtraction Extract()
        {
            KiwadBuilder builder(2);
            for (auto const& [name, bytes] : _files)
                builder.Add(name, bytes, true);
            std::filesystem::path const path = _directory.Path() / fmt::format("Root{}.wad", _archives++);
            std::vector<uint8> const archive = builder.Build();
            std::ofstream(path, std::ios::binary).write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
            std::string error;
            std::unique_ptr<KiwadArchive> const opened = KiwadArchive::Open(path, error);
            EXPECT_TRUE(opened) << error;
            return opened ? LevelExtractor::Extract(*opened, _catalog) : LevelExtraction{};
        }

        static bool Mentions(LevelExtraction const& extraction, std::string_view text)
        {
            return std::any_of(extraction.Errors.begin(), extraction.Errors.end(), [text](std::string const& error) { return error.find(text) != std::string::npos; });
        }

        static std::string Report(LevelExtraction const& extraction)
        {
            std::string report;
            for (std::string const& error : extraction.Errors)
                report += error + "\n";
            return report;
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        std::unique_ptr<TypeRegistry> _registry;
        TypeCatalogPtr _catalog;
        std::vector<Level> _shared;
        std::vector<std::pair<std::string, std::vector<Level>>> _schools;
        int32 _cap = 0;
        std::map<std::string, std::vector<uint8>> _files;
        int _archives = 0;
    };
}

TEST_F(LevelExtractorTest, EachSchoolTakesItsOwnValuesAndTheSharedTableOtherwise)
{
    LevelExtraction const extraction = Extract();
    ASSERT_TRUE(extraction.Ok()) << Report(extraction);
    uint32 const fire = StringHash::KiStringHash("Fire");
    ASSERT_EQ(extraction.Levels.Levels.size(), 3u);
    PlayerLevelInfo const& first = extraction.Levels.Levels[1];
    EXPECT_EQ(first.SchoolId, fire);
    EXPECT_EQ(first.Level, 1u);
    EXPECT_EQ(first.XpToLevel, 45);
    EXPECT_EQ(first.Hitpoints, 415);
    EXPECT_EQ(first.Mana, 15);
    EXPECT_EQ(first.Gold, 300000);
    EXPECT_EQ(first.PetEnergy, 40);
    EXPECT_EQ(first.LevelName, "Levels_Level1");
    EXPECT_EQ(first.PipConversion[0], 0);
    PlayerLevelInfo const& second = extraction.Levels.Levels[2];
    EXPECT_EQ(second.Hitpoints, 430);
    EXPECT_EQ(second.XpToLevel, 160);
    EXPECT_EQ(second.TrainingPoints, 1);
    EXPECT_EQ(second.PipConversion[0], 12);
    EXPECT_EQ(second.ShadowPipRating, 5.0f);
    EXPECT_EQ(second.PipChance, 0.1f);
    EXPECT_EQ(extraction.Levels.Levels[0].PipChance, 0.1f);
    EXPECT_EQ(extraction.SchoolsWithTables, (std::vector<std::string>{ "Fire" }));
    EXPECT_EQ(extraction.SchoolsWithoutTables, (std::vector<std::string>{ "Moon" }));

    EXPECT_EQ(extraction.Levels.Schools, (std::vector<MagicSchool>{ { fire, "Fire", 1, 1, { "FireWeaving-Level01", "FireWeaving-Level02" } }, { StringHash::KiStringHash("Moon"), "Moon", 2, 10, {} } }));
    EXPECT_EQ(extraction.Levels.XpConfig, (std::vector<ConfigValue>{ { "m_maxSchoolLevel", 2.0 }, { "m_experienceBonus", 3.0 }, { "m_schoolOfFocusBonus", 1.0 } }));
    EXPECT_EQ(extraction.Levels.EncounterXpFactors, (std::vector<float>{ 1.0f, 0.5f, 0.0f }));
    EXPECT_EQ(extraction.Levels.MobRanks, (std::vector<MobRankLevel>{ { 0, 0 }, { 1, 1 }, { 2, 6 } }));
    EXPECT_EQ(extraction.Stats.Settings, (std::vector<ConfigValue>{ { "m_criticalHitLevelThreshold", 50.0 }, { "m_criticalDamageAddPercent", double{ 0.3f } }, { "m_shadowPipMax", 2.0 } }));
    EXPECT_EQ(extraction.Stats.CritAndBlock, (std::vector<CritAndBlockValues>{ { 0, 0, 1.0f, 0.00205f, 0.0f, 0.0f, 0.002f }, { 50, 0, 1.0f, 0.00205f, 0.0f, 0.0f, 0.002f },
        { 50, 1, 0.03f, 0.00205f, 0.0f, 0.0f, 0.002f } }));
    EXPECT_EQ(extraction.Stats.PipConversion, (std::vector<PipConversionValues>{ { 0, 0, 1.0f, 0.0f, 0.009f }, { 0, 1, 0.33f, 0.0f, 0.009f } }));

    std::vector<std::string> errors;
    std::shared_ptr<PlayerLevelSet const> const set = PlayerLevelSet::Build(extraction.Levels, errors);
    ASSERT_TRUE(set) << errors.front();
    EXPECT_EQ(set->GetMaxLevel(), 2u);
    ASSERT_TRUE(set->GetInfo("Fire", 1));
    EXPECT_EQ(set->GetInfo("Fire", 1)->Hitpoints, 415);
    EXPECT_EQ(set->GetInfo(fire, 9), set->GetInfo(fire, 2));
    EXPECT_FALSE(set->GetInfo("Moon", 1));
    EXPECT_EQ(set->GetLevelForRank(2), 6);
}

TEST_F(LevelExtractorTest, TheScriptReplacesTheNineTablesWithFloatsThatReadBack)
{
    LevelExtraction const extraction = Extract();
    ASSERT_TRUE(extraction.Ok()) << Report(extraction);
    WorldSqlScript const script = LevelScript::Build(extraction);
    std::vector<std::string> const& statements = script.GetStatements();
    ASSERT_EQ(statements.size(), 18u);
    uint32 const fire = StringHash::KiStringHash("Fire");
    EXPECT_EQ(statements[0], "DELETE FROM `player_level_stats`");
    EXPECT_NE(statements[1].find(fmt::format("({}, 1, 45, 415, 15, 300000, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, X'4C6576656C735F4C6576656C31')", fire)), std::string::npos) << statements[1];
    EXPECT_NE(statements[1].find(fmt::format("({}, 0, 0, 0, 0, 0, {}, ", fire, double{ 0.1f })), std::string::npos) << statements[1];
    EXPECT_EQ(statements[3], fmt::format("INSERT INTO `magic_school_template` (`school_id`, `school_name`, `min_level`, `school_index`) VALUES ({}, X'46697265', 1, 1), ({}, X'4D6F6F6E', 2, 10)",
        fire, StringHash::KiStringHash("Moon")));
    EXPECT_EQ(statements[5], fmt::format("INSERT INTO `magic_school_badge` (`school_id`, `position`, `badge_name`) VALUES ({0}, 0, {1}), ({0}, 1, {2})", fire,
        WorldSqlScript::Literal(std::string("FireWeaving-Level01")), WorldSqlScript::Literal(std::string("FireWeaving-Level02"))));
    EXPECT_EQ(statements[9], "INSERT INTO `magic_xp_encounter_factor` (`position`, `factor`) VALUES (0, 1), (1, 0.5), (2, 0)");
    EXPECT_EQ(statements[11], "INSERT INTO `mob_rank_level` (`rank`, `level`) VALUES (0, 0), (1, 1), (2, 6)");
    EXPECT_EQ(statements[16], "DELETE FROM `stat_pip_conversion_band`");
    EXPECT_EQ(std::stod(fmt::format("{}", double{ 0.00205f })), double{ 0.00205f });
    EXPECT_EQ(static_cast<float>(std::stod(fmt::format("{}", double{ 0.00205f }))), 0.00205f);
    EXPECT_EQ(WorldSqlScript::Literal(int64{ -1 }), "-1");
    EXPECT_EQ(WorldSqlScript::Literal(0.5), "0.5");
    EXPECT_EQ(LevelScript::GetTables(), (std::vector<std::string_view>{ "player_level_stats", "magic_school_template", "magic_school_badge", "magic_xp_config", "magic_xp_encounter_factor",
        "mob_rank_level", "stat_effect_config", "stat_crit_block_band", "stat_pip_conversion_band" }));

    WorldSqlScript joined;
    joined.Append(script);
    joined.Append(script);
    EXPECT_EQ(joined.GetStatements().size(), 36u);
}

TEST_F(LevelExtractorTest, BrokenInputsAreReported)
{
    std::map<std::string, std::vector<uint8>> const good = _files;
    auto const expectProblem = [&](std::string_view expected)
    {
        LevelExtraction const extraction = Extract();
        EXPECT_FALSE(extraction.Ok()) << expected;
        EXPECT_TRUE(Mentions(extraction, expected)) << "wanted: " << expected << "\n" << Report(extraction);
        _files = good;
    };

    _files.erase("MagicXPConfig.xml");
    expectProblem("MagicXPConfig.xml: ");
    _files["MagicXPConfig.xml"] = _files["MagicSchools/MoonSchool.xml"];
    expectProblem("MagicXPConfig.xml holds class MagicSchoolTemplate, which the level view does not read");
    _files.erase("MagicSchools/FireSchool.xml");
    _files.erase("MagicSchools/MoonSchool.xml");
    expectProblem("the archive holds no MagicSchools/*.xml school templates");
    _files.erase("MagicSchools/FireSchool.xml");
    expectProblem("MagicXPConfig.xml has a level table for Fire, which no MagicSchools/*.xml template names");
    _files["MagicSchools/FireSchool.xml"] = School("Fire", 1, 1, {}, true);
    expectProblem("MagicSchools/FireSchool.xml carries 1 behavior(s), which nothing reads yet");
    _files["MagicSchools/Other.xml"] = School("Fire", 1, 1, {});
    expectProblem("magic_school_template lists the school Fire twice");
    _files.erase("WizStatisticEffectConfig.xml");
    expectProblem("WizStatisticEffectConfig.xml: ");
    _files["MagicSchools/Other.xml"] = std::vector<uint8>{ 'n', 'o', 't' };
    expectProblem("MagicSchools/Other.xml: ");

    _cap = 0;
    _files["MagicXPConfig.xml"] = MagicXP();
    expectProblem("MagicXPConfig.xml gives the level cap as 0, where 1 to 65535 belongs");
    _cap = 3;
    _files["MagicXPConfig.xml"] = MagicXP();
    expectProblem("the shared table has no level 3, and it must run from 0 to the cap of 3");
    _cap = 2;
    _schools[0].second.push_back(Own(1, 999, 0));
    _files["MagicXPConfig.xml"] = MagicXP();
    expectProblem("player_level_stats lists Fire at level 1 twice");
    _schools[0].second.back() = Own(5, 999, 0);
    _files["MagicXPConfig.xml"] = MagicXP();
    expectProblem("school Fire lists level 5, which the shared table does not");
    _schools[0].second.pop_back();
    _schools[0].second.pop_back();
    _files["MagicXPConfig.xml"] = MagicXP();
    expectProblem("player_level_stats has no row for Fire at level 2, and every school's rows run from 0 to the cap of 2");
}

TEST_F(LevelExtractorTest, TheScriptAppliesToAWorldDatabaseAndLoadsInTheManager)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> server = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(server);
    MySQLConnectionInfo world = *server;
    world.Database = fmt::format("ambrose_extract_levels_{:08x}", std::random_device()());
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

    LevelExtraction const extraction = Extract();
    ASSERT_TRUE(extraction.Ok()) << Report(extraction);
    WorldSqlScript const script = LevelScript::Build(extraction);
    ASSERT_TRUE(DBUpdater::Run(world, "world", UpdaterSettings{}));
    std::string error;
    for (int round = 0; round < 2; ++round)
    {
        ASSERT_TRUE(script.Apply(world, error)) << error;
        ASSERT_TRUE(WorldDatabase.SetConnectionInfo(world.ToConnectionString(), 1, 1));
        ASSERT_EQ(WorldDatabase.Open(), 0u);
        PlayerLevelMgr manager;
        PlayerLevelLoadResult const loaded = manager.Load();
        ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
        EXPECT_FALSE(loaded.Empty);
        EXPECT_EQ(loaded.Schools, 2u);
        EXPECT_EQ(loaded.LevelTables, 1u);
        EXPECT_EQ(loaded.MaxLevel, 2u);
        EXPECT_EQ(loaded.Levels, 3u);
        EXPECT_EQ(loaded.StatSettings, 3u);
        EXPECT_EQ(loaded.BandValues, 5u);
        EXPECT_EQ(manager.GetLevels()->GetData().Levels, extraction.Levels.Levels);
        EXPECT_EQ(manager.GetLevels()->GetData().Schools, extraction.Levels.Schools);
        EXPECT_EQ(manager.GetLevels()->GetData().EncounterXpFactors, extraction.Levels.EncounterXpFactors);
        EXPECT_EQ(manager.GetLevels()->GetData().MobRanks, extraction.Levels.MobRanks);
        EXPECT_EQ(manager.GetLevels()->GetXpSetting("m_experienceBonus"), 3.0);
        EXPECT_EQ(manager.GetStats()->GetData().CritAndBlock, extraction.Stats.CritAndBlock);
        EXPECT_EQ(manager.GetStats()->GetData().PipConversion, extraction.Stats.PipConversion);
        EXPECT_EQ(manager.GetStats()->Get("m_criticalDamageAddPercent"), double{ 0.3f });
        WorldDatabase.Close();
    }
}
