/*
 * Project Ambrose by Imjustchico
 * Tests KIWAD header and TOC parsing for versions 1 and 2, TOC length measurement, and rejection of every truncation.
 */

#include "KiwadBuilder.h"
#include "KiwadHeader.h"

#include <gtest/gtest.h>

TEST(KiwadHeaderTest, VersionTwoWithThreeEntriesMeasuresTocLength)
{
    KiwadBuilder builder(2, 0x5A);
    builder.Add("LoginMessages.xml", "<?xml version=\"1.0\" ?>", true).Add("Locale/en/Test.lang", "text", false).Add("a", "b", true);
    std::vector<uint8> const archive = builder.Build();
    std::size_t const expected = 14 + (21 + 18) + (21 + 20) + (21 + 2);
    EXPECT_EQ(builder.GetTocLength(), expected);
    std::optional<uint64> const measured = KiwadHeader::MeasureTocLength(archive);
    ASSERT_TRUE(measured.has_value());
    EXPECT_EQ(*measured, expected);

    KiwadParseResult const result = KiwadHeader::Parse(archive);
    ASSERT_TRUE(result.Succeeded()) << result.Message;
    KiwadHeader const& header = *result.Header;
    EXPECT_EQ(header.Version, 2u);
    ASSERT_TRUE(header.Flags.has_value());
    EXPECT_EQ(*header.Flags, 0x5A);
    ASSERT_EQ(header.Entries.size(), 3u);
    EXPECT_EQ(header.Entries[0].Name, "LoginMessages.xml");
    EXPECT_TRUE(header.Entries[0].Compressed);
    EXPECT_EQ(header.Entries[0].Offset, expected);
    EXPECT_EQ(header.Entries[1].Name, "Locale/en/Test.lang");
    EXPECT_FALSE(header.Entries[1].Compressed);
    EXPECT_EQ(header.Entries[1].CompressedSize, KiwadHeader::StoredMarker);
    EXPECT_EQ(header.Entries[1].GetStoredSize(), 4u);
}

TEST(KiwadHeaderTest, VersionOneHasNoFlagByte)
{
    KiwadBuilder builder(1);
    builder.Add("Root.txt", "root", false);
    std::vector<uint8> const archive = builder.Build();
    KiwadParseResult const result = KiwadHeader::Parse(archive);
    ASSERT_TRUE(result.Succeeded()) << result.Message;
    EXPECT_FALSE(result.Header->Flags.has_value());
    EXPECT_EQ(result.Header->TocLength, 13u + 21u + 9u);
    EXPECT_EQ(KiwadHeader::GetHeaderSize(1), 13u);
    EXPECT_EQ(KiwadHeader::GetHeaderSize(2), 14u);
}

TEST(KiwadHeaderTest, EveryTruncationFailsWithoutReadingPastTheEnd)
{
    KiwadBuilder builder(2);
    builder.Add("first.bin", "0123456789", false).Add("second.bin", std::string(300, 'x'), true);
    std::vector<uint8> const archive = builder.Build();
    for (std::size_t length = 0; length < archive.size(); ++length)
    {
        std::span<uint8 const> const prefix(archive.data(), length);
        KiwadParseResult const result = KiwadHeader::Parse(prefix);
        EXPECT_FALSE(result.Succeeded()) << "length " << length;
        EXPECT_FALSE(result.Message.empty());
    }
    EXPECT_TRUE(KiwadHeader::Parse(archive).Succeeded());
}

TEST(KiwadHeaderTest, RejectsBadMagicHugeCountsAndOutOfBoundsEntries)
{
    std::vector<uint8> archive = KiwadBuilder(1).Add("x", "y", false).Build();

    std::vector<uint8> badMagic = archive;
    badMagic[0] = 'Q';
    EXPECT_EQ(KiwadHeader::Parse(badMagic).Error, KiwadError::BadMagic);

    std::vector<uint8> hugeCount = archive;
    hugeCount[9] = 0xFF;
    hugeCount[10] = 0xFF;
    hugeCount[11] = 0xFF;
    hugeCount[12] = 0x7F;
    EXPECT_EQ(KiwadHeader::Parse(hugeCount).Error, KiwadError::TooManyEntries);

    std::vector<uint8> pastEnd = archive;
    pastEnd[13] = 0xF0;
    EXPECT_EQ(KiwadHeader::Parse(pastEnd).Error, KiwadError::EntryOutOfBounds);

    std::vector<uint8> longName = archive;
    longName[13 + 17] = 0xFF;
    longName[13 + 18] = 0xFF;
    EXPECT_EQ(KiwadHeader::Parse(longName).Error, KiwadError::NameTooLong);

    std::vector<uint8> compressedWithoutSize = archive;
    compressedWithoutSize[13 + 12] = 1;
    EXPECT_EQ(KiwadHeader::Parse(compressedWithoutSize).Error, KiwadError::BadCompressedSize);
}

TEST(KiwadHeaderTest, TocLengthCanBeMeasuredFromTheTocAlone)
{
    KiwadBuilder builder(1);
    builder.Add("one.xml", std::string(4000, 'a'), true).Add("two.xml", std::string(3000, 'b'), false);
    std::vector<uint8> const archive = builder.Build();
    std::span<uint8 const> const tocOnly(archive.data(), builder.GetTocLength());
    std::optional<uint64> const measured = KiwadHeader::MeasureTocLength(tocOnly);
    ASSERT_TRUE(measured.has_value());
    EXPECT_EQ(*measured, builder.GetTocLength());
    EXPECT_EQ(KiwadHeader::Parse(tocOnly).Error, KiwadError::EntryOutOfBounds);
    EXPECT_FALSE(KiwadHeader::MeasureTocLength(std::span<uint8 const>(archive.data(), builder.GetTocLength() - 1)).has_value());
}

TEST(KiwadHeaderTest, TruncationInsideANameIsReportedAsTruncated)
{
    std::vector<uint8> const archive = KiwadBuilder(1).Add("a-long-entry-name.xml", "x", false).Build();
    KiwadParseResult const result = KiwadHeader::Parse(std::span<uint8 const>(archive.data(), 13 + 21 + 5));
    EXPECT_EQ(result.Error, KiwadError::Truncated) << result.Message;
    EXPECT_NE(result.Message.find("name"), std::string::npos);
}
