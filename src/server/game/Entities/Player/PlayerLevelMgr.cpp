/*
 * Project Ambrose by Imjustchico
 * Reads the six level tables in one snapshot and the three stat tables in another, hands each school its badges and the encounter factors their order only when the positions run from 0 without a gap or a stranger, and swaps a set in only once the set has checked itself whole.
 */

#include "PlayerLevelMgr.h"
#include "DatabaseEnv.h"
#include "ReloadMgr.h"
#include "WorldDatabase.h"

#include <fmt/format.h>

#include <map>
#include <utility>

namespace
{
    bool HasRows(PreparedResultSet* result)
    {
        return result && result->GetRowCount() > 0;
    }

    std::vector<PlayerLevelInfo> ReadLevels(PreparedResultSet* result)
    {
        std::vector<PlayerLevelInfo> rows;
        if (!HasRows(result))
            return rows;
        rows.reserve(result->GetRowCount());
        do
        {
            Field const* const row = result->Fetch();
            PlayerLevelInfo info;
            info.SchoolId = row[0].Get<uint32>();
            info.Level = row[1].Get<uint32>();
            info.XpToLevel = row[2].Get<int32>();
            info.Hitpoints = row[3].Get<int32>();
            info.Mana = row[4].Get<int32>();
            info.Gold = row[5].Get<int32>();
            info.PipChance = row[6].Get<float>();
            info.TrainingPoints = row[7].Get<int32>();
            info.CraftingSlots = row[8].Get<int32>();
            info.PetEnergy = row[9].Get<int32>();
            info.PipConversionAll = row[10].Get<int32>();
            for (std::size_t school = 0; school < info.PipConversion.size(); ++school)
                info.PipConversion[school] = row[11 + school].Get<int32>();
            info.ShadowPipRating = row[18].Get<float>();
            info.Archmastery = row[19].Get<float>();
            info.LevelName = row[20].Get<std::string>();
            rows.push_back(std::move(info));
        } while (result->NextRow());
        return rows;
    }

    std::vector<MagicSchool> ReadSchools(PreparedResultSet* schools, PreparedResultSet* badges, std::vector<std::string>& errors)
    {
        std::vector<MagicSchool> rows;
        std::map<uint32, std::size_t> byId;
        if (HasRows(schools))
        {
            do
            {
                Field const* const row = schools->Fetch();
                MagicSchool school;
                school.Id = row[0].Get<uint32>();
                school.Name = row[1].Get<std::string>();
                school.MinLevel = row[2].Get<uint32>();
                school.Index = row[3].Get<int32>();
                byId.emplace(school.Id, rows.size());
                rows.push_back(std::move(school));
            } while (schools->NextRow());
        }
        if (HasRows(badges))
        {
            do
            {
                Field const* const row = badges->Fetch();
                uint32 const schoolId = row[0].Get<uint32>();
                uint32 const position = row[1].Get<uint32>();
                auto const school = byId.find(schoolId);
                if (school == byId.end())
                {
                    errors.push_back(fmt::format("magic_school_badge gives a badge to school id {}, which magic_school_template does not hold", schoolId));
                    continue;
                }
                std::vector<std::string>& list = rows[school->second].Badges;
                if (position != list.size())
                {
                    errors.push_back(fmt::format("magic_school_badge gives {} a badge at position {} where position {} comes next", rows[school->second].Name, position, list.size()));
                    continue;
                }
                list.push_back(row[2].Get<std::string>());
            } while (badges->NextRow());
        }
        return rows;
    }

    std::vector<ConfigValue> ReadSettings(PreparedResultSet* result)
    {
        std::vector<ConfigValue> rows;
        if (!HasRows(result))
            return rows;
        do
        {
            Field const* const row = result->Fetch();
            rows.push_back({ row[0].Get<std::string>(), row[1].Get<double>() });
        } while (result->NextRow());
        return rows;
    }

    std::vector<float> ReadFactors(PreparedResultSet* result, std::vector<std::string>& errors)
    {
        std::vector<float> factors;
        if (!HasRows(result))
            return factors;
        do
        {
            Field const* const row = result->Fetch();
            uint32 const position = row[0].Get<uint32>();
            if (position != factors.size())
            {
                errors.push_back(fmt::format("magic_xp_encounter_factor holds position {} where position {} comes next", position, factors.size()));
                continue;
            }
            factors.push_back(row[1].Get<float>());
        } while (result->NextRow());
        return factors;
    }

    std::vector<MobRankLevel> ReadRanks(PreparedResultSet* result)
    {
        std::vector<MobRankLevel> rows;
        if (!HasRows(result))
            return rows;
        do
        {
            Field const* const row = result->Fetch();
            rows.push_back({ row[0].Get<int32>(), row[1].Get<int32>() });
        } while (result->NextRow());
        return rows;
    }

    std::vector<CritAndBlockValues> ReadCritAndBlock(PreparedResultSet* result)
    {
        std::vector<CritAndBlockValues> rows;
        if (!HasRows(result))
            return rows;
        do
        {
            Field const* const row = result->Fetch();
            rows.push_back({ row[0].Get<int32>(), row[1].Get<uint32>(), row[2].Get<float>(), row[3].Get<float>(), row[4].Get<float>(), row[5].Get<float>(), row[6].Get<float>() });
        } while (result->NextRow());
        return rows;
    }

