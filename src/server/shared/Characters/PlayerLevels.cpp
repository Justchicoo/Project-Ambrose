/*
 * Project Ambrose by Imjustchico
 * Checks a level set whole before anything reads it: a school's id must be the client's hash of its name, names, badges and encounter factors must be present and fit their columns, MagicXPConfig must give a level cap, every school with rows must run from level 0 to that cap with each level once, a row must name a school that exists, every number must be finite, and a mob rank must be listed once; a set with no rows at all is valid and empty. A rank the table lacks takes the level of the highest rank listed, which is the last entry of the client's own list.
 */

#include "PlayerLevels.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <cmath>
#include <set>
#include <utility>

namespace
{
    bool Finite(PlayerLevelInfo const& row) noexcept
    {
        return std::isfinite(row.PipChance) && std::isfinite(row.ShadowPipRating) && std::isfinite(row.Archmastery);
    }

    void CheckSchools(std::vector<MagicSchool> const& schools, std::vector<std::string>& errors)
    {
        std::set<uint32> ids;
        std::set<std::string, std::less<>> names;
        for (MagicSchool const& school : schools)
        {
            if (school.Name.empty() || school.Name.size() > PlayerLevelSet::MaxSchoolNameBytes)
            {
                errors.push_back(fmt::format("magic_school_template has a school {} whose name is {} bytes, where 1 to {} fit", school.Id, school.Name.size(), PlayerLevelSet::MaxSchoolNameBytes));
                continue;
            }
            if (uint32 const expected = StringHash::KiStringHash(school.Name); school.Id != expected)
                errors.push_back(fmt::format("magic_school_template gives {} the id {}, where the client's hash of that name is {}", school.Name, school.Id, expected));
            if (!ids.insert(school.Id).second)
                errors.push_back(fmt::format("magic_school_template lists the school id {} twice", school.Id));
            if (!names.insert(school.Name).second)
                errors.push_back(fmt::format("magic_school_template lists the school {} twice", school.Name));
            if (school.Badges.size() > PlayerLevelSet::MaxBadges)
                errors.push_back(fmt::format("magic_school_badge gives {} {} badges, where at most {} fit", school.Name, school.Badges.size(), PlayerLevelSet::MaxBadges));
            for (std::size_t position = 0; position < school.Badges.size(); ++position)
                if (school.Badges[position].empty() || school.Badges[position].size() > PlayerLevelSet::MaxBadgeBytes)
                    errors.push_back(fmt::format("magic_school_badge gives {} a badge at position {} that is {} bytes, where 1 to {} fit", school.Name, position, school.Badges[position].size(), PlayerLevelSet::MaxBadgeBytes));
        }
    }
}

