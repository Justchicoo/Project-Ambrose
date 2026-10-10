/*
 * Project Ambrose by Imjustchico
 * Tests what a skipped value is read as: a text of compact length and a text of the 16-bit length a stream without compact lengths writes both give their text, a value with bits left after its text or with bytes that are not printable gives none, and the readings name the 16-bit text.
 */

#include "SkippedValue.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

TEST(SkippedValueTest, ATextOfCompactLengthAndATextOfSixteenBitLengthBothReadAsTheirText)
{
    std::vector<uint8> const compact{ 0x08, 'k', 'e', 'p', 't' };
    EXPECT_EQ(SkippedValue::Text(compact.size() * 8, compact), "kept");

    std::vector<uint8> const plain{ 0x0F, 0x00, 'T', 'e', 's', 't', 'R', 'i', 'n', 'g', 'O', 'f', 'E', 'i', 'g', 'h', 't' };
    EXPECT_EQ(SkippedValue::Text(plain.size() * 8, plain), "TestRingOfEight");
    std::vector<std::string> const readings = SkippedValue::Readings(plain.size() * 8, plain);
    EXPECT_TRUE(std::find(readings.begin(), readings.end(), "std::string \"TestRingOfEight\" of 16-bit length") != readings.end());
}

TEST(SkippedValueTest, AValueWithBitsLeftOrBytesThatAreNotPrintableGivesNoText)
{
    std::vector<uint8> const longer{ 0x02, 0x00, 'h', 'i', 0x00 };
    EXPECT_FALSE(SkippedValue::Text(longer.size() * 8, longer));
    std::vector<uint8> const binary{ 0x02, 0x00, 0x01, 0x02 };
    EXPECT_FALSE(SkippedValue::Text(binary.size() * 8, binary));
    std::vector<uint8> const word{ 0x07, 0x00, 0x00, 0x00 };
    EXPECT_FALSE(SkippedValue::Text(word.size() * 8, word));
}
