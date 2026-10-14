/*
 * Project Ambrose by Imjustchico
 * Decodes each trigger result with the zone data's options, names it by its class with the "class " prefix dropped, or by its hash when it does not decode, counts it per result and per zone, and tallies each property value of the asked-about classes as the JSON the property renders to, sorting classes by use and values by class, property and use.
 */

#include "TriggerResultCensus.h"
#include "ObjectSerializer.h"
#include "PropertyJson.h"
#include "PropertyObject.h"
#include "ZoneExtractor.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>

namespace
{
    std::string ShortName(std::string_view name)
    {
        constexpr std::string_view prefix = "class ";
        return std::string(name.starts_with(prefix) ? name.substr(prefix.size()) : name);
    }
}

TriggerResultCensus TriggerResultCensus::Take(ZoneExtraction const& zones, TypeCatalogPtr const& catalog, std::set<std::string, std::less<>> const& detailed)
{
    TriggerResultCensus census;
    std::map<std::string, ResultClassUsage, std::less<>> usage;
    std::map<std::tuple<std::string, std::string, std::string>, std::size_t> values;
    for (ExtractedZone const& zone : zones.Zones)
    {
        std::set<std::string, std::less<>> inZone;
        for (ExtractedTrigger const& trigger : zone.Triggers)
        {
            ++census._triggers;
            for (std::vector<ExtractedTriggerResult> const* list : { &trigger.Results, &trigger.CooldownResults })
                for (ExtractedTriggerResult const& result : *list)
                {
                    ++census._results;
                    DecodeResult const decoded = result.Data ? ObjectSerializer::Decode(catalog, *result.Data, ZoneExtractor::DataOptions()) : DecodeResult{};
                    bool const read = decoded.Ok() && decoded.Object;
                    std::string const name = read ? ShortName(decoded.Object->GetClass().Name)
                                                  : result.ClassName ? ShortName(*result.ClassName) : fmt::format("#{}", result.ClassHash);
                    ResultClassUsage& counted = usage[name];
                    counted.ClassName = name;
                    ++counted.Results;
                    if (!read)
                        ++counted.Undecoded;
                    if (inZone.insert(name).second)
                        ++counted.Zones;
                    if (!read || !detailed.contains(name))
                        continue;
                    nlohmann::ordered_json const json = PropertyJson::ToJson(decoded.Object.get());
                    for (auto const& [property, value] : json.items())
                        if (property != PropertyJson::ClassKey)
                            ++values[{ name, property, value.dump() }];
                }
        }
    }
    for (auto& [name, counted] : usage)
        census._usage.push_back(std::move(counted));
    std::stable_sort(census._usage.begin(), census._usage.end(), [](ResultClassUsage const& left, ResultClassUsage const& right) { return left.Results > right.Results; });
    for (auto const& [key, count] : values)
        census._values.push_back({ std::get<0>(key), std::get<1>(key), std::get<2>(key), count });
    std::stable_sort(census._values.begin(), census._values.end(), [](ResultValueCount const& left, ResultValueCount const& right)
    {
        return std::tie(left.ClassName, left.Property) == std::tie(right.ClassName, right.Property) ? left.Results > right.Results
                                                                                                    : std::tie(left.ClassName, left.Property) < std::tie(right.ClassName, right.Property);
    });
    return census;
}
