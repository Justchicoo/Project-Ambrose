/*
 * Project Ambrose by Imjustchico
 * Takes the spawn requirement census of extracted zones, decoding each placed object's requirement bytes with the zone data options and walking every RequirementList it holds no deeper than a fixed depth.
 */

#include "SpawnRequirementCensus.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "ZoneExtractor.h"

#include <algorithm>
#include <map>

namespace
{
    constexpr std::string_view ClassPrefix = "class ";
    constexpr std::string_view ListClass = "class RequirementList";
    constexpr std::size_t MaxDepth = 16;

    std::string TypeOf(std::string_view className)
    {
        if (className.starts_with(ClassPrefix))
            className.remove_prefix(ClassPrefix.size());
        return std::string(className);
    }

    void Walk(PropertyObject const& list, std::size_t depth, std::set<std::string, std::less<>>& types)
    {
        PropertyValue const* const held = list.Get("m_requirements");
        PropertyValue::List const* const entries = held ? held->GetList() : nullptr;
        if (!entries)
            return;
        for (PropertyValue const& entry : *entries)
        {
            PropertyObject const* const requirement = entry.AsObject();
            if (!requirement)
            {
                types.emplace(SpawnRequirementCensus::UnnamedRequirement);
                continue;
            }
            types.insert(TypeOf(requirement->GetClass().Name));
            if (requirement->IsA(ListClass))
            {
                if (depth < MaxDepth)
                    Walk(*requirement, depth + 1, types);
                else
                    types.emplace(SpawnRequirementCensus::UnnamedRequirement);
            }
        }
    }
}

std::set<std::string, std::less<>> SpawnRequirementCensus::TypesOf(PropertyObject const& list)
{
    std::set<std::string, std::less<>> types;
    Walk(list, 0, types);
    return types;
}

SpawnRequirementCensus SpawnRequirementCensus::Take(ZoneExtraction const& zones, TypeCatalogPtr const& catalog, bool spawners)
{
    SpawnRequirementCensus census;
    std::map<std::string, RequirementUsage, std::less<>> usage;
    census._zones = zones.Zones.size();
    for (ExtractedZone const& zone : zones.Zones)
    {
        std::set<std::string, std::less<>> zoneTypes;
        bool gated = false;
        auto const count = [&](ExtractedObject const& object, std::string const& spawner)
        {
            ++census._placements;
            if (!object.SpawnRequirements)
                return;
            gated = true;
            GatedPlacement placement{ zone.Path, spawner, object.ClassName, object.TemplateId, object.ObjectId, {} };
            DecodeResult const decoded = ObjectSerializer::Decode(catalog, *object.SpawnRequirements, ZoneExtractor::DataOptions());
            if (!decoded.Ok() || !decoded.Object)
            {
                ++census._undecoded;
                placement.Types.emplace(UnnamedRequirement);
            }
            else
                placement.Types = TypesOf(*decoded.Object);
            if (placement.Types.empty())
                ++census._empty;
            for (std::string const& type : placement.Types)
            {
                RequirementUsage& counted = usage[type];
                counted.Type = type;
                ++counted.Placements;
                if (zoneTypes.insert(type).second)
                    ++counted.Zones;
            }
            census._gated.push_back(std::move(placement));
        };
        for (ExtractedObject const& object : zone.Objects)
            count(object, std::string());
        if (spawners)
            for (ExtractedSpawner const& spawner : zone.Spawners)
                for (ExtractedSpawnItem const& item : spawner.Items)
                    count(item.Object, spawner.Name);
        if (gated)
            ++census._gatedZones;
    }
    for (auto& [type, counted] : usage)
        census._usage.push_back(std::move(counted));
    std::stable_sort(census._usage.begin(), census._usage.end(), [](RequirementUsage const& left, RequirementUsage const& right) { return left.Placements > right.Placements; });
    return census;
}

std::vector<GatedPlacement const*> SpawnRequirementCensus::UsingOnly(std::set<std::string, std::less<>> const& allowed) const
{
    std::vector<GatedPlacement const*> found;
    for (GatedPlacement const& placement : _gated)
    {
        bool const fits = !placement.Types.empty() && std::all_of(placement.Types.begin(), placement.Types.end(),
            [&allowed](std::string const& type) { return type == TypeOf(ListClass) || allowed.contains(type); });
        if (fits)
            found.push_back(&placement);
    }
    return found;
}
