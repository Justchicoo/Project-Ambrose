/*
 * Project Ambrose by Imjustchico
 * Tests tokenizing, trimming, case helpers, checked number parsing, and formatting.
 */

#include "StringUtil.h"
#include "Types.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

TEST(StringUtilTest, StringToRejectsUnsignedOverflow)
{
    EXPECT_FALSE(Ambrose::StringTo<uint32>("4294967296").has_value());
    EXPECT_EQ(Ambrose::StringTo<uint32>("4294967295"), uint32(4294967295u));
}

TEST(StringUtilTest, StringToParsesSignedValues)
{
    EXPECT_EQ(Ambrose::StringTo<int32>("-5"), -5);
    EXPECT_FALSE(Ambrose::StringTo<uint32>("-5").has_value());
    EXPECT_FALSE(Ambrose::StringTo<int8>("128").has_value());
}

TEST(StringUtilTest, StringToRejectsPartialAndEmptyInput)
{
    EXPECT_FALSE(Ambrose::StringTo<int32>("").has_value());
    EXPECT_FALSE(Ambrose::StringTo<int32>("12abc").has_value());
    EXPECT_FALSE(Ambrose::StringTo<int32>(" 12").has_value());
    EXPECT_FALSE(Ambrose::StringTo<int32>("+12").has_value());
}

TEST(StringUtilTest, StringToSupportsBases)
{
    EXPECT_EQ(Ambrose::StringTo<uint32>("ff", 16), uint32(255));
    EXPECT_EQ(Ambrose::StringTo<uint8>("101", 2), uint8(5));
}

TEST(StringUtilTest, StringToParsesFloatingPoint)
{
    auto const value = Ambrose::StringTo<double>("2.5");
    ASSERT_TRUE(value.has_value());
    EXPECT_DOUBLE_EQ(*value, 2.5);
    EXPECT_FALSE(Ambrose::StringTo<double>("2.5x").has_value());
}

TEST(StringUtilTest, StringToParsesBooleans)
{
    EXPECT_EQ(Ambrose::StringTo<bool>("1"), true);
    EXPECT_EQ(Ambrose::StringTo<bool>("TRUE"), true);
    EXPECT_EQ(Ambrose::StringTo<bool>("no"), false);
    EXPECT_FALSE(Ambrose::StringTo<bool>("maybe").has_value());
}

TEST(StringUtilTest, TokenizeDropsEmptyTokensWhenAsked)
{
    std::vector<std::string_view> const expected{ "a"sv, "b"sv };
    EXPECT_EQ(Ambrose::Tokenize("a  b", ' ', false), expected);
}

TEST(StringUtilTest, TokenizeKeepsEmptyTokensWhenAsked)
{
    std::vector<std::string_view> const expected{ "a"sv, ""sv, "b"sv, ""sv };
    EXPECT_EQ(Ambrose::Tokenize("a,,b,", ',', true), expected);
    EXPECT_TRUE(Ambrose::Tokenize("", ',', false).empty());
}

TEST(StringUtilTest, TrimRemovesSurroundingWhitespace)
{
    EXPECT_EQ(Ambrose::Trim("  \t hello world \r\n"), "hello world"sv);
    EXPECT_EQ(Ambrose::TrimLeft("  x "), "x "sv);
    EXPECT_EQ(Ambrose::TrimRight(" x  "), " x"sv);
    EXPECT_EQ(Ambrose::Trim("   "), ""sv);
}

TEST(StringUtilTest, CaseHelpersAreAsciiOnly)
{
    EXPECT_EQ(Ambrose::ToLower("Wizard CITY 101"), "wizard city 101");
    EXPECT_EQ(Ambrose::ToUpper("ravenwood"), "RAVENWOOD");
    EXPECT_TRUE(Ambrose::EqualsIgnoreCase("Merle", "mERLE"));
    EXPECT_FALSE(Ambrose::EqualsIgnoreCase("Merle", "Merlin"));
}

TEST(StringUtilTest, StringFormatUsesFmt)
{
    EXPECT_EQ(Ambrose::StringFormat("{} has {} pips", "Wizard", 3), "Wizard has 3 pips");
}

TEST(StringUtilTest, ForLogEscapesControlBytesAndCapsLength)
{
    EXPECT_EQ(Ambrose::ForLog("W.1.610.0"), "W.1.610.0");
    EXPECT_EQ(Ambrose::ForLog("x\nSession 9 accepted\r\t"), "x\\x0ASession 9 accepted\\x0D\\x09");
    EXPECT_EQ(Ambrose::ForLog(std::string_view("a\0b\x7F\\", 5)), "a\\x00b\\x7F\\\\");
    EXPECT_EQ(Ambrose::ForLog("caf\xC3\xA9"), "caf\xC3\xA9");
    EXPECT_EQ(Ambrose::ForLog(std::string(100, 'n'), 8), "nnnnnnnn...(100 bytes)");
    EXPECT_EQ(Ambrose::ForLog(std::string(64, '\n')).size(), 256u);
    EXPECT_EQ(Ambrose::ForLog(""), "");
}
