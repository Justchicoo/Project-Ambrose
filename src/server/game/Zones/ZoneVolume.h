/*
 * Project Ambrose by Imjustchico
 * A zone volume's shape from zone_volume, a sphere, an upright cylinder or a box around its position, and whether a point lies inside it, with a margin a wizard must pass beyond the edge before it counts as having left, so standing on the boundary does not fire enter and exit over and over.
 */

#ifndef AMBROSE_ZONEVOLUME_H
#define AMBROSE_ZONEVOLUME_H

#include "Types.h"

#include <string>
#include <string_view>

enum class VolumeShape : uint8
{
    Sphere,
    Cylinder,
    Box,
    Unknown
};

struct ZoneVolume
{
    uint32 Index = 0;
    std::string Name;
    VolumeShape Shape = VolumeShape::Unknown;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Radius = 0.0f;
    float Length = 0.0f;
    float Width = 0.0f;
    float Depth = 0.0f;

    static VolumeShape ShapeOf(std::string_view name) noexcept;
    bool Contains(float x, float y, float z, float margin = 0.0f) const noexcept;
};

class VolumePresence
{
public:
    static constexpr float ExitMargin = 2.0f;

    enum class Change : uint8
    {
        None,
        Entered,
        Exited
    };

    Change Update(ZoneVolume const& volume, float x, float y, float z) noexcept;
    void Place(ZoneVolume const& volume, float x, float y, float z) noexcept { _inside = volume.Contains(x, y, z); }
    bool Inside() const noexcept { return _inside; }

private:
    bool _inside = false;
};

#endif
