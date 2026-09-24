/*
 * Project Ambrose by Imjustchico
 * Converts the client's signed 16-bit quarter-unit coordinates and byte directions to safe world floats.
 */

#ifndef AMBROSE_MOVEMENTPACKING_H
#define AMBROSE_MOVEMENTPACKING_H

#include "Types.h"

#include <optional>

namespace MovementPacking
{
    std::optional<int16> TryPackLocation(float value) noexcept;
    float UnpackLocation(int16 value) noexcept;
    uint8 PackYaw(float radians) noexcept;
    float UnpackYaw(uint8 value) noexcept;
}

#endif