std::shared_ptr<PlayerLevelSet const> PlayerLevelSet::Build(PlayerLevelData data, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    auto set = std::make_shared<PlayerLevelSet>();
    set->_data = std::move(data);
    PlayerLevelData const& rows = set->_data;

    CheckSchools(rows.Schools, errors);
    for (MagicSchool const& school : rows.Schools)
    {
        set->_schoolsById.emplace(school.Id, &school);
        set->_schoolsByName.emplace(school.Name, &school);
    }

    for (ConfigValue const& setting : rows.XpConfig)
    {
        if (setting.Name.empty() || setting.Name.size() > MaxSettingNameBytes)
            errors.push_back(fmt::format("magic_xp_config has a setting whose name is {} bytes, where 1 to {} fit", setting.Name.size(), MaxSettingNameBytes));
        else if (!std::isfinite(setting.Value))
            errors.push_back(fmt::format("magic_xp_config gives {} a value that is not a finite number", setting.Name));
        else if (!set->_xpSettings.emplace(setting.Name, setting.Value).second)
            errors.push_back(fmt::format("magic_xp_config lists {} twice", setting.Name));
    }
    if (rows.EncounterXpFactors.size() > MaxEncounterFactors)
        errors.push_back(fmt::format("magic_xp_encounter_factor holds {} factors, where at most {} fit", rows.EncounterXpFactors.size(), MaxEncounterFactors));
    for (std::size_t position = 0; position < rows.EncounterXpFactors.size(); ++position)
        if (!std::isfinite(rows.EncounterXpFactors[position]))
            errors.push_back(fmt::format("magic_xp_encounter_factor gives position {} a factor that is not a finite number", position));
    std::set<int32> ranks;
    for (MobRankLevel const& rank : rows.MobRanks)
        if (!ranks.insert(rank.Rank).second)
            errors.push_back(fmt::format("mob_rank_level lists rank {} twice", rank.Rank));

    if (rows.Empty())
        return errors.size() == before ? set : nullptr;

    auto const cap = set->_xpSettings.find(MaxLevelSetting);
    if (cap == set->_xpSettings.end())
        errors.push_back(fmt::format("magic_xp_config does not give {}, the level cap every school's rows run to", MaxLevelSetting));
    else if (cap->second < 1.0 || cap->second > MaxLevel || cap->second != std::floor(cap->second))
        errors.push_back(fmt::format("magic_xp_config gives {} as {}, where a whole number from 1 to {} belongs", MaxLevelSetting, cap->second, MaxLevel));
    else
        set->_maxLevel = static_cast<uint32>(cap->second);

    std::map<uint32, std::vector<PlayerLevelInfo const*>> levels;
    for (PlayerLevelInfo const& row : rows.Levels)
    {
        auto const school = set->_schoolsById.find(row.SchoolId);
        if (school == set->_schoolsById.end())
        {
            errors.push_back(fmt::format("player_level_stats has a row for school id {} at level {}, and magic_school_template has no such school", row.SchoolId, row.Level));
            continue;
        }
        if (!Finite(row))
            errors.push_back(fmt::format("player_level_stats gives {} at level {} a pip chance, shadow pip rating or archmastery that is not a finite number", school->second->Name, row.Level));
        if (row.LevelName.size() > MaxLevelNameBytes)
            errors.push_back(fmt::format("player_level_stats gives {} at level {} a level name of {} bytes, where at most {} fit", school->second->Name, row.Level, row.LevelName.size(), MaxLevelNameBytes));
        if (row.Level > (set->_maxLevel != 0 ? set->_maxLevel : MaxLevel))
        {
            errors.push_back(fmt::format("player_level_stats gives {} a row at level {}, above the level cap of {}", school->second->Name, row.Level, set->_maxLevel != 0 ? set->_maxLevel : MaxLevel));
            continue;
        }
        std::vector<PlayerLevelInfo const*>& table = levels[row.SchoolId];
        if (table.size() <= row.Level)
            table.resize(std::size_t{ row.Level } + 1, nullptr);
        if (table[row.Level])
            errors.push_back(fmt::format("player_level_stats lists {} at level {} twice", school->second->Name, row.Level));
        else
            table[row.Level] = &row;
    }
    if (set->_maxLevel != 0)
        for (auto& [schoolId, table] : levels)
        {
            table.resize(std::size_t{ set->_maxLevel } + 1, nullptr);
            for (uint32 level = 0; level <= set->_maxLevel; ++level)
                if (!table[level])
                {
                    errors.push_back(fmt::format("player_level_stats has no row for {} at level {}, and every school's rows run from 0 to the cap of {}", set->_schoolsById.at(schoolId)->Name, level, set->_maxLevel));
                    break;
                }
        }
    if (levels.empty())
        errors.emplace_back("player_level_stats has no rows, so no wizard has base stats");
    set->_levels = std::move(levels);
    if (errors.size() != before)
        return nullptr;
    return set;
}

PlayerLevelInfo const* PlayerLevelSet::GetInfo(uint32 schoolId, int64 level) const noexcept
{
    if (level < 0)
        return nullptr;
    auto const table = _levels.find(schoolId);
    if (table == _levels.end() || table->second.empty())
        return nullptr;
    std::size_t const index = static_cast<uint64>(level) >= table->second.size() ? table->second.size() - 1 : static_cast<std::size_t>(level);
    return table->second[index];
}

PlayerLevelInfo const* PlayerLevelSet::GetInfo(std::string_view school, int64 level) const noexcept
{
    MagicSchool const* const found = FindSchool(school);
    return found ? GetInfo(found->Id, level) : nullptr;
}

MagicSchool const* PlayerLevelSet::FindSchool(uint32 id) const noexcept
{
    auto const found = _schoolsById.find(id);
    return found == _schoolsById.end() ? nullptr : found->second;
}

MagicSchool const* PlayerLevelSet::FindSchool(std::string_view name) const noexcept
{
    auto const found = _schoolsByName.find(name);
    return found == _schoolsByName.end() ? nullptr : found->second;
}

std::optional<double> PlayerLevelSet::GetXpSetting(std::string_view name) const noexcept
{
    auto const found = _xpSettings.find(name);
    if (found == _xpSettings.end())
        return std::nullopt;
    return found->second;
}

int32 PlayerLevelSet::GetLevelForRank(int32 rank) const noexcept
{
    if (rank <= 0 || _data.MobRanks.empty())
        return 0;
    MobRankLevel const* highest = nullptr;
    for (MobRankLevel const& entry : _data.MobRanks)
    {
        if (entry.Rank == rank)
            return entry.Level;
        if (!highest || entry.Rank > highest->Rank)
            highest = &entry;
    }
    return highest->Level;
}
