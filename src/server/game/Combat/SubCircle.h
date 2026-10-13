/*
 * Project Ambrose by Imjustchico
 * Where a duel puts one participant: the spot on the sigil's ring a circle's angle and radius name, turned by the sigil's yaw, and the facing that turns the participant toward the centre, worked out as the client works it out from the participant's rotation and radius.
 */

#ifndef AMBROSE_SUBCIRCLE_H
#define AMBROSE_SUBCIRCLE_H

#include "PropertyValue.h"

struct SubCirclePlacement
{
    PropertyTypes::Vector3D Position;
    float Yaw = 0.0f;
};

namespace SubCircle
{
    SubCirclePlacement Place(PropertyTypes::Vector3D const& sigilPosition, float sigilYaw, float rotationDegrees, float radius) noexcept;
}

#endif
