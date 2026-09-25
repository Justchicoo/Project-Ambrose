/*
 * Project Ambrose by Imjustchico
 * Tests the name tables on tables the test writes: packed indices round-trip, a first name alone when middle and last are 0 and first, space, middle and last otherwise in each locale, out-of-range indices, set high bits, unknown genders and locales refused, disallowed names matched by gender, locale id and wildcard indices, other tables kept as given, and every kind of invalid table or disallowed name reported with a capped list.
 */

#include "CharacterNames.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr uint32 Female = 0;
    constexpr uint32 Male = 1;

    constexpr uint32 Pack(uint8 first, uint8 middle, uint8 last)
    {
        return NameIndices{ first, middle, last }.Pack();
    }

    CharacterNameTable Table(std::string name, std::string locale, std::vector<std::pair<std::string, std::string>> parts)
    {
        CharacterNameTable table{ std::move(name), std::move(locale), {} };
        for (auto& [key, text] : parts)
            table.Parts.push_back(CharacterNamePart{ std::move(key), std::move(text) });
        return table;
    }

    std::vector<CharacterNameTable> HumanTables(std::string const& locale, bool german = false)
    {
        return {
            Table("FirstName_HumanMale", locale, { { "First_Boy_0", "Aaron" }, { "First_Boy_1", german ? "J\xC3\xBCrgen" : "Blaze" }, { "First_Boy_2", "Cody" } }),
            Table("FirstName_HumanFemale", locale, { { "First_Girl_0", "Abby" }, { "First_Girl_1", "Bella" } }),
            Table("MiddleName_Human", locale, { { "", "" }, { "Middle_0", german ? "Sturm" : "Storm" }, { "Middle_1", "Dragon" } }),
            Table("LastName_Human", locale, { { "", "" }, { "Last_0", "Blade" }, { "Last_1", "Rider" }, { "Last_2", "Heart" } }),
        };
    }

    std::shared_ptr<CharacterNameSet const> BuildOrFail(std::vector<CharacterNameTable> tables, std::vector<DisallowedName> disallowed = {})
    {
        std::vector<std::string> errors;
        std::shared_ptr<CharacterNameSet const> set = CharacterNameSet::Build(std::move(tables), std::move(disallowed), errors);
        EXPECT_TRUE(errors.empty()) << errors.front();
        return set;
    }

    std::vector<std::string> BuildErrors(std::vector<CharacterNameTable> tables, std::vector<DisallowedName> disallowed = {})
    {
        std::vector<std::string> errors;
        EXPECT_FALSE(CharacterNameSet::Build(std::move(tables), std::move(disallowed), errors));
        return errors;
    }

    bool Contains(std::vector<std::string> const& errors, std::string const& expected)
    {
        return std::find(errors.begin(), errors.end(), expected) != errors.end();
    }
}

TEST(CharacterNamesTest, PackedIndicesRoundTrip)
{
    NameIndices const indices = NameIndices::Unpack(0x00ABCDEFu);
    EXPECT_EQ(indices.First, 0xAB);
    EXPECT_EQ(indices.Middle, 0xCD);
    EXPECT_EQ(indices.Last, 0xEF);
    EXPECT_EQ(indices.Pack(), 0x00ABCDEFu);
    EXPECT_EQ(NameIndices::Unpack(0xFF000102u), (NameIndices{ 0, 1, 2, 0xFF })) << "the top byte is the locale the client counted its positions in, and is kept rather than dropped";
    EXPECT_EQ((NameIndices{ 0xAB, 0xCD, 0xEF, 3 }).Pack(), 0x03ABCDEFu);
    EXPECT_EQ(Pack(120, 45, 200), (120u << 16) | (45u << 8) | 200u);
}

TEST(CharacterNamesTest, NamesFormatAsFirstOrFirstSpaceMiddleAndLast)
{
    std::vector<CharacterNameTable> tables = HumanTables("en-US");
    std::vector<CharacterNameTable> german = HumanTables("de", true);
    tables.insert(tables.end(), german.begin(), german.end());
    std::shared_ptr<CharacterNameSet const> const names = BuildOrFail(std::move(tables));
    ASSERT_TRUE(names);

    EXPECT_EQ(names->FormatName(Pack(0, 0, 0), Male, "en-US"), "Aaron");
    EXPECT_EQ(names->FormatName(Pack(1, 1, 2), Male, "en-US"), "Blaze StormRider");
    EXPECT_EQ(names->FormatName(Pack(2, 0, 1), Male, "en-US"), "Cody Blade");
    EXPECT_EQ(names->FormatName(Pack(0, 2, 0), Male, "en-US"), "Aaron Dragon");
    EXPECT_EQ(names->FormatName(Pack(1, 1, 3), Female, "en-US"), "Bella StormHeart");
    EXPECT_EQ(names->FormatName(Pack(1, 1, 2), Male, "de"), "J\xC3\xBCrgen SturmRider");
    EXPECT_FALSE(names->FormatName(Pack(0, 0, 0), Male, "fr"));
    EXPECT_EQ(names->GetHumanLocales(), (std::vector<std::string>{ "de", "en-US" }));
    EXPECT_EQ(names->GetPartCount(), 24u);
    EXPECT_TRUE(names->HasLocale("de"));
    EXPECT_FALSE(names->HasLocale("fr"));
    ASSERT_TRUE(names->FindTable("LastName_Human", "de"));
    EXPECT_EQ(names->FindTable("LastName_Human", "de")->Parts.size(), 4u);
    EXPECT_FALSE(names->FindTable("LastName_Human", "fr"));
}