    std::vector<PipConversionValues> ReadPipConversion(PreparedResultSet* result)
    {
        std::vector<PipConversionValues> rows;
        if (!HasRows(result))
            return rows;
        do
        {
            Field const* const row = result->Fetch();
            rows.push_back({ row[0].Get<int32>(), row[1].Get<uint32>(), row[2].Get<float>(), row[3].Get<float>(), row[4].Get<float>() });
        } while (result->NextRow());
        return rows;
    }
}

PlayerLevelMgr& PlayerLevelMgr::Instance()
{
    static PlayerLevelMgr instance;
    return instance;
}

void PlayerLevelMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(LevelTarget), [this](std::vector<std::string>& errors) { return LoadLevels(errors); });
    sReloadMgr.Register(std::string(StatTarget), [this](std::vector<std::string>& errors) { return LoadStats(errors); });
}

bool PlayerLevelMgr::LoadLevels(std::vector<std::string>& errors)
{
    bool const open = WorldDatabase.IsOpen();
    auto const levels = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_PLAYER_LEVEL_STATS) : nullptr;
    auto const schools = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_MAGIC_SCHOOL_TEMPLATES) : nullptr;
    auto const badges = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_MAGIC_SCHOOL_BADGES) : nullptr;
    auto const settings = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_MAGIC_XP_CONFIG) : nullptr;
    auto const factors = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_MAGIC_XP_ENCOUNTER_FACTORS) : nullptr;
    auto const ranks = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_MOB_RANK_LEVELS) : nullptr;
    if (!levels || !schools || !badges || !settings || !factors || !ranks)
    {
        errors.emplace_back("the world database is not open, so the level tables cannot be read");
        return false;
    }
    std::vector<PreparedQueryResult> results;
    if (!WorldDatabase.QuerySnapshot({ levels.get(), schools.get(), badges.get(), settings.get(), factors.get(), ranks.get() }, results))
    {
        errors.emplace_back("player_level_stats, magic_school_template, magic_school_badge, magic_xp_config, magic_xp_encounter_factor and mob_rank_level cannot be read from the world database");
        return false;
    }
    std::size_t const before = errors.size();
    PlayerLevelData data;
    data.Levels = ReadLevels(results[0].get());
    data.Schools = ReadSchools(results[1].get(), results[2].get(), errors);
    data.XpConfig = ReadSettings(results[3].get());
    data.EncounterXpFactors = ReadFactors(results[4].get(), errors);
    data.MobRanks = ReadRanks(results[5].get());
    std::shared_ptr<PlayerLevelSet const> set = PlayerLevelSet::Build(std::move(data), errors);
    if (!set || errors.size() != before)
        return false;
    _levels.Replace(std::move(set));
    return true;
}

bool PlayerLevelMgr::LoadStats(std::vector<std::string>& errors)
{
    bool const open = WorldDatabase.IsOpen();
    auto const settings = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_STAT_EFFECT_CONFIG) : nullptr;
    auto const critAndBlock = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_STAT_CRIT_BLOCK_BANDS) : nullptr;
    auto const pipConversion = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_STAT_PIP_CONVERSION_BANDS) : nullptr;
    if (!settings || !critAndBlock || !pipConversion)
    {
        errors.emplace_back("the world database is not open, so the stat tables cannot be read");
        return false;
    }
    std::vector<PreparedQueryResult> results;
    if (!WorldDatabase.QuerySnapshot({ settings.get(), critAndBlock.get(), pipConversion.get() }, results))
    {
        errors.emplace_back("stat_effect_config, stat_crit_block_band and stat_pip_conversion_band cannot be read from the world database");
        return false;
    }
    StatEffectData data;
    data.Settings = ReadSettings(results[0].get());
    data.CritAndBlock = ReadCritAndBlock(results[1].get());
    data.PipConversion = ReadPipConversion(results[2].get());
    std::shared_ptr<StatEffectSet const> set = StatEffectSet::Build(std::move(data), errors);
    if (!set)
        return false;
    _stats.Replace(std::move(set));
    return true;
}

PlayerLevelLoadResult PlayerLevelMgr::Load()
{
    PlayerLevelLoadResult result;
    bool const levels = LoadLevels(result.Errors);
    bool const stats = LoadStats(result.Errors);
    result.Loaded = levels && stats;
    std::shared_ptr<PlayerLevelSet const> const levelSet = GetLevels();
    std::shared_ptr<StatEffectSet const> const statSet = GetStats();
    result.Empty = levelSet->IsEmpty() && statSet->IsEmpty();
    result.Schools = levelSet->GetData().Schools.size();
    result.LevelTables = levelSet->GetLevelTableCount();
    result.MaxLevel = levelSet->GetMaxLevel();
    result.Levels = levelSet->GetData().Levels.size();
    result.StatSettings = statSet->GetData().Settings.size();
    result.BandValues = statSet->GetData().CritAndBlock.size() + statSet->GetData().PipConversion.size();
    return result;
}

void PlayerLevelMgr::Clear()
{
    _levels.Replace(std::make_shared<PlayerLevelSet const>());
    _stats.Replace(std::make_shared<StatEffectSet const>());
}
