/*
 * Project Ambrose by Imjustchico
 * Tests the client's own packing: coordinates truncate to four-unit steps toward zero and read back within four units, the bytes a client sends read back as it meant them, a facing reads back within one 1/40-radian step, a turn below zero comes back into one turn, and coordinates a short cannot hold or that are not finite are refused.
 */

#include "MovementPacking.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <numbers>

TEST(MovementPackingTest, CoordinatesTruncateTowardZeroAndRoundTripWithinFourUnits)
{
    for (float const value : { -2408.09f, 2609.10f, -7.13f, -5.628328f, -1531.451f, -30.48013f })
    {
        std::optional<int16> const packed = MovementPacking::TryPackLocation(value);
        ASSERT_TRUE(packed.has_value());
        EXPECT_EQ(*packed, static_cast<int16>(static_cast<int32>(value * 0.25f))) << "the client's own cast of the quarter";
        EXPECT_LT(std::abs(MovementPacking::UnpackLocation(*packed) - value), 4.0f);
    }
    EXPECT_EQ(MovementPacking::TryPackLocation(-7.13f), -1);
    EXPECT_EQ(MovementPacking::TryPackLocation(7.99f), 1);
    EXPECT_FLOAT_EQ(MovementPacking::UnpackLocation(static_cast<int16>(0xFE81)), -1532.0f) << "a short the client sent reads back signed";
}

TEST(MovementPackingTest, FacingsReadBackWithinOneStep)
{
    float const step = 1.0f / MovementPacking::YawSteps;
    for (float const yaw : { 0.0f, 1.5f, std::numbers::pi_v<float>, 6.25f })
    {
        float const back = MovementPacking::UnpackYaw(MovementPacking::PackYaw(yaw));
        EXPECT_LE(back, yaw + 1e-5f);
        EXPECT_LT(yaw - back, step + 1e-5f);
    }
    EXPECT_EQ(MovementPacking::PackYaw(1.5f), 60) << "1.5 radians is 60 of the client's steps";
    EXPECT_FLOAT_EQ(MovementPacking::UnpackYaw(60), 1.5f);
    EXPECT_EQ(MovementPacking::PackYaw(-0.5f), MovementPacking::PackYaw(2.0f * std::numbers::pi_v<float> - 0.5f));
    EXPECT_EQ(MovementPacking::PackYaw(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(MovementPackingTest, OutOfRangeAndNonFiniteCoordinatesAreRejected)
{
    EXPECT_FALSE(MovementPacking::TryPackLocation(-131077.0f).has_value());
    EXPECT_FALSE(MovementPacking::TryPackLocation(131072.0f).has_value());
    EXPECT_TRUE(MovementPacking::TryPackLocation(131071.0f).has_value());
    EXPECT_FALSE(MovementPacking::TryPackLocation(std::numeric_limits<float>::infinity()).has_value());
    EXPECT_FALSE(MovementPacking::TryPackLocation(std::numeric_limits<float>::quiet_NaN()).has_value());
}
