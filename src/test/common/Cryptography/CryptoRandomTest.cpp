/*
 * Project Ambrose by Imjustchico
 * Tests the secure random generator for filled buffers, distinct draws, rough byte uniformity, and unbiased bounds.
 */

#include "ConstantTime.h"
#include "CryptoRandom.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <set>

TEST(CryptoRandomTest, DrawsAreFilledAndDistinct)
{
    std::array<uint8, 32> const first = Ambrose::Crypto::GetRandomArray<32>();
    std::array<uint8, 32> const second = Ambrose::Crypto::GetRandomArray<32>();
    EXPECT_NE(first, second);
    EXPECT_FALSE(std::all_of(first.begin(), first.end(), [](uint8 byte) { return byte == 0; }));
    EXPECT_EQ(Ambrose::Crypto::GetRandomBytes(0).size(), 0u);
    EXPECT_EQ(Ambrose::Crypto::GetRandomBytes(1000).size(), 1000u);
    std::set<uint64> values;
    for (int i = 0; i < 1000; ++i)
        values.insert(Ambrose::Crypto::GetRandomUInt64());
    EXPECT_EQ(values.size(), 1000u);
}

TEST(CryptoRandomTest, ByteValuesAreRoughlyUniform)
{
    std::vector<uint8> const bytes = Ambrose::Crypto::GetRandomBytes(256 * 4096);
    std::array<std::size_t, 256> counts{};
    for (uint8 const byte : bytes)
        ++counts[byte];
    double chiSquare = 0.0;
    for (std::size_t const count : counts)
    {
        double const difference = static_cast<double>(count) - 4096.0;
        chiSquare += difference * difference / 4096.0;
    }
    EXPECT_LT(chiSquare, 400.0);
    EXPECT_GT(chiSquare, 130.0);
}

TEST(CryptoRandomTest, BoundedValuesStayInRangeAndCoverIt)
{
    std::array<int, 7> seen{};
    for (int i = 0; i < 7000; ++i)
    {
        uint32 const value = Ambrose::Crypto::GetRandomBelow(7);
        ASSERT_LT(value, 7u);
        ++seen[value];
    }
    for (int const count : seen)
        EXPECT_GT(count, 700);
    EXPECT_EQ(Ambrose::Crypto::GetRandomBelow(1), 0u);
    EXPECT_THROW(Ambrose::Crypto::GetRandomBelow(0), std::invalid_argument);
}

TEST(CryptoRandomTest, LargeBoundsRejectInsteadOfBiasing)
{
    constexpr uint32 Bound = 0xC0000000u;
    constexpr int Draws = 8000;
    int lowThird = 0;
    for (int i = 0; i < Draws; ++i)
    {
        uint32 const value = Ambrose::Crypto::GetRandomBelow(Bound);
        ASSERT_LT(value, Bound);
        if (value < 0x40000000u)
            ++lowThird;
    }
    double const fraction = static_cast<double>(lowThird) / Draws;
    EXPECT_GT(fraction, 0.29);
    EXPECT_LT(fraction, 0.38);
}

TEST(ConstantTimeTest, EqualsComparesLengthAndContent)
{
    uint8 const a[] = { 1, 2, 3, 4 };
    uint8 const b[] = { 1, 2, 3, 4 };
    uint8 const c[] = { 1, 2, 3, 5 };
    EXPECT_TRUE(Ambrose::Crypto::ConstantTimeEquals(a, b));
    EXPECT_FALSE(Ambrose::Crypto::ConstantTimeEquals(a, c));
    EXPECT_FALSE(Ambrose::Crypto::ConstantTimeEquals(std::span<uint8 const>(a, 3), b));
    EXPECT_TRUE(Ambrose::Crypto::ConstantTimeEquals(std::span<uint8 const>(), std::span<uint8 const>()));
}
