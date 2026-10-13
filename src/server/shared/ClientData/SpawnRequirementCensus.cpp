/*
 * Project Ambrose by Imjustchico
 * Takes the requirement census of extracted zones, of their placed objects' spawn requirements or of their triggers' requirements, decoding each one's bytes with the zone data options and walking every RequirementList it holds no deeper than a fixed depth.
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

    using TypeSet = std::set<std::string, std::less<>>;

    std::string TypeOf(std::string_view className)
    {
        if (className.starts_with(ClassPrefix))
            className.remove_prefix(ClassPrefix.size());
        return std::string(className);
    }

    bool AppliesNot(PropertyObject const& requirement)
    {
        PropertyValue const* const value = requirement.Get("m_applyNOT");
        bool const* const flag = value ? value->GetIf<bool>() : nullptr;
        return flag && *flag;
    }

    void Walk(PropertyObject const& list, std::size_t depth, TypeSet& types, TypeSet* negated)
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
                types.emplace(SpawnRequirementCensus::NullRequirement);
                continue;
            }
            std::string type = TypeOf(requirement->GetClass().Name);
            if (negated && AppliesNot(*requirement))
                negated->insert(type);
            types.insert(std::move(type));
            if (requirement->IsA(ListClass))
            {
                if (depth < MaxDepth)
                    Walk(*requirement, depth + 1, types, negated);
                else
                    types.emplace(SpawnRequirementCensus::UnnamedRequirement);
            }
        }
    }
}

class SpawnRequirementCensus::Counter
{
public:
    Counter(SpawnRequirementCensus& census, TypeCatalogPtr const& catalog) : _census(census), _catalog(catalog) { }

    void BeginZone()
    {
        _zoneTypes.clear();
        _gated = false;
    }

    void EndZone()
    {
        if (_gated)
            ++_census._gatedZones;
    }

    void Count(GatedPlacement placement, std::optional<std::vector<uint8>> const& requirements)
    {
        ++_census._placements;
        if (!requirements)
            return;
        _gated = true;
        DecodeResult const decoded = ObjectSerializer::Decode(_catalog, *requirements, ZoneExtractor::DataOptions());
        if (!decoded.Ok() || !decoded.Object)
        {
            ++_census._undecoded;
            placement.Types.emplace(UnnamedRequirement);
        }
        else
        {
            if (AppliesNot(*decoded.Object))
                ++_census._negatedLists;
            Walk(*decoded.Object, 0, placement.Types, &placement.Negated);
        }
        if (placement.Types.empty())
            ++_census._empty;
        for (std::string const& type : placement.Types)
        {
            RequirementUsage& counted = _usage[type];
            counted.Type = type;
            ++counted.Placements;
            if (placement.Negated.contains(type))
                ++counted.Negated;
            if (_zoneTypes.insert(type).second)
                ++counted.Zones;
        }
        _census._gated.push_back(std::move(placement));
    }

    void Finish()
    {
        for (auto& [type, counted] : _usage)
            _census._usage.push_back(std::move(counted));
        std::stable_sort(_census._usage.begin(), _census._usage.end(), [](RequirementUsage const& left, RequirementUsage const& right) { return left.Placements > right.Placements; });
    }

private:
    SpawnRequirementCensus& _census;
    TypeCatalogPtr const& _catalog;
    std::map<std::string, RequirementUsage, std::less<>> _usage;
    TypeSet _zoneTypes;
    bool _gated = false;
};

std::set<std::string, std::less<>> SpawnRequirementCensus::TypesOf(PropertyObject const& list)
{
    TypeSet types;
    Walk(list, 0, types, nullptr);
    return types;
}

SpawnRequirementCensus SpawnRequirementCensus::Take(ZoneExtraction const& zones, TypeCatalogPtr const& catalog, bool spawners)
{
    SpawnRequirementCensus census;
    census._zones = zones.Zones.size();
    Counter counter(census, catalog);
    for (ExtractedZone const& zone : zones.Zones)
    {
        counter.BeginZone();
        for (ExtractedObject const& object : zone.Objects)
            counter.Count({ zone.Path, {}, {}, object.ClassName, object.TemplateId, object.ObjectId, {}, {} }, object.SpawnRequirements);
        if (spawners)
            for (ExtractedSpawner const& spawner : zone.Spawners)
                for (ExtractedSpawnItem const& item : spawner.Items)
                    counter.Count({ zone.Path, spawner.Name, {}, item.Object.ClassName, item.Object.TemplateId, item.Object.ObjectId, {}, {} }, item.Object.SpawnRequirements);
        counter.EndZone();
    }
    counter.Finish();
    return census;
}

SpawnRequirementCensus SpawnRequirementCensus::TakeTriggers(ZoneExtraction const& zones, TypeCatalogPtr const& catalog)
{
    SpawnRequirementCensus census;
    census._zones = zones.Zones.size();
    Counter counter(census, catalog);
    for (ExtractedZone const& zone : zones.Zones)
    {
        counter.BeginZone();
        for (ExtractedTrigger const& trigger : zone.Triggers)
            counter.Count({ zone.Path, {}, trigger.Name, trigger.ClassName, 0, 0, {}, {} }, trigger.Requirements);
        counter.EndZone();
    }
    counter.Finish();
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
