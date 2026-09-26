/*
 * Project Ambrose by Imjustchico
 * What a wizard's spellbook tells its client about one spell, the SpellIDTracker the client keeps in its ClientSpellbookBehavior: the spell's template id, whether it is retired and the tiered spell group it belongs to, which is NoGroup for a spell in none.
 */

#ifndef AMBROSE_SPELLTRACKER_H
#define AMBROSE_SPELLTRACKER_H

#include "Types.h"

struct SpellTracker
{
    static constexpr int32 NoGroup = -1;

    uint32 SpellId = 0;
    bool Retired = false;
    int32 TieredGroupIndex = NoGroup;

    bool operator==(SpellTracker const&) const = default;
};

#endif
