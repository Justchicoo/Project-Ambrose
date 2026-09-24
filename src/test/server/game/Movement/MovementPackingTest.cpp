/*
 * Project Ambrose by Imjustchico
 * Tests world coordinate quarter-unit packing, yaw byte quantization, and rejection of unsafe coordinates.
 */

#include "MovementPacking.h"

#include <gtest/gtest.h>

#include <numbers>

TEST(MovementPackingTest, CapturedCoordinatesRoundTripWithinFourUnits)
{
    for (float const value : { -2408.09f, 2609.10f, -7.13f })
    {
        std::optional<int16> const packed = MovementPacking::TryPackLocation(value);
        ASSERT_TRUE(packed.has_value());
        EXPECT_LE(std::abs(MovementPacking::UnpackLocation(*packed) - value), 4.0f);
    }
}

TEST(MovementPackingTest, YawCardinalValuesRoundTripWithinOneByteStep)
{
    float const step = 2.0f * std::numbers::pi_v<float> / 256.0f;
    for (float const yaw : { 0.0f, std::numbers::pi_v<float> / 2.0f, std::numbers::pi_v<float>, 3.0f * std::numbers::pi_v<float> / 2.0f })
        EXPECT_LE(std::abs(MovementPacking::UnpackYaw(MovementPacking::PackYaw(yaw)) - yaw), step);
}

TEST(MovementPackingTest, OutOfRangeAndNonFiniteCoordinatesAreRejected)
{
    EXPECT_FALSE(MovementPacking::TryPackLocation(-131073.0f).has_value());
    EXPECT_FALSE(MovementPacking::TryPackLocation(131073.0f).has_value());
    EXPECT_FALSE(MovementPacking::TryPackLocation(std::numeric_limits<float>::infinity()).has_value());
}
