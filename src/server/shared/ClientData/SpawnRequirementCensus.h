/*
 * Project Ambrose by Imjustchico
 * Counts the requirement classes the placed objects of extracted zones gate their spawning on, or the zone triggers gate their firing on: each object's m_spawnRequirements, or each trigger's m_requirements, is decoded through the type dump and walked into every nested RequirementList, so each class found, a nested list among them, is counted once per placement and once per zone that holds it, and once more for each placement that applies it with m_applyNOT, the items of the zone's spawners joining the placements when asked, and each gated placement keeps the set of classes it uses, so the placements that use only some of them can be picked out; requirements that do not decode, lists that hold nothing and list entries that hold no object, are counted, not guessed at.
 */

#ifndef AMBROSE_SPAWNREQUIREMENTCENSUS_H
#define AMBROSE_SPAWNREQUIREMENTCENSUS_H

#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class PropertyObject;
struct ZoneExtraction;

struct RequirementUsage
{
    std::string Type;
    std::size_t Placements = 0;
    std::size_t Zones = 0;
    std::size_t Negated = 0;
};

struct GatedPlacement
{
    std::string Zone;
    std::string Spawner;
    std::string Trigger;
    std::string ClassName;
    uint64 TemplateId = 0;
    uint32 ObjectId = 0;
    std::set<std::string, std::less<>> Types;
    std::set<std::string, std::less<>> Negated;
};

class SpawnRequirementCensus
{
public:
    static constexpr std::string_view UnnamedRequirement = "(undecoded)";
    static constexpr std::string_view NullRequirement = "(null)";

    static SpawnRequirementCensus Take(ZoneExtraction const& zones, TypeCatalogPtr const& catalog, bool spawners = false);
    static SpawnRequirementCensus TakeTriggers(ZoneExtraction const& zones, TypeCatalogPtr const& catalog);
    static std::set<std::string, std::less<>> TypesOf(PropertyObject const& list);

    std::vector<RequirementUsage> const& GetUsage() const noexcept { return _usage; }
    std::vector<GatedPlacement> const& GetGated() const noexcept { return _gated; }
    std::vector<GatedPlacement const*> UsingOnly(std::set<std::string, std::less<>> const& allowed) const;
    std::size_t GetPlacementCount() const noexcept { return _placements; }
    std::size_t GetZoneCount() const noexcept { return _zones; }
    std::size_t GetGatedZoneCount() const noexcept { return _gatedZones; }
    std::size_t GetUndecodedCount() const noexcept { return _undecoded; }
    std::size_t GetEmptyCount() const noexcept { return _empty; }
    std::size_t GetNegatedListCount() const noexcept { return _negatedLists; }

private:
    class Counter;

    std::vector<RequirementUsage> _usage;
    std::vector<GatedPlacement> _gated;
    std::size_t _placements = 0;
    std::size_t _zones = 0;
    std::size_t _gatedZones = 0;
    std::size_t _undecoded = 0;
    std::size_t _empty = 0;
    std::size_t _negatedLists = 0;
};

#endif
