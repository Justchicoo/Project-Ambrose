/*
 * Project Ambrose by Imjustchico
 * Places a participant on the ring at the sigil's yaw less the circle's angle in radians, x along the cosine and y against the sine, at the sigil's height, facing the centre with a yaw kept between zero and a full turn.
 */

#include "SubCircle.h"

#include <cmath>
#include <numbers>

SubCirclePlacement SubCircle::Place(PropertyTypes::Vector3D const& sigilPosition, float sigilYaw, float rotationDegrees, float radius) noexcept
{
    double const angle = static_cast<double>(sigilYaw) - static_cast<double>(rotationDegrees) * std::numbers::pi / 180.0;
    double const x = sigilPosition.X + std::cos(angle) * radius;
    double const y = sigilPosition.Y - std::sin(angle) * radius;
    double yaw = std::atan2(x - sigilPosition.X, y - sigilPosition.Y);
    if (yaw < 0.0)
        yaw += 2.0 * std::numbers::pi;
    SubCirclePlacement placement;
    placement.Position = {static_cast<float>(x), static_cast<float>(y), sigilPosition.Z};
    placement.Yaw = static_cast<float>(yaw);
    return placement;
}
