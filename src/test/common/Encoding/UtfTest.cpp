/*
 * Project Ambrose by Imjustchico
 * Tests UTF-8 and UTF-16 round trips, the maximal subpart replacement rule, surrogate handling, and LE bytes.
 */

#include "Utf.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    constexpr char16_t Fffd = 0xFFFD;

    std::u16string Units(std::initializer_list<char16_t> units)
    {
        return std::u16string(units);
    }

    std::string const WizardUtf8 = "Wizard\xC3\xA9\xF0\x9F\x98\x80";
    std::u16string const WizardUtf16 = Units({ u'W', u'i', u'z', u'a', u'r', u'd', 0x00E9, 0xD83D, 0xDE00 });
}

TEST(UtfTest, WizardWithAccentAndEmojiRoundTrips)
{
    auto const utf16 = Utf::Utf8ToUtf16(WizardUtf8, Utf::InvalidPolicy::Reject);
    ASSERT_TRUE(utf16.has_value());
    EXPECT_EQ(*utf16, WizardUtf16);
    auto const utf8 = Utf::Utf16ToUtf8(*utf16, Utf::InvalidPolicy::Reject);
    ASSERT_TRUE(utf8.has_value());
    EXPECT_EQ(*utf8, WizardUtf8);
}

TEST(UtfTest, LittleEndianBytesRoundTrip)
{
    std::vector<uint8> const bytes = Utf::StringToUtf16LEBytes(WizardUtf16);
    std::vector<uint8> const expected{ 0x57, 0x00, 0x69, 0x00, 0x7A, 0x00, 0x61, 0x00, 0x72, 0x00, 0x64, 0x00, 0xE9, 0x00, 0x3D, 0xD8, 0x00, 0xDE };
    EXPECT_EQ(bytes, expected);
    auto const decoded = Utf::Utf16LEBytesToString(bytes, Utf::InvalidPolicy::Reject);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, WizardUtf16);
}

TEST(UtfTest, OddByteCountIsReplacedOrRejected)
{
    std::vector<uint8> const bytes{ 0x41, 0x00, 0x42 };
    EXPECT_FALSE(Utf::Utf16LEBytesToString(bytes, Utf::InvalidPolicy::Reject).has_value());
    auto const replaced = Utf::Utf16LEBytesToString(bytes, Utf::InvalidPolicy::ReplaceWithU_FFFD);
    ASSERT_TRUE(replaced.has_value());
    EXPECT_EQ(*replaced, Units({ u'A', Fffd }));
}

TEST(UtfTest, UnpairedSurrogatesAreReplacedOrRejected)
{
    std::u16string const loneHigh = Units({ 0xD800, u'A' });
    std::u16string const loneLow = Units({ u'B', 0xDC00 });
    std::u16string const reversed = Units({ 0xDE00, 0xD83D });
    EXPECT_FALSE(Utf::Utf16ToUtf8(loneHigh, Utf::InvalidPolicy::Reject).has_value());
    EXPECT_EQ(Utf::Utf16ToUtf8(loneHigh, Utf::InvalidPolicy::ReplaceWithU_FFFD), std::string("\xEF\xBF\xBD" "A"));
    EXPECT_EQ(Utf::Utf16ToUtf8(loneLow, Utf::InvalidPolicy::ReplaceWithU_FFFD), std::string("B" "\xEF\xBF\xBD"));
    EXPECT_EQ(Utf::Utf16ToUtf8(reversed, Utf::InvalidPolicy::ReplaceWithU_FFFD), std::string("\xEF\xBF\xBD\xEF\xBF\xBD"));
    EXPECT_FALSE(Utf::IsValidUtf16(loneLow));
    EXPECT_TRUE(Utf::IsValidUtf16(WizardUtf16));
}

TEST(UtfTest, UnicodeMaximalSubpartExampleMatchesTheStandard)
{
    std::string const input = "\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64";
    std::u16string const expected = Units({ u'a', Fffd, Fffd, Fffd, u'b', Fffd, u'c', Fffd, Fffd, u'd' });
    EXPECT_EQ(Utf::Utf8ToUtf16(input, Utf::InvalidPolicy::ReplaceWithU_FFFD), expected);
    EXPECT_FALSE(Utf::Utf8ToUtf16(input, Utf::InvalidPolicy::Reject).has_value());
}

TEST(UtfTest, IllFormedUtf8IsRejected)
{
    std::vector<std::string> const invalid{
        "\xC0\xAF",
        "\xE0\x80\xAF",
        "\xED\xA0\x80",
        "\xF4\x90\x80\x80",
        "\xF5\x80\x80\x80",
        "\xE2\x82",
        "\x80",
        "\xFF"
    };
    for (std::string const& sample : invalid)
    {
        EXPECT_FALSE(Utf::IsValidUtf8(sample));
        EXPECT_FALSE(Utf::Utf8ToUtf16(sample, Utf::InvalidPolicy::Reject).has_value());
    }
    EXPECT_EQ(Utf::Utf8ToUtf16("\xED\xA0\x80", Utf::InvalidPolicy::ReplaceWithU_FFFD), Units({ Fffd, Fffd, Fffd }));
    EXPECT_EQ(Utf::Utf8ToUtf16("x\xE2\x82", Utf::InvalidPolicy::ReplaceWithU_FFFD), Units({ u'x', Fffd }));
}

TEST(UtfTest, BoundaryCodePointsAreValid)
{
    EXPECT_TRUE(Utf::IsValidUtf8("\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80\xED\x9F\xBF\xEE\x80\x80\xEF\xBF\xBF\xF0\x90\x80\x80\xF4\x8F\xBF\xBF"));
}

TEST(UtfTest, EveryScalarValueRoundTrips)
{
    std::u16string utf16;
    for (char32_t codePoint = 0; codePoint <= 0x10FFFF; ++codePoint)
    {
        if (codePoint >= 0xD800 && codePoint <= 0xDFFF)
            continue;
        if (codePoint < 0x10000)
        {
            utf16.push_back(static_cast<char16_t>(codePoint));
        }
        else
        {
            char32_t const offset = codePoint - 0x10000;
            utf16.push_back(static_cast<char16_t>(0xD800 + (offset >> 10)));
            utf16.push_back(static_cast<char16_t>(0xDC00 + (offset & 0x3FF)));
        }
    }
    auto const utf8 = Utf::Utf16ToUtf8(utf16, Utf::InvalidPolicy::Reject);
    ASSERT_TRUE(utf8.has_value());
    EXPECT_TRUE(Utf::IsValidUtf8(*utf8));
    auto const back = Utf::Utf8ToUtf16(*utf8, Utf::InvalidPolicy::Reject);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, utf16);
}
