/*
 * Project Ambrose by Imjustchico
 * Follows MoveBehavior::PackPositionOrientation and UnpackPositionOrientation in the r806919 client, which multiply a coordinate by 0.25 and truncate it to a short, read it back times 4, multiply a facing by 40 and keep the low byte, and read it back times 0.025; a facing is first brought into one turn from 0, where the client's own facings lie, and a coordinate a short cannot hold, or one that is not a finite number, is refused rather than wrapped.
 */

#include "MovementPacking.h"

#include <cmath>
#include <limits>
#include <numbers>

namespace
{
    constexpr float FullTurn = 2.0f * std::numbers::pi_v<float>;
}

std::optional<int16> MovementPacking::TryPackLocation(float value) noexcept
{
    if (!std::isfinite(value))
        return std::nullopt;
    float const steps = std::trunc(value / LocationScale);
    if (steps < static_cast<float>(std::numeric_limits<int16>::min()) || steps > static_cast<float>(std::numeric_limits<int16>::max()))
        return std::nullopt;
    return static_cast<int16>(steps);
}

float MovementPacking::UnpackLocation(int16 value) noexcept
{
    return static_cast<float>(value) * LocationScale;
}

uint8 MovementPacking::PackYaw(float radians) noexcept
{
    if (!std::isfinite(radians))
        return 0;
    float normalized = std::fmod(radians, FullTurn);
    if (normalized < 0.0f)
        normalized += FullTurn;
    return static_cast<uint8>(static_cast<int32>(normalized * YawSteps) & 0xFF);
}

float MovementPacking::UnpackYaw(uint8 value) noexcept
{
    return static_cast<float>(value) * YawStep;
}
