/*
 * Project Ambrose by Imjustchico
 * Renders a property object as ordered JSON for tools and debugging, keeping its class name and property order and naming enum and flag values.
 */

#ifndef AMBROSE_PROPERTYJSON_H
#define AMBROSE_PROPERTYJSON_H

#include "PropertyObject.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <string_view>

namespace PropertyJson
{
    inline constexpr std::string_view ClassKey = "$class";

    nlohmann::ordered_json ToJson(PropertyObject const* object);
    std::string Dump(PropertyObject const* object, int indent = 2);
}

#endif
