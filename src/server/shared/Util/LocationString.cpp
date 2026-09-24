/*
 * Project Ambrose by Imjustchico
 * Parses and formats comma-separated world coordinates independently of the process locale while preserving named locations.
 */

#include "LocationString.h"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

std::optional<LocationString> LocationString::Parse(std::string_view text)
{
    if (text.empty())
        return std::nullopt;
    if (text.find(',') == std::string_view::npos)
        return Named(std::string(text));

    LocationString result;
    std::size_t coordinate = 0;
    std::size_t start = 0;
    for (std::size_t index = 0; index <= text.size(); ++index)
    {
        if (index != text.size() && text[index] != ',')
            continue;
        if (coordinate >= result.Coordinates.size())
            return std::nullopt;
        float value = 0;
        auto const [end, error] = std::from_chars(text.data() + start, text.data() + index, value);
        if (error != std::errc() || end != text.data() + index || !std::isfinite(value))
            return std::nullopt;
        result.Coordinates[coordinate++] = value;
        start = index + 1;
    }
    if (coordinate != result.Coordinates.size())
        return std::nullopt;
    result.Name.clear();
    return result;
}

LocationString LocationString::Named(std::string name)
{
    LocationString result;
    result.Name = std::move(name);
    return result;
}

LocationString LocationString::CoordinatesOf(float x, float y, float z, float yaw)
{
    LocationString result;
    result.Coordinates = { x, y, z, yaw };
    return result;
}

std::string LocationString::Format() const
{
    if (IsNamed())
        return Name;
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    for (std::size_t index = 0; index < 3; ++index)
    {
        if (index != 0)
            stream << ',';
        if (std::trunc(Coordinates[index]) == Coordinates[index])
            stream << static_cast<long long>(Coordinates[index]);
        else
            stream << std::setprecision(std::numeric_limits<float>::max_digits10) << Coordinates[index];
    }
    stream << ',' << std::fixed << std::setprecision(6) << Coordinates[3];
    return stream.str();
}
