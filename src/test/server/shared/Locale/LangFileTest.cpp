/*
 * Project Ambrose by Imjustchico
 * Tests the .lang parser on UTF-16LE files the test writes itself: the stem and entries with numeric keys that keep their leading zeros, named keys, a non-blank metadata line, non-ASCII text, CRLF and lone LF line breaks, a last entry with or without a final line break or with empty text, one or two blank padding lines after the last entry, and files over the size limit, without a byte order mark, with an odd length, an unpaired surrogate, a missing header, an empty key or an unfinished entry refused.
 */

#include "LangFile.h"
#include "Utf.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

namespace
{
    std::vector<uint8> Utf16File(std::string_view utf8, bool bom = true)
    {
        std::vector<uint8> bytes;
        if (bom)
        {
            bytes.push_back(0xFF);
            bytes.push_back(0xFE);
        }
        std::vector<uint8> const text = Utf::StringToUtf16LEBytes(*Utf::Utf8ToUtf16(utf8, Utf::InvalidPolicy::Reject));
        bytes.insert(bytes.end(), text.begin(), text.end());
        return bytes;
    }
}

TEST(LangFileTest, AStemAndItsEntriesParseWithKeysKeptAsStrings)
{
    LangParseResult const parsed = LangFile::Parse(Utf16File("1:TestTitles\r\n00001718\r\n\r\nTo the Commons!\r\nBoss\r\nTitle for a boss\r\nBoss\r\n0042\r\n\r\nCaf\xC3\xA9 Wizard|Ravenwood\r\n"));
    ASSERT_TRUE(parsed.Ok()) << parsed.Error;
    EXPECT_EQ(parsed.Stem, "TestTitles");
    ASSERT_EQ(parsed.Entries.size(), 3u);
    EXPECT_EQ(parsed.Entries[0].Key, "00001718");
    EXPECT_EQ(parsed.Entries[0].Metadata, "");
    EXPECT_EQ(parsed.Entries[0].Text, "To the Commons!");
    EXPECT_EQ(parsed.Entries[1].Key, "Boss");
    EXPECT_EQ(parsed.Entries[1].Metadata, "Title for a boss");
    EXPECT_EQ(parsed.Entries[1].Text, "Boss");
    EXPECT_EQ(parsed.Entries[2].Key, "0042");
    EXPECT_EQ(parsed.Entries[2].Text, "Caf\xC3\xA9 Wizard|Ravenwood");
    EXPECT_EQ(LangFile::MakeKey(parsed.Stem, parsed.Entries[0].Key), "TestTitles_00001718");
    EXPECT_EQ(LangFile::MakeKey("Persona, First", "0001"), "Persona, First_0001");
}

TEST(LangFileTest, LineBreaksAndTheLastEntryAreReadEitherWay)
{
    LangParseResult const bare = LangFile::Parse(Utf16File("1:Plain\n1\n\nOne\n2\n\nTwo"));
    ASSERT_TRUE(bare.Ok()) << bare.Error;
    ASSERT_EQ(bare.Entries.size(), 2u);
    EXPECT_EQ(bare.Entries[1].Text, "Two");

    LangParseResult const emptyText = LangFile::Parse(Utf16File("1:Plain\r\n1\r\n\r\n\r\n"));
    ASSERT_TRUE(emptyText.Ok()) << emptyText.Error;
    ASSERT_EQ(emptyText.Entries.size(), 1u);
    EXPECT_EQ(emptyText.Entries[0].Text, "");

    for (std::string_view const padded : { "1:Pad\r\n1\r\n\r\nOne\r\n\r\n", "1:Pad\r\n1\r\n\r\nOne\r\n\r\n\r\n", "1:Pad\r\n1\r\nnote\r\nOne\n\n" })
    {
        LangParseResult const parsed = LangFile::Parse(Utf16File(padded));
        ASSERT_TRUE(parsed.Ok()) << parsed.Error;
        ASSERT_EQ(parsed.Entries.size(), 1u);
        EXPECT_EQ(parsed.Entries[0].Key, "1");
        EXPECT_EQ(parsed.Entries[0].Text, "One");
    }
    LangParseResult const paddedEmpty = LangFile::Parse(Utf16File("1:Pad\r\n1\r\n\r\n\r\n\r\n"));
    ASSERT_TRUE(paddedEmpty.Ok()) << paddedEmpty.Error;
    ASSERT_EQ(paddedEmpty.Entries.size(), 1u);
    EXPECT_EQ(paddedEmpty.Entries[0].Text, "");
    LangParseResult const blankAfterHeader = LangFile::Parse(Utf16File("1:Pad\r\n\r\n"));
    ASSERT_TRUE(blankAfterHeader.Ok()) << blankAfterHeader.Error;
    EXPECT_TRUE(blankAfterHeader.Entries.empty());

    LangParseResult const headerOnly = LangFile::Parse(Utf16File("1:Empty\r\n"));
    ASSERT_TRUE(headerOnly.Ok()) << headerOnly.Error;
    EXPECT_EQ(headerOnly.Stem, "Empty");
    EXPECT_TRUE(headerOnly.Entries.empty());
}

TEST(LangFileTest, MalformedFilesAreRefusedWithTheReason)
{
    EXPECT_EQ(LangFile::Parse(Utf16File("1:NoBom\r\n", false)).Error, "does not start with a UTF-16LE byte order mark");
    std::vector<uint8> odd = Utf16File("1:Odd\r\n");
    odd.push_back(0x41);
    EXPECT_EQ(LangFile::Parse(odd).Error, "is 17 bytes, which cannot be whole UTF-16 units");
    std::vector<uint8> surrogate = Utf16File("1:Half\r\n");
    surrogate.push_back(0x00);
    surrogate.push_back(0xD8);
    EXPECT_EQ(LangFile::Parse(surrogate).Error, "holds UTF-16 text with an unpaired surrogate");
    EXPECT_EQ(LangFile::Parse(Utf16File("Stem\r\n1\r\n\r\nText\r\n")).Error, "does not start with a 1:<Stem> header line");
    EXPECT_EQ(LangFile::Parse(Utf16File("1:\r\n")).Error, "does not start with a 1:<Stem> header line");
    EXPECT_EQ(LangFile::Parse(Utf16File("1:Cut\r\n1\r\n\r\nOne\r\n2\r\n")).Error, "ends on line 5 in the middle of an entry, which needs a key, a metadata and a text line");
    EXPECT_EQ(LangFile::Parse(Utf16File("1:Keys\r\n1\r\n\r\nOne\r\n\r\n\r\nTwo\r\n")).Error, "holds an empty key on line 5");
    std::vector<uint8> const small = Utf16File("1:Big\r\n");
    EXPECT_EQ(LangFile::Parse(small, small.size() - 1).Error, "is 16 bytes, more than the 15 a .lang file may hold");
    EXPECT_FALSE(LangFile::Parse({}).Ok());
}