TEST(CharacterNamesTest, IndicesOutsideTheTablesAreRefused)
{
    std::shared_ptr<CharacterNameSet const> const names = BuildOrFail(HumanTables("en-US"));
    ASSERT_TRUE(names);
    EXPECT_EQ(names->Check(Pack(2, 2, 3), Male, "en-US"), NameCheck::Ok);
    EXPECT_TRUE(names->IsValidIndices(Pack(2, 2, 3), Male, "en-US"));
    EXPECT_EQ(names->Check(Pack(3, 0, 0), Male, "en-US"), NameCheck::FirstOutOfRange);
    EXPECT_EQ(names->Check(Pack(2, 0, 0), Female, "en-US"), NameCheck::FirstOutOfRange);
    EXPECT_EQ(names->Check(Pack(0, 3, 0), Male, "en-US"), NameCheck::MiddleOutOfRange);
    EXPECT_EQ(names->Check(Pack(0, 0, 4), Female, "en-US"), NameCheck::LastOutOfRange);
    EXPECT_EQ(names->Check(0x03000000u | Pack(2, 2, 3), Male, "en-US"), NameCheck::Ok)
        << "the client marks which locale its indices are positions in in the top byte, which is not part of any index";
    EXPECT_EQ(NameIndices::Unpack(0x03000000u | Pack(2, 2, 3)).Locale, 3);
    EXPECT_EQ(names->Check(0, 2, "en-US"), NameCheck::UnknownGender);
    EXPECT_EQ(names->Check(0, Male, "pl"), NameCheck::UnknownLocale);
    for (uint32 const bad : { Pack(3, 0, 0), Pack(0, 3, 0), Pack(0, 0, 4) })
    {
        EXPECT_FALSE(names->IsValidIndices(bad, Male, "en-US")) << bad;
        EXPECT_FALSE(names->FormatName(bad, Male, "en-US")) << bad;
    }
    EXPECT_FALSE(names->IsValidIndices(0, 7, "en-US"));
    EXPECT_EQ(CharacterNameSet::GetCheckName(NameCheck::LastOutOfRange), "last name index out of range");
    EXPECT_EQ(CharacterNameSet::Empty()->Check(0, Male, "en-US"), NameCheck::UnknownLocale);
}

TEST(CharacterNamesTest, DisallowedNamesMatchByGenderLocaleAndWildcard)
{
    std::vector<DisallowedName> const disallowed{ { 0, 1, Male, 1, 1, 2 }, { 1, 2, Male, 2, 1, 999 }, { 2, 1, Female, 256, 2, 256 } };
    std::shared_ptr<CharacterNameSet const> const names = BuildOrFail(HumanTables("en-US"), disallowed);
    ASSERT_TRUE(names);
    EXPECT_TRUE(names->IsDisallowed(Pack(1, 1, 2), Male));
    EXPECT_TRUE(names->IsDisallowed(Pack(1, 1, 2), Male, 1));
    EXPECT_FALSE(names->IsDisallowed(Pack(1, 1, 2), Male, 2));
    EXPECT_FALSE(names->IsDisallowed(Pack(1, 1, 2), Female));
    EXPECT_FALSE(names->IsDisallowed(Pack(1, 1, 3), Male));
    for (uint8 last = 0; last < 4; ++last)
        EXPECT_TRUE(names->IsDisallowed(Pack(2, 1, last), Male, 2)) << int{ last };
    EXPECT_FALSE(names->IsDisallowed(Pack(2, 2, 0), Male));
    EXPECT_TRUE(names->IsDisallowed(Pack(0, 2, 3), Female));
    EXPECT_TRUE(names->IsDisallowed(Pack(1, 2, 0), Female));
    EXPECT_FALSE(names->IsDisallowed(Pack(1, 1, 0), Female));
    EXPECT_EQ(names->Check(Pack(1, 1, 2), Male, "en-US"), NameCheck::Disallowed);
    EXPECT_EQ(names->Check(Pack(1, 1, 2), Male, "en-US", 2), NameCheck::Ok);
    EXPECT_EQ(names->Check(Pack(3, 1, 2), Male, "en-US"), NameCheck::FirstOutOfRange);
    EXPECT_TRUE(names->IsValidIndices(Pack(1, 1, 2), Male, "en-US"));
    EXPECT_EQ(names->FormatName(Pack(1, 1, 2), Male, "en-US"), "Blaze StormRider");
    EXPECT_EQ(names->GetDisallowed(), disallowed);
}

