/*
 * Project Ambrose by Imjustchico
 * Builds the book from the rows still known, in the order they were learned whatever order they are handed in, and takes its revision from every row, unlearned ones too, so the next write outranks all of them. A spell is appended when learned, which is where the client's AddSpell puts it, and a tracker names a tiered spell's retired flag and group and any other spell as neither retired nor in a group, as the client does.
 */

#include "PlayerSpellbook.h"

#include <algorithm>

PlayerSpellbook PlayerSpellbook::FromStored(std::vector<CharacterSpell> stored)
{
    std::sort(stored.begin(), stored.end(), [](CharacterSpell const& left, CharacterSpell const& right)
    {
        return left.Learned != right.Learned ? left.Learned < right.Learned : left.SpellId < right.SpellId;
    });
    PlayerSpellbook book;
    for (CharacterSpell const& row : stored)
    {
        book._revision = std::max(book._revision, row.Revision);
        if (!row.Known || row.SpellId == 0 || !book._known.insert(row.SpellId).second)
            continue;
        book._spells.push_back(row.SpellId);
    }
    return book;
}

SpellTracker PlayerSpellbook::TrackerFor(SpellInfo const& spell) noexcept
{
    SpellTracker tracker;
    tracker.SpellId = spell.TemplateId;
    if (spell.Tiered)
    {
        tracker.Retired = spell.Retired;
        tracker.TieredGroupIndex = spell.TieredGroupIndex;
    }
    return tracker;
}

std::optional<CharacterSpell> PlayerSpellbook::Learn(uint32 spellId)
{
    if (spellId == 0 || !_known.insert(spellId).second)
        return std::nullopt;
    _spells.push_back(spellId);
    ++_revision;
    return CharacterSpell{ spellId, true, _revision, _revision };
}

std::optional<CharacterSpell> PlayerSpellbook::Unlearn(uint32 spellId)
{
    if (_known.erase(spellId) == 0)
        return std::nullopt;
    std::erase(_spells, spellId);
    ++_revision;
    return CharacterSpell{ spellId, false, 0, _revision };
}

std::vector<SpellTracker> PlayerSpellbook::Track(SpellStore const& spells, std::vector<uint32>& missing) const
{
    std::vector<SpellTracker> trackers;
    trackers.reserve(_spells.size());
    for (uint32 const spellId : _spells)
    {
        if (SpellInfo const* const spell = spells.Find(spellId))
            trackers.push_back(TrackerFor(*spell));
        else
            missing.push_back(spellId);
    }
    return trackers;
}
