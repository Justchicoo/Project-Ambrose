/*
 * Project Ambrose by Imjustchico
 * Counts the requirement classes the placed objects of extracted zones gate their spawning on: each object's m_spawnRequirements is decoded through the type dump and walked into every nested RequirementList, so each class found, a nested list among them, is counted once per placement and once per zone that holds it, the items of the zone's spawners joining the placements when asked, and each gated placement keeps the set of classes it uses, so the placements that use only some of them can be picked out; requirements that do not decode, and lists that hold nothing, are counted, not guessed at.
 */

#ifndef AMBROSE_SPAWNREQUIREMENTCENSUS_H
#define AMBROSE_SPAWNREQUIREMENTCENSUS_H

#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
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
};

struct GatedPlacement
{
    std::string Zone;
    std::string Spawner;
    std::string ClassName;
    uint64 TemplateId = 0;
    uint32 ObjectId = 0;
    std::set<std::string, std::less<>> Types;
};

class SpawnRequirementCensus
{
public:
    static constexpr std::string_view UnnamedRequirement = "(undecoded)";

    static SpawnRequirementCensus Take(ZoneExtraction const& zones, TypeCatalogPtr const& catalog, bool spawners = false);
    static std::set<std::string, std::less<>> TypesOf(PropertyObject const& list);

    std::vector<RequirementUsage> const& GetUsage() const noexcept { return _usage; }
    std::vector<GatedPlacement> const& GetGated() const noexcept { return _gated; }
    std::vector<GatedPlacement const*> UsingOnly(std::set<std::string, std::less<>> const& allowed) const;
    std::size_t GetPlacementCount() const noexcept { return _placements; }
    std::size_t GetZoneCount() const noexcept { return _zones; }
    std::size_t GetGatedZoneCount() const noexcept { return _gatedZones; }
    std::size_t GetUndecodedCount() const noexcept { return _undecoded; }
    std::size_t GetEmptyCount() const noexcept { return _empty; }

private:
    std::vector<RequirementUsage> _usage;
    std::vector<GatedPlacement> _gated;
    std::size_t _placements = 0;
    std::size_t _zones = 0;
    std::size_t _gatedZones = 0;
    std::size_t _undecoded = 0;
    std::size_t _empty = 0;
};

#endif
