/*
 * Project Ambrose by Imjustchico
 * Reads the two creature deck tables in one snapshot, resolves their spells against the spells being served, and swaps the set in only once it has checked itself whole.
 */

#include "CreatureDeckMgr.h"
#include "DatabaseEnv.h"
#include "ReloadMgr.h"
#include "WorldDatabase.h"

#include <utility>

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
    return Replace(std::move(deckRows), std::move(cardRows), CreatureDeckSet::ResolverFor(sSpellMgr.GetSpells()), errors);
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
