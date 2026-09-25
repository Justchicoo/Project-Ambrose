/*
 * Project Ambrose by Imjustchico
 * Reads and writes the remembered place of the launcher's window. A place is taken only when every number is a number, the size is one the window's own screens can be read at, and the corner is somewhere a desktop could have put it; anything else is dropped and the window opens where it would have opened on a machine that had never run it. A maximised window keeps the size it had before it was maximised, so unmaximising it puts back the window the user last sized by hand.
 */

#include "LauncherPlace.h"

#include <nlohmann/json.hpp>

namespace
{
    bool Within(int value, int from, int to)
    {
        return value >= from && value <= to;
    }

    bool Number(nlohmann::json const& body, char const* name, int& into)
    {
        auto const found = body.find(name);
        if (found == body.end() || !found->is_number_integer())
            return false;
        if (found->is_number_unsigned() && found->get<unsigned long long>() > LauncherPlace::FurthestFromOrigin)
            return false;
        long long const value = found->get<long long>();
        if (value < -LauncherPlace::FurthestFromOrigin || value > LauncherPlace::FurthestFromOrigin)
            return false;
        into = static_cast<int>(value);
        return true;
    }
}

std::optional<WindowPlace> LauncherPlace::Read(std::string const& json)
{
    nlohmann::json const body = nlohmann::json::parse(json, nullptr, false);
    if (!body.is_object())
        return std::nullopt;
    if (body.value("schema", 0) != SchemaVersion)
        return std::nullopt;

    WindowPlace place;
    if (!Number(body, "x", place.X) || !Number(body, "y", place.Y) || !Number(body, "width", place.Width) || !Number(body, "height", place.Height))
        return std::nullopt;
    if (!Within(place.Width, MinimumWidth, LargestWidth) || !Within(place.Height, MinimumHeight, LargestHeight))
        return std::nullopt;

    auto const maximised = body.find("maximised");
    if (maximised != body.end())
    {
        if (!maximised->is_boolean())
            return std::nullopt;
        place.Maximised = maximised->get<bool>();
    }
    return place;
}

std::string LauncherPlace::Describe(WindowPlace const& place)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["x"] = place.X;
    body["y"] = place.Y;
    body["width"] = place.Width;
    body["height"] = place.Height;
    body["maximised"] = place.Maximised;
    return body.dump();
}
