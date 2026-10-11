/*
 * Project Ambrose by Imjustchico
 * Reads the two creature deck tables in one snapshot and the deck file World.CreatureDeckFile names, resolves their spells against the spells being served, and swaps the set in only once it has checked itself whole.
 */

#include "CreatureDeckMgr.h"
#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "ReloadMgr.h"
#include "Settings.h"
#include "WorldDatabase.h"

#include <fmt/format.h>
#include <sqlite3.h>

#include <algorithm>
#include <map>
#include <utility>

namespace
{
    std::string ColumnText(sqlite3_stmt* statement, int column)
    {
        void const* const bytes = sqlite3_column_blob(statement, column);
        int const size = sqlite3_column_bytes(statement, column);
        return bytes == nullptr ? std::string() : std::string(static_cast<char const*>(bytes), static_cast<std::size_t>(size));
    }
}

CreatureDeckMgr& CreatureDeckMgr::Instance()
{
    static CreatureDeckMgr instance;
    return instance;
}

void CreatureDeckMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); }, { std::string(SpellMgr::Target) });
}

bool CreatureDeckMgr::Load(std::vector<std::string>& errors)
{
    bool const open = WorldDatabase.IsOpen();
    auto const decks = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_CREATURE_DECKS) : nullptr;
    auto const cards = open ? WorldDatabase.GetPreparedStatement(WORLD_SEL_CREATURE_DECK_SPELLS) : nullptr;
    if (!decks || !cards)
    {
        errors.emplace_back("the world database is not open, so the creature decks cannot be read");
        return false;
    }
    std::vector<PreparedQueryResult> results;
    if (!WorldDatabase.QuerySnapshot({ decks.get(), cards.get() }, results))
    {
        errors.emplace_back("creature_deck and creature_deck_spell cannot be read from the world database");
        return false;
    }
    std::vector<CreatureDeckRow> deckRows;
    if (PreparedResultSet* const result = results[0].get(); result && result->GetRowCount() > 0)
    {
        deckRows.reserve(result->GetRowCount());
        do
        {
            Field const* const row = result->Fetch();
            deckRows.push_back({ row[0].Get<uint32>(), row[1].Get<std::string>() });
        } while (result->NextRow());
    }
    std::vector<CreatureDeckSpellRow> cardRows;
    if (PreparedResultSet* const result = results[1].get(); result && result->GetRowCount() > 0)
    {
        cardRows.reserve(result->GetRowCount());
        do
        {
            Field const* const row = result->Fetch();
            cardRows.push_back({ row[0].Get<uint32>(), row[1].Get<std::string>(), row[2].Get<uint32>() });
        } while (result->NextRow());
    }
    if (std::string const file = sSettings.Get<std::string>("World.CreatureDeckFile"); !file.empty())
    {
        uint32 last = 0;
        for (CreatureDeckRow const& row : deckRows)
            last = std::max(last, row.Id);
        if (!ReadDeckFile(ConfigMgr::PathFromUtf8(file), last + 1, deckRows, cardRows, errors))
            return false;
    }
    return Replace(std::move(deckRows), std::move(cardRows), CreatureDeckSet::ResolverFor(sSpellMgr.GetSpells()), errors);
}

bool CreatureDeckMgr::ReadDeckFile(std::filesystem::path const& file, uint32 firstId, std::vector<CreatureDeckRow>& decks, std::vector<CreatureDeckSpellRow>& spells, std::vector<std::string>& errors)
{
    std::string const shown = file.u8string().empty() ? std::string() : std::string(reinterpret_cast<char const*>(file.u8string().c_str()));
    sqlite3* database = nullptr;
    if (sqlite3_open_v2(shown.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
        errors.push_back(fmt::format("the creature deck file {} cannot be opened: {}", shown, database ? sqlite3_errmsg(database) : "out of memory"));
        sqlite3_close(database);
        return false;
    }
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, "SELECT name, spell, count FROM deck ORDER BY id", -1, &statement, nullptr) != SQLITE_OK)
    {
        errors.push_back(fmt::format("the creature deck file {} has no deck table of name, spell and count: {}", shown, sqlite3_errmsg(database)));
        sqlite3_close(database);
        return false;
    }
    std::map<std::string, uint32> ids;
    uint32 next = firstId;
    int step = SQLITE_ROW;
    while ((step = sqlite3_step(statement)) == SQLITE_ROW)
    {
        std::string name = ColumnText(statement, 0);
        auto [found, added] = ids.emplace(name, next);
        if (added)
            decks.push_back({ next++, std::move(name) });
        sqlite3_int64 const count = sqlite3_column_int64(statement, 2);
        spells.push_back({ found->second, ColumnText(statement, 1), count < 0 ? 0u : static_cast<uint32>(count) });
    }
    bool const done = step == SQLITE_DONE;
    if (!done)
        errors.push_back(fmt::format("the creature deck file {} could not be read to its end: {}", shown, sqlite3_errmsg(database)));
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return done;
}

bool CreatureDeckMgr::Replace(std::vector<CreatureDeckRow> decks, std::vector<CreatureDeckSpellRow> spells, CreatureDeckSet::SpellResolver const& resolve, std::vector<std::string>& errors)
{
    std::shared_ptr<CreatureDeckSet const> set = CreatureDeckSet::Build(std::move(decks), std::move(spells), resolve, errors);
    if (!set)
        return false;
    _decks.Replace(std::move(set));
    return true;
}

void CreatureDeckMgr::Clear()
{
    _decks.Replace(std::make_shared<CreatureDeckSet const>());
}
