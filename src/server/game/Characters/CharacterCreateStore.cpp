/*
 * Project Ambrose by Imjustchico
 * Reads character_create_school and playercreateinfo in one consistent pass, checks every row before any of them is used, and swaps the pair in together, because a school with no starting state and a starting state for no school are both ways of creating a wizard that cannot stand anywhere. A row is refused for an empty name, an empty zone, a level below one, a negative experience or a school that appears twice, and a refusal leaves the rows already loaded exactly where they were.
 */

#include "CharacterCreateStore.h"
#include "DatabaseEnv.h"
#include "WorldDatabase.h"

#include <fmt/format.h>

#include <utility>

namespace
{
    constexpr uint32 EverySchool = 0;

    std::map<uint32, CharacterSchool> ReadSchools(PreparedResultSet* result, std::vector<std::string>& errors)
    {
        std::map<uint32, CharacterSchool> schools;
        if (!result)
            return schools;
        do
        {
            Field const* const row = result->Fetch();
            CharacterSchool school;
            uint32 const id = row[0].Get<uint32>();
            school.SchoolId = id;
            school.Name = row[1].Get<std::string>();
            school.SortOrder = row[2].Get<uint16>();
            if (school.SchoolId == EverySchool)
            {
                errors.emplace_back("character_create_school has a row with school id 0, which is the id that stands for every school in playercreateinfo and cannot be a school of its own");
                continue;
            }
            if (school.Name.empty())
            {
                errors.push_back(fmt::format("character_create_school row {} has no name", school.SchoolId));
                continue;
            }
            if (!schools.emplace(school.SchoolId, std::move(school)).second)
                errors.push_back(fmt::format("character_create_school has school id {} twice", id));
        } while (result->NextRow());
        return schools;
    }

    std::map<uint32, CharacterStartState> ReadStarts(PreparedResultSet* result, std::vector<std::string>& errors)
    {
        std::map<uint32, CharacterStartState> starts;
        if (!result)
            return starts;
        do
        {
            Field const* const row = result->Fetch();
            CharacterStartState start;
            start.SchoolId = row[0].Get<uint32>();
            start.World = row[1].Get<int32>();
            start.Zone = row[2].Get<std::string>();
            start.ZoneDisplay = row[3].Get<std::string>();
            start.PositionX = row[4].Get<float>();
            start.PositionY = row[5].Get<float>();
            start.PositionZ = row[6].Get<float>();
            start.Orientation = row[7].Get<float>();
            start.Level = row[8].Get<int32>();
            start.Experience = row[9].Get<int32>();
            if (start.Zone.empty())
            {
                errors.push_back(fmt::format("playercreateinfo row {} names no zone, so a wizard made from it would have nowhere to stand", start.SchoolId));
                continue;
            }
            if (start.Level < 1)
            {
                errors.push_back(fmt::format("playercreateinfo row {} starts at level {}, and a wizard begins at level 1 or above", start.SchoolId, start.Level));
                continue;
            }
            if (start.Experience < 0)
            {
                errors.push_back(fmt::format("playercreateinfo row {} starts with {} experience", start.SchoolId, start.Experience));
                continue;
            }
            uint32 const school = start.SchoolId;
            if (!starts.emplace(school, std::move(start)).second)
                errors.push_back(fmt::format("playercreateinfo has school id {} twice", school));
        } while (result->NextRow());
        return starts;
    }
}

CharacterCreateSet::CharacterCreateSet(std::map<uint32, CharacterSchool> schools, std::map<uint32, CharacterStartState> starts)
    : _schools(std::move(schools)), _starts(std::move(starts))
{
}

bool CharacterCreateSet::HasSchool(uint32 schoolId) const
{
    return _schools.contains(schoolId);
}

std::optional<CharacterSchool> CharacterCreateSet::GetSchool(uint32 schoolId) const
{
    auto const found = _schools.find(schoolId);
    if (found == _schools.end())
        return std::nullopt;
    return found->second;
}

CharacterStartState const* CharacterCreateSet::GetStart(uint32 schoolId) const
{
    if (auto const own = _starts.find(schoolId); own != _starts.end())
        return &own->second;
    if (auto const every = _starts.find(EverySchool); every != _starts.end())
        return &every->second;
    return nullptr;
}

std::size_t CharacterCreateSet::GetSchoolCount() const noexcept
{
    return _schools.size();
}

std::size_t CharacterCreateSet::GetStartCount() const noexcept
{
    return _starts.size();
}

CharacterCreateStore& CharacterCreateStore::Instance()
{
    static CharacterCreateStore store;
    return store;
}

CharacterCreateStore::CharacterCreateStore() = default;

CharacterCreateLoadResult CharacterCreateStore::Load()
{
    CharacterCreateLoadResult outcome;
    constexpr std::string_view NotOpen = "the world database is not open, so the creation rows cannot be read";
    auto const schoolStatement = WorldDatabase.GetPreparedStatement(WORLD_SEL_CHARACTER_CREATE_SCHOOLS);
    auto const startStatement = WorldDatabase.GetPreparedStatement(WORLD_SEL_PLAYER_CREATE_INFO);
    if (!WorldDatabase.IsOpen() || !schoolStatement || !startStatement)
    {
        outcome.Errors.emplace_back(NotOpen);
        return outcome;
    }

    std::vector<PreparedQueryResult> results;
    if (!WorldDatabase.QuerySnapshot({ schoolStatement.get(), startStatement.get() }, results))
    {
        outcome.Errors.emplace_back(WorldDatabase.IsOpen()
                ? std::string_view("character_create_school and playercreateinfo cannot be read from the world database")
                : NotOpen);
        return outcome;
    }

    std::map<uint32, CharacterSchool> schools = ReadSchools(results[0].get(), outcome.Errors);
    std::map<uint32, CharacterStartState> starts = ReadStarts(results[1].get(), outcome.Errors);
    if (!outcome.Errors.empty())
        return outcome;

    if (schools.empty())
        outcome.Warnings.emplace_back("character_create_school holds no school, so no wizard can be created until it does");
    else if (starts.empty())
        outcome.Warnings.emplace_back("playercreateinfo holds no row, so no wizard can be created until it does");
    else
        for (auto const& [schoolId, school] : schools)
            if (!starts.contains(schoolId) && !starts.contains(EverySchool))
                outcome.Warnings.push_back(fmt::format("school {} has no playercreateinfo row and there is no row 0 standing for every school, so that school cannot be created", school.Name));

    outcome.Schools = schools.size();
    outcome.Starts = starts.size();
    outcome.Loaded = true;
    Set(std::make_shared<CharacterCreateSet const>(std::move(schools), std::move(starts)));
    return outcome;
}

std::shared_ptr<CharacterCreateSet const> CharacterCreateStore::Get() const
{
    std::lock_guard const lock(_guard);
    return _rows;
}

uint64 CharacterCreateStore::GetGeneration() const
{
    std::lock_guard const lock(_guard);
    return _generation;
}

void CharacterCreateStore::Set(std::shared_ptr<CharacterCreateSet const> rows)
{
    std::lock_guard const lock(_guard);
    _rows = std::move(rows);
    ++_generation;
}

void CharacterCreateStore::Clear()
{
    std::lock_guard const lock(_guard);
    _rows.reset();
    ++_generation;
}
