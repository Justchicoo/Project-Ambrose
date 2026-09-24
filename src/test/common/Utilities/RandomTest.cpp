/*
 * Project Ambrose by Imjustchico
 * Tests random helper ranges, distribution coverage, and standard engine compatibility.
 */

#include "Random.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <numeric>
#include <random>

TEST(RandomTest, SingleValueRangeReturnsThatValue)
{
    EXPECT_EQ(urand(1, 1), 1u);
    EXPECT_EQ(irand(-7, -7), -7);
    EXPECT_FLOAT_EQ(frand(2.5f, 2.5f), 2.5f);
}

TEST(RandomTest, MillionDrawsStayInRangeAndCoverEveryValue)
{
    std::array<int, 10> counts{};
    for (int i = 0; i < 1000000; ++i)
    {
        uint32 const value = urand(0, 9);
        ASSERT_LE(value, 9u);
        ++counts[value];
    }
    for (int count : counts)
        EXPECT_GT(count, 90000);
}

TEST(RandomTest, SwappedBoundsAreAccepted)
{
    for (int i = 0; i < 1000; ++i)
    {
        int32 const value = irand(5, -5);
        ASSERT_GE(value, -5);
        ASSERT_LE(value, 5);
    }
}

TEST(RandomTest, FloatAndNormalizedRanges)
{
    for (int i = 0; i < 10000; ++i)
    {
        float const f = frand(-1.0f, 1.0f);
        ASSERT_GE(f, -1.0f);
        ASSERT_LT(f, 1.0f);
        double const n = rand_norm();
        ASSERT_GE(n, 0.0);
        ASSERT_LT(n, 1.0);
    }
}

TEST(RandomTest, ChanceRollsRespectBounds)
{
    for (int i = 0; i < 1000; ++i)
    {
        EXPECT_FALSE(roll_chance_i(0));
        EXPECT_TRUE(roll_chance_i(100));
        EXPECT_FALSE(roll_chance_f(0.0f));
        EXPECT_TRUE(roll_chance_f(100.0f));
    }
}

TEST(RandomTest, EngineWorksWithStandardAlgorithms)
{
    std::array<int, 8> values{};
    std::iota(values.begin(), values.end(), 0);
    std::shuffle(values.begin(), values.end(), RandomEngine::Instance());
    std::sort(values.begin(), values.end());
    EXPECT_EQ(values, (std::array<int, 8>{ 0, 1, 2, 3, 4, 5, 6, 7 }));
    int const roll = std::uniform_int_distribution<int>(1, 6)(RandomEngine::Instance());
    EXPECT_GE(roll, 1);
    EXPECT_LE(roll, 6);
}
