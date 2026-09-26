/*
 * Project Ambrose by Imjustchico
 * Tests a wizard's spellbook: learning a spell it knows does not add it twice or write anything, unlearning takes it out under the next revision and leaves a row that says so, a spell learned again goes to the end; a book read from rows handed in any order keeps the order they were learned in, skips the unlearned and takes its revision from every row; and the trackers the client is sent name a tiered spell's group and retired flag, give a plain spell no group, and leave out a spell the set does not hold, naming it.
 */

#include "PlayerSpellbook.h"
#include "StringHash.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    SpellInfo Spell(std::string name, bool tiered = false, bool retired = false, int32 group = SpellInfo::NoTieredGroup)
    {
        SpellInfo spell;
        spell.TemplateId = StringHash::KiStringHash(name);
        spell.Name = std::move(name);
        spell.File = "Spells/" + spell.Name + ".xml";
        spell.Tiered = tiered;
        spell.Retired = retired;
        spell.TieredGroupIndex = group;
        return spell;
    }

    uint32 Id(std::string_view name)
    {
        return StringHash::KiStringHash(name);
    }
}

TEST(PlayerSpellbookTest, LearningASpellItKnowsDoesNotAddItTwice)
{
    PlayerSpellbook book;
    std::optional<CharacterSpell> const first = book.Learn(Id("Fire Cat"));
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, (CharacterSpell{ Id("Fire Cat"), true, 1, 1 }));
    EXPECT_FALSE(book.Learn(Id("Fire Cat"))) << "a spell it knows gives nothing to write or send";
    EXPECT_EQ(book.GetSpells(), std::vector<uint32>{ Id("Fire Cat") });
    EXPECT_EQ(book.GetRevision(), 1u);
    EXPECT_TRUE(book.Knows(Id("Fire Cat")));
    EXPECT_FALSE(book.Learn(0)) << "no spell's name hashes to 0";
}

TEST(PlayerSpellbookTest, UnlearningTakesASpellOutAndOneLearnedAgainGoesToTheEnd)
{
    PlayerSpellbook book;
    ASSERT_TRUE(book.Learn(Id("Fire Cat")));
    ASSERT_TRUE(book.Learn(Id("Thunder Snake")));
    std::optional<CharacterSpell> const gone = book.Unlearn(Id("Fire Cat"));
    ASSERT_TRUE(gone);
    EXPECT_EQ(*gone, (CharacterSpell{ Id("Fire Cat"), false, 0, 3 })) << "the row stays, saying it is no longer known, under the next revision";
    EXPECT_FALSE(book.Knows(Id("Fire Cat")));
    EXPECT_FALSE(book.Unlearn(Id("Fire Cat"))) << "a spell it does not know gives nothing to write or send";
    std::optional<CharacterSpell> const again = book.Learn(Id("Fire Cat"));
    ASSERT_TRUE(again);
    EXPECT_EQ(again->Learned, 4u);
    EXPECT_EQ(book.GetSpells(), (std::vector<uint32>{ Id("Thunder Snake"), Id("Fire Cat") })) << "where the client's own AddSpell puts it";
}

TEST(PlayerSpellbookTest, ABookReadFromItsRowsKeepsTheOrderLearnedAndTheNewestRevision)
{
    PlayerSpellbook book = PlayerSpellbook::FromStored({ { Id("Thunder Snake"), true, 5, 5 }, { Id("Blood Bat"), false, 0, 9 }, { Id("Fire Cat"), true, 2, 2 },
        { Id("Frost Beetle"), true, 5, 6 } });
    std::vector<uint32> expected{ Id("Fire Cat") };
    for (uint32 const id : { Id("Thunder Snake"), Id("Frost Beetle") })
        expected.push_back(id);
    std::sort(expected.begin() + 1, expected.end());
    EXPECT_EQ(book.GetSpells(), expected) << "learned order first, and two learned at the same revision by id";
    EXPECT_FALSE(book.Knows(Id("Blood Bat")));
    EXPECT_EQ(book.GetRevision(), 9u) << "an unlearned row's revision counts, so the next write outranks it";
    std::optional<CharacterSpell> const next = book.Learn(Id("Blood Bat"));
    ASSERT_TRUE(next);
    EXPECT_EQ(next->Revision, 10u);
}

TEST(PlayerSpellbookTest, TrackersNameATieredSpellsGroupAndLeaveOutASpellTheSetDoesNotHold)
{
    std::vector<std::string> errors;
    std::shared_ptr<SpellStore const> const spells = SpellStore::Build({ Spell("Fire Cat", true, false, 4), Spell("Fire Cat - Amulet"), Spell("Old Bolt", true, true) }, errors);
    ASSERT_TRUE(spells) << errors.front();
    PlayerSpellbook book;
    for (std::string_view const name : { "Fire Cat", "Removed Spell", "Fire Cat - Amulet", "Old Bolt" })
        ASSERT_TRUE(book.Learn(Id(name)));
    std::vector<uint32> missing;
    std::vector<SpellTracker> const trackers = book.Track(*spells, missing);
    EXPECT_EQ(trackers, (std::vector<SpellTracker>{ { Id("Fire Cat"), false, 4 }, { Id("Fire Cat - Amulet"), false, SpellTracker::NoGroup }, { Id("Old Bolt"), true, SpellTracker::NoGroup } }));
    EXPECT_EQ(missing, std::vector<uint32>{ Id("Removed Spell") }) << "a spell the set does not hold is left out of what the client is sent, and named";
    EXPECT_TRUE(book.Knows(Id("Removed Spell"))) << "and stays in the book";

    SpellInfo plain = Spell("Plain");
    plain.Retired = true;
    EXPECT_EQ(PlayerSpellbook::TrackerFor(plain), (SpellTracker{ Id("Plain"), false, SpellTracker::NoGroup })) << "only a tiered spell carries its retired flag, as the client fills it";
}
