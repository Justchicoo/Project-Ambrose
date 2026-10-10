/*
 * Project Ambrose by Imjustchico
 * Every zone's spawners from zone_spawner and zone_spawner_entry (sSpawnerMgr), with the start node rules of a spawn that stands on a path, read at start and again by `.reload zone_spawner`, which builds the new set off to the side, validates it and swaps it in only when every row is good, keeping the old set and reporting each error otherwise; and what each spawner does in each running zone instance: it keeps as many of its objects alive as its count allows, choosing each by its entries' chances, brings one back its respawn time after one is taken away, that time scaled by Rate.Respawn as it stands at the moment of the despawn, and never holds more than its count. A game master's own spawns are placed and taken away here too, and so is any object a despawn effect takes away; and the ResSpawn and ResDespawn results zone_trigger_result holds for the zone's triggers, read with the spawners, which start a spawner in the instance whose trigger fired or stop it and take its objects away with the effect they name.
 */

#ifndef AMBROSE_SPAWNERMGR_H
#define AMBROSE_SPAWNERMGR_H

#include "Map.h"
#include "MapObjectSpawner.h"
#include "PathWalkers.h"
#include "ReloadableStore.h"
#include "TypeRegistry.h"
#include "Types.h"
#include "ZoneMgr.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ZoneSpawnEntry
{
    uint32 Position = 0;
    uint32 PercentChance = 0;
    ZoneObjectSpawn Object;
    int32 StartNodeType = 0;
    uint32 StartNode = 0;
    uint64 PathId = 0;
    int32 UniqueLoc = 0;

    bool operator==(ZoneSpawnEntry const&) const = default;
    bool HasPlace() const noexcept;
};

struct ZoneSpawner
{
    uint32 Index = 0;
    std::string Name;
    uint64 SpawnerId = 0;
    bool Active = true;
    uint32 MaxSpawns = 0;
    uint32 RespawnSeconds = 0;
    bool HasRequirements = false;
    std::vector<ZoneSpawnEntry> Entries;

    bool operator==(ZoneSpawner const&) const = default;
    bool Spawns(std::optional<bool> active = std::nullopt) const noexcept;
    uint32 TotalChance() const noexcept;
    uint32 WeightOf(ZoneSpawnEntry const& entry) const noexcept;
};

struct ZoneSpawnResult
{
    std::string Trigger;
    uint32 Position = 0;
    bool Despawn = false;
    uint64 SpawnerId = 0;
    bool Activate = false;
    uint32 TemplateId = 0;
    std::string Effect;

    bool operator==(ZoneSpawnResult const&) const = default;
};

class ZoneSpawners
{
public:
    ZoneSpawners() = default;
    explicit ZoneSpawners(std::map<std::string, std::vector<ZoneSpawner>, std::less<>> byZone);

    std::vector<ZoneSpawner> const* In(std::string_view zone) const;
    std::vector<ZoneSpawnResult> const* ResultsIn(std::string_view zone) const;
    void SetResults(std::map<std::string, std::vector<ZoneSpawnResult>, std::less<>> results);
    std::size_t Count() const noexcept;
    std::size_t ZoneCount() const noexcept;
    std::size_t ResultCount() const noexcept;

private:
    std::map<std::string, std::vector<ZoneSpawner>, std::less<>> _byZone;
    std::map<std::string, std::vector<ZoneSpawnResult>, std::less<>> _results;
};

struct SpawnerContext
{
    Map::Clock::time_point Now;
    std::chrono::milliseconds ReleaseDelay{ 0 };
    float RespawnRate = 1.0f;
    MapObjectSources Sources;
    std::function<uint32(uint32)> Roll;
    PathLookup Paths;
    uint64 PathGeneration = 0;
};

enum class SpawnStartNode : int32
{
    Random = 0,
    RandomUnique = 1,
    First = 2,
    Last = 3,
    Specific = 4
};

class SpawnerMgr
{
public:
    static constexpr std::string_view ReloadTarget = "zone_spawner";
    static constexpr uint32 MaxSpawnsPerSpawner = 1000;
    static constexpr uint32 MaxRespawnSeconds = 7 * 24 * 60 * 60;
    static constexpr uint32 DefaultDespawnEffect = 0;
    static constexpr std::string_view SpawnResultClass = "class ResSpawn";
    static constexpr std::string_view DespawnResultClass = "class ResDespawn";

    using RateReader = std::function<float()>;

    static SpawnerMgr& Instance();

    SpawnerMgr(SpawnerMgr const&) = delete;
    SpawnerMgr& operator=(SpawnerMgr const&) = delete;

    bool Load(std::vector<std::string>& errors);
    void RegisterReloadTargets();
    void Replace(ZoneSpawners spawners);
    void SetRateReader(RateReader reader);
    void Clear();

    std::shared_ptr<ZoneSpawners const> Get() const { return _spawners.Get(); }
    uint64 GetGeneration() const noexcept { return _spawners.GetGeneration(); }
    float GetRespawnRate() const;

    static std::optional<ZoneSpawners> Build(std::vector<std::pair<std::string, ZoneSpawner>> spawners, std::vector<std::pair<std::string, std::pair<uint32, ZoneSpawnEntry>>> entries,
        std::set<std::string, std::less<>> const& zones, std::vector<std::string>& errors);
    static MapObjectChanges Update(Map& map, std::vector<ZoneSpawner> const& spawners, uint64 generation, SpawnerContext const& context);
    static bool Despawn(Map& map, uint64 globalId, std::optional<uint32> effect, uint64 killer, SpawnerContext const& context, MapObjectChanges& changes);
    static std::optional<uint64> SpawnTemporary(Map& map, uint64 templateId, PropertyTypes::Vector3D const& position, float yaw, SpawnerContext const& context,
        MapObjectChanges& changes);
    static MapObject const* FindNearest(Map const& map, PropertyTypes::Vector3D const& position, float range);
    static std::optional<ZoneSpawnResult> ReadResult(TypeCatalogPtr const& catalog, std::span<uint8 const> data, std::string& error);
    static void RunResults(Map& map, std::vector<ZoneSpawner> const& spawners, std::vector<ZoneSpawnResult> const& results, std::string_view trigger, uint64 wizard,
        SpawnerContext const& context, MapObjectChanges& changes);
    static std::chrono::milliseconds RespawnDelay(ZoneSpawner const& spawner, float rate);
    static std::size_t PickNode(ZoneSpawnEntry const& entry, ZonePath const& path, std::set<uint64> const& taken, std::function<uint32(uint32)> const& roll);

    SpawnerContext WorldContext(Map::Clock::time_point now, std::chrono::milliseconds releaseDelay) const;
    static void UseWorldPaths(SpawnerContext& context, Map const& map);
    MapObjectChanges UpdateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay);
    static MapObjectChanges PopulateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay);
    MapObjectChanges TriggerFromWorld(Map& map, std::vector<std::string> const& fired, uint64 wizard, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay);

private:
    SpawnerMgr() = default;

    ReloadableStore<ZoneSpawners> _spawners;
    RateReader _rate;
};

#define sSpawnerMgr SpawnerMgr::Instance()

#endif
