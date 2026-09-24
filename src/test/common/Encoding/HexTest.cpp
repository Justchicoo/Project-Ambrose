/*
 * Project Ambrose by Imjustchico
 * Tests hex encoding, strict decoding, and the exact hexdump -C line layout.
 */

#include "Hex.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(HexTest, EncodesInBothCases)
{
    std::vector<uint8> const bytes{ 0x00, 0x0F, 0xAB, 0xFF };
    EXPECT_EQ(Hex::Encode(bytes), "000fabff");
    EXPECT_EQ(Hex::Encode(bytes, Hex::Case::Upper), "000FABFF");
    EXPECT_EQ(Hex::Encode(std::vector<uint8>{}), "");
}

TEST(HexTest, DecodesMixedCaseAndRejectsBadInput)
{
    EXPECT_EQ(Hex::Decode("000fABff"), (std::vector<uint8>{ 0x00, 0x0F, 0xAB, 0xFF }));
    EXPECT_EQ(Hex::Decode(""), std::vector<uint8>{});
    EXPECT_FALSE(Hex::Decode("abc").has_value());
    EXPECT_FALSE(Hex::Decode("zz").has_value());
    EXPECT_FALSE(Hex::Decode("0x12").has_value());
}

TEST(HexTest, DumpMatchesHexdumpLayout)
{
    std::string const text = "Hello Wizard0123ABCD";
    std::vector<uint8> bytes(text.begin(), text.end());
    bytes[5] = 0x00;
    std::string const expected =
        std::string("00000000  48 65 6c 6c 6f 00 57 69  7a 61 72 64 30 31 32 33  |Hello.Wizard0123|") + "\n" +
        "00000010  41 42 43 44" + std::string(39, ' ') + "|ABCD|" + "\n";
    EXPECT_EQ(Hex::Dump(bytes), expected);
}

TEST(HexTest, DumpUsesTheStartOffset)
{
    std::vector<uint8> const bytes{ 0x7E, 0x7F };
    EXPECT_EQ(Hex::Dump(bytes, 0x1000), "00001000  7e 7f" + std::string(45, ' ') + "|~.|" + "\n");
    EXPECT_EQ(Hex::Dump(std::vector<uint8>{}), "");
}
