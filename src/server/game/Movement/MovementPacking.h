/*
 * Project Ambrose by Imjustchico
 * Packs and unpacks positions and facings the way the client's own MoveBehavior does: a coordinate travels as a signed 16-bit count of four-unit steps and a facing as a byte of 1/40-radian steps, both truncated toward zero when packed.
 */

#ifndef AMBROSE_MOVEMENTPACKING_H
#define AMBROSE_MOVEMENTPACKING_H

#include "Types.h"

#include <optional>

namespace MovementPacking
{
    inline constexpr float LocationScale = 4.0f;
    inline constexpr float YawSteps = 40.0f;
    inline constexpr float YawStep = 0.025f;

    std::optional<int16> TryPackLocation(float value) noexcept;
    float UnpackLocation(int16 value) noexcept;
    uint8 PackYaw(float radians) noexcept;
    float UnpackYaw(uint8 value) noexcept;
}

#endif