TEST(CharacterNamesTest, OtherTablesAreKeptAsGiven)
{
    std::vector<CharacterNameTable> tables = HumanTables("en-US");
    tables.push_back(Table("FirstName", "fr", { { "First_0", "B\xC3\xA9" "b\xC3\xA9" }, { "First_1", "" } }));
    tables.push_back(Table("FirstName_AdventureParty", "en-US", { { "First_0", "" }, { "First_1", "Battling" } }));
    std::shared_ptr<CharacterNameSet const> const names = BuildOrFail(tables);
    ASSERT_TRUE(names);
    ASSERT_TRUE(names->FindTable("FirstName", "fr"));
    EXPECT_EQ(*names->FindTable("FirstName", "fr"), tables[4]);
    EXPECT_EQ(names->GetTables().size(), 6u);
    EXPECT_EQ(names->GetHumanLocales(), (std::vector<std::string>{ "en-US" }));
    EXPECT_TRUE(BuildOrFail({}));
}

TEST(CharacterNamesTest, InvalidTablesAndDisallowedNamesAreReported)
{
    std::vector<CharacterNameTable> tables = HumanTables("en-US");
    tables[0].Parts[1].Text.clear();
    tables[2].Parts[0].LocaleKey = "Middle_None";
    tables.push_back(Table("MiddleName_Human", "en-US", { { "", "" } }));
    tables.push_back(Table("First Name", "en-US", { { "First_0", "A" } }));
    tables.push_back(Table("PetNames", "", { { "First_0", "A" } }));
    tables.push_back(Table("Empty", "en-US", {}));
    tables.push_back(Table("Pets", "en-US", { { "", "Orphan" }, { "First_1", "Bad\x01" }, { "First_2", "\xFF" }, { std::string(129, 'k'), "A" } }));
    tables.push_back(Table("FirstName_HumanMale", "de", { { "First_Boy_0", "Aaron" } }));
    CharacterNameTable large = Table("Large", "en-US", {});
    large.Parts.resize(257, CharacterNamePart{ "Key", "Text" });
    tables.push_back(std::move(large));
    std::vector<std::string> const errors = BuildErrors(std::move(tables), { { 4, 1, 2, 0, 0, 0 }, { 4, 1, Male, 0, 0, 0 } });

    for (std::string const& expected : {
        std::string("FirstName_HumanMale (en-US) position 1: a name needs a locale key and text"),
        std::string("MiddleName_Human (en-US) position 0 must be empty, since index 0 means no middle name"),
        std::string("MiddleName_Human (en-US) is listed more than once"),
        std::string("the table name 'First Name' must be 1-64 letters, digits or underscores"),
        std::string("PetNames (): the locale must be 1-16 letters, digits, underscores or dashes"),
        std::string("Empty (en-US) holds 0 parts; a table holds 1-256"),
        std::string("Pets (en-US) position 0: has text but no locale key"),
        std::string("Pets (en-US) position 1: the text holds a control character"),
        std::string("Pets (en-US) position 2: the text is not UTF-8"),
        std::string("Pets (en-US) position 3: the locale key is too long"),
        std::string("Large (en-US) holds 257 parts; a table holds 1-256"),
        std::string("the de locale has some human name tables but not FirstName_HumanFemale, MiddleName_Human, LastName_Human"),
        std::string("disallowed name 4 has gender 2; 0 is female and 1 is male"),
        std::string("disallowed name 4 is listed more than once") })
    {
        EXPECT_TRUE(Contains(errors, expected)) << "missing: " << expected;
    }

    std::vector<CharacterNameTable> noisy;
    for (int index = 0; index < 150; ++index)
        noisy.push_back(Table("Bad Table", std::to_string(index), { { "Key", "Text" } }));
    std::vector<std::string> const capped = BuildErrors(std::move(noisy));
    ASSERT_EQ(capped.size(), 101u);
    EXPECT_EQ(capped.back(), "and 50 more problems");
}
