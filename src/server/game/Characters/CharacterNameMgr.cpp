/*
 * Project Ambrose by Imjustchico
 * Reads character_name_part rows grouped by table and locale, reporting the first place each table's positions stop running from 0 without gaps, and character_name_disallowed rows in one snapshot of the world database, validates the rows it could read even after a gap so every problem is reported, builds the name set off to the side, and publishes it with a new generation under a lock only when nothing failed, one load at a time.
 */

#include "CharacterNameMgr.h"
#include "DatabaseEnv.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <utility>

CharacterNameMgr& CharacterNameMgr::Instance()
{
    static CharacterNameMgr manager;
    return manager;
}

CharacterNameMgr::CharacterNameMgr() : _snapshot{ CharacterNameSet::Empty(), std::string(DefaultLocale) }
{
}

std::vector<CharacterNameTable> CharacterNameMgr::ReadTables(PreparedResultSet* result, std::vector<std::string>& errors)
{
    std::vector<CharacterNameTable> tables;
    if (!result || result->GetRowCount() == 0)
        return tables;
    bool broken = false;
    do
    {
        PreparedResultSet const& row = *result;
        std::string name = row[0].Get<std::string>();
        std::string locale = row[1].Get<std::string>();
        uint32 const index = row[2].Get<uint16>();
        if (tables.empty() || tables.back().Name != name || tables.back().Locale != locale)
        {
            tables.emplace_back();
            tables.back().Name = std::move(name);
            tables.back().Locale = std::move(locale);
            broken = false;
        }
        CharacterNameTable& table = tables.back();
        if (broken)
            continue;
        if (index != table.Parts.size())
        {
            errors.push_back(fmt::format("character_name_part {} ({}) has position {} where position {} should be; positions must run from 0 without gaps", table.Name, table.Locale, index, table.Parts.size()));
            broken = true;
            continue;
        }
        table.Parts.push_back(CharacterNamePart{ row[3].Get<std::string>(), row[4].Get<std::string>() });
    } while (result->NextRow());
    return tables;
}

std::vector<DisallowedName> CharacterNameMgr::ReadDisallowed(PreparedResultSet* result)
{
    std::vector<DisallowedName> disallowed;
    if (!result || result->GetRowCount() == 0)
        return disallowed;
    do
    {
        PreparedResultSet const& row = *result;
        disallowed.push_back(DisallowedName{ row[0].Get<uint32>(), row[1].Get<uint32>(), row[2].Get<uint32>(), row[3].Get<uint32>(), row[4].Get<uint32>(), row[5].Get<uint32>() });
    } while (result->NextRow());
    return disallowed;
}

CharacterNameLoadResult CharacterNameMgr::Load()
{
    std::lock_guard const loading(_loadMutex);
    CharacterNameLoadResult outcome;
    constexpr std::string_view NotOpen = "the world database is not open, so the name tables cannot be read";
    auto const partsStatement = WorldDatabase.GetPreparedStatement(WORLD_SEL_CHARACTER_NAME_PARTS);
    auto const disallowedStatement = WorldDatabase.GetPreparedStatement(WORLD_SEL_CHARACTER_NAME_DISALLOWED);
    if (!WorldDatabase.IsOpen() || !partsStatement || !disallowedStatement)
    {
        outcome.Errors.emplace_back(NotOpen);
        return outcome;
    }
    std::vector<PreparedQueryResult> results;
    if (!WorldDatabase.QuerySnapshot({ partsStatement.get(), disallowedStatement.get() }, results))
    {
        outcome.Errors.emplace_back(WorldDatabase.IsOpen() ? std::string_view("character_name_part and character_name_disallowed cannot be read from the world database") : NotOpen);
        return outcome;
    }

    std::vector<CharacterNameTable> tables = ReadTables(results[0].get(), outcome.Errors);
    std::vector<DisallowedName> disallowed = ReadDisallowed(results[1].get());
    bool const readProblems = !outcome.Errors.empty();
    std::shared_ptr<CharacterNameSet const> names = CharacterNameSet::Build(std::move(tables), std::move(disallowed), outcome.Errors);
    if (readProblems || !names)
        return outcome;

    outcome.Loaded = true;
    outcome.Tables = names->GetTables().size();
    outcome.Parts = names->GetPartCount();
    outcome.Disallowed = names->GetDisallowed().size();
    std::vector<std::string> const humanLocales = names->GetHumanLocales();
    outcome.HumanLocales = humanLocales.size();
    std::lock_guard const lock(_mutex);
    if (outcome.Tables != 0 && humanLocales.empty())
        outcome.Warnings.push_back("the name tables hold no locale with all four human name tables, so wizard names cannot be checked or shown");
    else if (!humanLocales.empty() && !names->HasLocale(_snapshot.DefaultLocale))
        outcome.Warnings.push_back(fmt::format("the default locale {} has no human name tables, so names checked or shown without a locale fail; the tables have {}", _snapshot.DefaultLocale, fmt::join(humanLocales, ", ")));
    _snapshot.Names = std::move(names);
    ++_generation;
    return outcome;
}

void CharacterNameMgr::SetDefaultLocale(std::string locale)
{
    std::lock_guard const lock(_mutex);
    _snapshot.DefaultLocale = std::move(locale);
}

std::string CharacterNameMgr::GetDefaultLocale() const
{
    std::lock_guard const lock(_mutex);
    return _snapshot.DefaultLocale;
}

std::shared_ptr<CharacterNameSet const> CharacterNameMgr::GetNames() const
{
    std::lock_guard const lock(_mutex);
    return _snapshot.Names;
}

uint64 CharacterNameMgr::GetGeneration() const
{
    std::lock_guard const lock(_mutex);
    return _generation;
}

void CharacterNameMgr::Clear()
{
    std::lock_guard const loading(_loadMutex);
    std::lock_guard const lock(_mutex);
    _snapshot.Names = CharacterNameSet::Empty();
    ++_generation;
}

CharacterNameMgr::Snapshot CharacterNameMgr::Current() const
{
    std::lock_guard const lock(_mutex);
    return _snapshot;
}

NameCheck CharacterNameMgr::Check(uint32 nameIndices, uint32 gender, std::string_view locale, std::optional<uint32> localeId) const
{
    Snapshot const current = Current();
    return current.Names->Check(nameIndices, gender, locale.empty() ? std::string_view(current.DefaultLocale) : locale, localeId);
}

bool CharacterNameMgr::IsValidIndices(uint32 nameIndices, uint32 gender, std::string_view locale) const
{
    Snapshot const current = Current();
    return current.Names->IsValidIndices(nameIndices, gender, locale.empty() ? std::string_view(current.DefaultLocale) : locale);
}

bool CharacterNameMgr::IsDisallowed(uint32 nameIndices, uint32 gender, std::optional<uint32> localeId) const
{
    return GetNames()->IsDisallowed(nameIndices, gender, localeId);
}

std::optional<std::string> CharacterNameMgr::FormatName(uint32 nameIndices, uint32 gender, std::string_view locale) const
{
    Snapshot const current = Current();
    return current.Names->FormatName(nameIndices, gender, locale.empty() ? std::string_view(current.DefaultLocale) : locale);
}
