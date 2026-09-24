/*
 * Project Ambrose by Imjustchico
 * Represents the compact invariant world location string as either four coordinates or a named spawn location.
 */

#ifndef AMBROSE_LOCATIONSTRING_H
#define AMBROSE_LOCATIONSTRING_H

#include <array>
#include <optional>
#include <string>
#include <string_view>

struct LocationString
{
    std::array<float, 4> Coordinates{};
    std::string Name;

    bool IsNamed() const noexcept { return !Name.empty(); }

    static std::optional<LocationString> Parse(std::string_view text);
    static LocationString Named(std::string name);
    static LocationString CoordinatesOf(float x, float y, float z, float yaw);
    std::string Format() const;
};

#endif
