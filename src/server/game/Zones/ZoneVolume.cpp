/*
 * Project Ambrose by Imjustchico
 * Reads a shape's name case-insensitively and tests containment: a sphere by distance from its position, a cylinder by horizontal distance and half its length above and below, and a box by half its width, depth and length on each axis; an unknown shape contains nothing, and presence enters at the edge and leaves only past the exit margin.
 */

#include "ZoneVolume.h"
#include "StringUtil.h"

#include <cmath>

VolumeShape ZoneVolume::ShapeOf(std::string_view name) noexcept
{
    std::string const lower = Ambrose::ToLower(name);
    if (lower.find("sphere") != std::string::npos)
        return VolumeShape::Sphere;
    if (lower.find("cylinder") != std::string::npos)
        return VolumeShape::Cylinder;
    if (lower.find("box") != std::string::npos)
        return VolumeShape::Box;
    return VolumeShape::Unknown;
}

bool ZoneVolume::Contains(float x, float y, float z, float margin) const noexcept
{
    float const dx = x - X;
    float const dy = y - Y;
    float const dz = z - Z;
    switch (Shape)
    {
        case VolumeShape::Sphere:
            return dx * dx + dy * dy + dz * dz <= (Radius + margin) * (Radius + margin);
        case VolumeShape::Cylinder:
            return dx * dx + dy * dy <= (Radius + margin) * (Radius + margin) && std::fabs(dz) <= Length / 2.0f + margin;
        case VolumeShape::Box:
            return std::fabs(dx) <= Width / 2.0f + margin && std::fabs(dy) <= Depth / 2.0f + margin && std::fabs(dz) <= Length / 2.0f + margin;
        case VolumeShape::Unknown:
            break;
    }
    return false;
}

VolumePresence::Change VolumePresence::Update(ZoneVolume const& volume, float x, float y, float z) noexcept
{
    if (!_inside && volume.Contains(x, y, z))
    {
        _inside = true;
        return Change::Entered;
    }
    if (_inside && !volume.Contains(x, y, z, ExitMargin))
    {
        _inside = false;
        return Change::Exited;
    }
    return Change::None;
}
