/*
 * Project Ambrose by Imjustchico
 * Implements quarter-unit signed coordinate packing and a full-turn 8-bit yaw quantizer.
 */

#include "MovementPacking.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace
{
    constexpr float CoordinateScale = 4.0f;
    constexpr float FullTurn = 2.0f * std::numbers::pi_v<float>;
}

std::optional<int16> MovementPacking::TryPackLocation(float value) noexcept
{
    if (!std::isfinite(value))
        return std::nullopt;
    constexpr float minimum = static_cast<float>(std::numeric_limits<int16>::min()) * CoordinateScale;
    constexpr float maximum = static_cast<float>(std::numeric_limits<int16>::max()) * CoordinateScale;
    if (value < minimum || value > maximum)
        return std::nullopt;
    float const scaled = std::round(value / CoordinateScale);
    if (scaled < static_cast<float>(std::numeric_limits<int16>::min()) || scaled > static_cast<float>(std::numeric_limits<int16>::max()))
        return std::nullopt;
    return static_cast<int16>(scaled);
}

float MovementPacking::UnpackLocation(int16 value) noexcept
{
    return static_cast<float>(value) * CoordinateScale;
}

uint8 MovementPacking::PackYaw(float radians) noexcept
{
    if (!std::isfinite(radians))
        return 0;
    float normalized = std::fmod(radians, FullTurn);
    if (normalized < 0)
        normalized += FullTurn;
    return static_cast<uint8>(std::lround(normalized * 256.0f / FullTurn)) & 0xFFu;
}

float MovementPacking::UnpackYaw(uint8 value) noexcept
{
    return static_cast<float>(value) * FullTurn / 256.0f;
}
