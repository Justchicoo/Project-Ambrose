/*
 * Project Ambrose by Imjustchico
 * Tests the creature decks on made-up decks and spells: a set is built with each card's spell resolved and its copies kept, a deck is found by id and by name whatever its case, a deck named twice, a card of a missing deck, a card with no copies or a spell the install does not hold refuses the set and names each fault, a failed replace keeps the set serving, and the resolver finds a spell by its name, its template file's name or that name without spaces.
 */

#include "CreatureDeck.h"
#include "CreatureDeckMgr.h"
#include "SpellMgr.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    std::optional<uint32> KnownSpells(std::string_view name)
    {
        if (name == "Test Bolt")
            return 11;
        if (name == "Test Shield")
            return 22;
        return std::nullopt;
    }

    bool Holds(std::vector<std::string> const& errors, std::string const& text)
    {
        for (std::string const& error : errors)
            if (error.find(text) != std::string::npos)
                return true;
        return false;
    }

    SpellInfo MadeUpSpell(std::string name, std::string file)
    {
        SpellInfo spell;
        spell.Name = std::move(name);
        spell.File = std::move(file);
        spell.TemplateId = SpellStore::NameHash(spell.Name);
        return spell;
    }
}

TEST(CreatureDeckTest, BuildsDecksWithResolvedCards)
{
    std::vector<std::string> errors;
    auto const set = CreatureDeckSet::Build({ { 1, "Test-Deck-A" }, { 2, "Test-Deck-B" } },
        { { 1, "Test Bolt", 4 }, { 1, "Test Shield", 2 }, { 2, "Test Bolt", 1 } }, KnownSpells, errors);
    ASSERT_NE(set, nullptr);
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(set->Size(), 2u);
    EXPECT_EQ(set->GetCardRowCount(), 3u);
    CreatureDeck const* const deck = set->FindByName("test-deck-a");
    ASSERT_NE(deck, nullptr);
    EXPECT_EQ(deck->Id, 1u);
    EXPECT_EQ(deck->GetCardCount(), 6u);
    ASSERT_EQ(deck->Cards.size(), 2u);
    EXPECT_EQ(deck->Cards[0].SpellId, 11u);
    EXPECT_EQ(deck->Cards[1].Count, 2u);
    EXPECT_EQ(set->Find(2), set->FindByName("Test-Deck-B"));
    EXPECT_EQ(set->Find(3), nullptr);
}

TEST(CreatureDeckTest, RefusesASetWithAnyFault)
{
    std::vector<std::string> errors;
    auto const set = CreatureDeckSet::Build({ { 1, "Test-Deck-A" }, { 2, "test-deck-a" }, { 1, "Test-Deck-C" } },
        { { 1, "Test Missing", 1 }, { 9, "Test Bolt", 1 }, { 1, "Test Shield", 0 } }, KnownSpells, errors);
    EXPECT_EQ(set, nullptr);
    EXPECT_TRUE(Holds(errors, "names test-deck-a twice"));
    EXPECT_TRUE(Holds(errors, "holds id 1 twice"));
    EXPECT_TRUE(Holds(errors, "Test Missing, which no spell"));
    EXPECT_TRUE(Holds(errors, "deck id 9"));
    EXPECT_TRUE(Holds(errors, "no copies"));
}

TEST(CreatureDeckTest, FailedReplaceKeepsTheServingSet)
{
    sCreatureDeckMgr.Clear();
    std::vector<std::string> errors;
    ASSERT_TRUE(sCreatureDeckMgr.Replace({ { 1, "Test-Deck-A" } }, { { 1, "Test Bolt", 3 } }, KnownSpells, errors));
    uint64 const generation = sCreatureDeckMgr.GetGeneration();
    EXPECT_FALSE(sCreatureDeckMgr.Replace({ { 1, "Test-Deck-A" } }, { { 1, "Test Missing", 3 } }, KnownSpells, errors));
    EXPECT_TRUE(Holds(errors, "Test Missing"));
    EXPECT_EQ(sCreatureDeckMgr.GetGeneration(), generation);
    std::shared_ptr<CreatureDeckSet const> const decks = sCreatureDeckMgr.GetDecks();
    ASSERT_NE(decks->FindByName("Test-Deck-A"), nullptr);
    EXPECT_EQ(decks->FindByName("Test-Deck-A")->Cards[0].Spell, "Test Bolt");
    sCreatureDeckMgr.Clear();
}

TEST(CreatureDeckTest, ResolverFindsSpellsByNameOrFile)
{
    std::vector<std::string> errors;
    std::shared_ptr<SpellStore const> const spells = SpellStore::Build({ MadeUpSpell("Test Bolt", "Spells/Test/Test Bolt - Mob.xml"), MadeUpSpell("Gust Of Testing", "Spells/Test/TestGust.xml") }, errors);
    ASSERT_NE(spells, nullptr);
    CreatureDeckSet::SpellResolver const resolve = CreatureDeckSet::ResolverFor(spells);
    EXPECT_EQ(resolve("test bolt"), SpellStore::NameHash("Test Bolt"));
    EXPECT_EQ(resolve("Test Bolt - Mob"), SpellStore::NameHash("Test Bolt"));
    EXPECT_EQ(resolve("Test Gust"), SpellStore::NameHash("Gust Of Testing"));
    EXPECT_EQ(resolve("Test Missing"), std::nullopt);
}
