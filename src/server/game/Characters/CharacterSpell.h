/*
 * Project Ambrose by Imjustchico
 * A row of character_spell: the spell a wizard learned by its template id, whether it still knows it, the spellbook revision it was last learned at, which orders the book, and the revision of the write that last set the row.
 */

#ifndef AMBROSE_CHARACTERSPELL_H
#define AMBROSE_CHARACTERSPELL_H

#include "Types.h"

struct CharacterSpell
{
    uint32 SpellId = 0;
    bool Known = true;
    uint64 Learned = 0;
    uint64 Revision = 0;

    bool operator==(CharacterSpell const&) const = default;
};

#endif
