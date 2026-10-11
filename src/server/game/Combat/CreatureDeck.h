/*
 * Project Ambrose by Imjustchico
 * One generation of the creature decks the world database holds: each deck found by its name whatever its case, holding each of its spells by the spell's template id with how many copies it carries, checked whole when built, so a deck named twice, a card of a deck that does not exist, a card with no copies or a spell the install does not hold refuses the whole set and every fault is named up to MaxReportedErrors with the rest counted; and the spell resolver that finds a deck's spell in the spells the install holds, by its exact name, by the name of its template file, or by that file name with its spaces left out.
 */

#ifndef AMBROSE_CREATUREDECK_H
#define AMBROSE_CREATUREDECK_H

#include "SpellMgr.h"
#include "Types.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct CreatureDeckRow
{
    uint32 Id = 0;
    std::string Name;
};

struct CreatureDeckSpellRow
{
    uint32 DeckId = 0;
    std::string Spell;
    uint32 Count = 0;
};

struct CreatureDeckCard
{
    uint32 SpellId = 0;
    std::string Spell;
    uint32 Count = 0;
};

struct CreatureDeck
{
    uint32 Id = 0;
    std::string Name;
    std::vector<CreatureDeckCard> Cards;

    uint32 GetCardCount() const noexcept;
};

class CreatureDeckSet
{
public:
    static constexpr std::size_t MaxReportedErrors = 20;

    using SpellResolver = std::function<std::optional<uint32>(std::string_view)>;

    static std::shared_ptr<CreatureDeckSet const> Build(std::vector<CreatureDeckRow> decks, std::vector<CreatureDeckSpellRow> spells, SpellResolver const& resolve, std::vector<std::string>& errors);
    static SpellResolver ResolverFor(std::shared_ptr<SpellStore const> spells);

    CreatureDeck const* Find(uint32 id) const noexcept;
    CreatureDeck const* FindByName(std::string_view name) const;
    std::vector<CreatureDeck> const& GetAll() const noexcept { return _decks; }
    std::size_t Size() const noexcept { return _decks.size(); }
    std::size_t GetCardRowCount() const noexcept;

private:
    std::vector<CreatureDeck> _decks;
    std::unordered_map<uint32, std::size_t> _byId;
    std::unordered_map<std::string, std::size_t> _byFoldedName;
};

#endif
