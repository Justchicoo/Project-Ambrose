/*
 * Project Ambrose by Imjustchico
 * A wizard's spellbook while it plays: the spells it knows in the order it learned them, read from its character_spell rows, with the revision of the last write, so learning a spell it knows or unlearning one it does not changes nothing and every change gives the row to write under the next revision; and the tracker the client keeps for each known spell, filled from the spell set in use as the client's own AddSpell fills one, a spell the set does not hold left out and named.
 */

#ifndef AMBROSE_PLAYERSPELLBOOK_H
#define AMBROSE_PLAYERSPELLBOOK_H

#include "CharacterSpell.h"
#include "SpellMgr.h"
#include "SpellTracker.h"

#include <optional>
#include <unordered_set>
#include <vector>

class PlayerSpellbook
{
public:
    static PlayerSpellbook FromStored(std::vector<CharacterSpell> stored);
    static SpellTracker TrackerFor(SpellInfo const& spell) noexcept;

    bool Knows(uint32 spellId) const noexcept { return _known.contains(spellId); }
    std::vector<uint32> const& GetSpells() const noexcept { return _spells; }
    uint64 GetRevision() const noexcept { return _revision; }

    std::optional<CharacterSpell> Learn(uint32 spellId);
    std::optional<CharacterSpell> Unlearn(uint32 spellId);
    std::vector<SpellTracker> Track(SpellStore const& spells, std::vector<uint32>& missing) const;

private:
    std::vector<uint32> _spells;
    std::unordered_set<uint32> _known;
    uint64 _revision = 0;
};

#endif
