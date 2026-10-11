/*
 * Project Ambrose by Imjustchico
 * Builds a generation of creature decks from the rows the world database gives, checking every deck and card before any of it is kept, and resolves a deck's spell names against the spells the install holds.
 */

#include "CreatureDeck.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace
{
    std::string FileStem(std::string_view file)
    {
        std::size_t const slash = file.find_last_of("/\\");
        if (slash != std::string_view::npos)
            file.remove_prefix(slash + 1);
        if (std::size_t const dot = file.rfind('.'); dot != std::string_view::npos)
            file.remove_suffix(file.size() - dot);
        return Ambrose::ToLower(file);
    }

    std::string WithoutSpaces(std::string text)
    {
        std::erase(text, ' ');
        return text;
    }
}

uint32 CreatureDeck::GetCardCount() const noexcept
{
    uint32 total = 0;
    for (CreatureDeckCard const& card : Cards)
        total += card.Count;
    return total;
}

std::shared_ptr<CreatureDeckSet const> CreatureDeckSet::Build(std::vector<CreatureDeckRow> decks, std::vector<CreatureDeckSpellRow> spells, SpellResolver const& resolve, std::vector<std::string>& errors)
{
    std::size_t faults = 0;
    auto const report = [&](std::string message)
    {
        if (++faults <= MaxReportedErrors)
            errors.push_back(std::move(message));
    };
    auto built = std::make_shared<CreatureDeckSet>();
    built->_decks.reserve(decks.size());
    for (CreatureDeckRow& row : decks)
    {
        std::string folded = Ambrose::ToLower(row.Name);
        if (built->_byId.contains(row.Id))
        {
            report(fmt::format("creature_deck holds id {} twice", row.Id));
            continue;
        }
        if (built->_byFoldedName.contains(folded))
        {
            report(fmt::format("creature_deck names {} twice, which differ at most in case", row.Name));
            continue;
        }
        built->_byId.emplace(row.Id, built->_decks.size());
        built->_byFoldedName.emplace(std::move(folded), built->_decks.size());
        built->_decks.push_back({ row.Id, std::move(row.Name), {} });
    }
    for (CreatureDeckSpellRow& row : spells)
    {
        auto const deck = built->_byId.find(row.DeckId);
        if (deck == built->_byId.end())
        {
            report(fmt::format("creature_deck_spell gives {} to deck id {}, which creature_deck does not hold", row.Spell, row.DeckId));
            continue;
        }
        CreatureDeck& target = built->_decks[deck->second];
        if (row.Count == 0)
        {
            report(fmt::format("{} holds {} with no copies", target.Name, row.Spell));
            continue;
        }
        std::optional<uint32> const spellId = resolve ? resolve(row.Spell) : std::nullopt;
        if (!spellId)
        {
            report(fmt::format("{} holds {}, which no spell of the install is named", target.Name, row.Spell));
            continue;
        }
        target.Cards.push_back({ *spellId, std::move(row.Spell), row.Count });
    }
    if (faults > MaxReportedErrors)
        errors.push_back(fmt::format("and {} more problems", faults - MaxReportedErrors));
    if (faults != 0)
        return nullptr;
    return built;
}

CreatureDeckSet::SpellResolver CreatureDeckSet::ResolverFor(std::shared_ptr<SpellStore const> spells)
{
    auto byFile = std::make_shared<std::unordered_map<std::string, uint32>>();
    auto bySpacelessFile = std::make_shared<std::unordered_map<std::string, uint32>>();
    for (SpellInfo const& spell : spells->GetAll())
    {
        std::string stem = FileStem(spell.File);
        bySpacelessFile->emplace(WithoutSpaces(stem), spell.TemplateId);
        byFile->emplace(std::move(stem), spell.TemplateId);
    }
    return [spells = std::move(spells), byFile = std::move(byFile), bySpacelessFile = std::move(bySpacelessFile)](std::string_view name) -> std::optional<uint32>
    {
        if (SpellInfo const* const spell = spells->FindByName(name))
            return spell->TemplateId;
        std::string const folded = Ambrose::ToLower(name);
        if (auto const file = byFile->find(folded); file != byFile->end())
            return file->second;
        if (auto const spaceless = bySpacelessFile->find(WithoutSpaces(folded)); spaceless != bySpacelessFile->end())
            return spaceless->second;
        return std::nullopt;
    };
}

CreatureDeck const* CreatureDeckSet::Find(uint32 id) const noexcept
{
    auto const found = _byId.find(id);
    return found == _byId.end() ? nullptr : &_decks[found->second];
}

CreatureDeck const* CreatureDeckSet::FindByName(std::string_view name) const
{
    auto const found = _byFoldedName.find(Ambrose::ToLower(name));
    return found == _byFoldedName.end() ? nullptr : &_decks[found->second];
}

std::size_t CreatureDeckSet::GetCardRowCount() const noexcept
{
    std::size_t total = 0;
    for (CreatureDeck const& deck : _decks)
        total += deck.Cards.size();
    return total;
}
