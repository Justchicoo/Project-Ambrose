/*
 * Project Ambrose by Imjustchico
 * Every creature deck the world database holds (sCreatureDeckMgr), read from creature_deck and creature_deck_spell in one snapshot, each card's spell found among the spells the install holds, and swapped in as one checked set through the reload target creature_decks, which follows spells; a set that fails to load keeps the one serving and reports every fault, and a caller holds the set it was given for as long as it needs it.
 */

#ifndef AMBROSE_CREATUREDECKMGR_H
#define AMBROSE_CREATUREDECKMGR_H

#include "CreatureDeck.h"
#include "ReloadableStore.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class CreatureDeckMgr
{
public:
    static constexpr std::string_view Target = "creature_decks";

    static CreatureDeckMgr& Instance();

    CreatureDeckMgr() = default;
    CreatureDeckMgr(CreatureDeckMgr const&) = delete;
    CreatureDeckMgr& operator=(CreatureDeckMgr const&) = delete;

    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    bool Replace(std::vector<CreatureDeckRow> decks, std::vector<CreatureDeckSpellRow> spells, CreatureDeckSet::SpellResolver const& resolve, std::vector<std::string>& errors);
    std::shared_ptr<CreatureDeckSet const> GetDecks() const { return _decks.Get(); }
    uint64 GetGeneration() const noexcept { return _decks.GetGeneration(); }
    void Clear();

private:
    ReloadableStore<CreatureDeckSet> _decks;
};

#define sCreatureDeckMgr CreatureDeckMgr::Instance()

#endif
