/*
 * Project Ambrose by Imjustchico
 * Every zone's spawners from zone_spawner and zone_spawner_entry (sSpawnerMgr), read at start and again by `.reload zone_spawner`, which builds the new set off to the side, validates it and swaps it in only when every row is good, keeping the old set and reporting each error otherwise; and what each spawner does in each running zone instance: it keeps as many of its objects alive as its count allows, choosing each by its entries' chances, brings one back its respawn time after one is taken away, that time scaled by Rate.Respawn as it stands at the moment of the despawn, and never holds more than its count. A game master's own spawns are placed and taken away here too, and so is any object a despawn effect takes away.
 */

#ifndef AMBROSE_SPAWNERMGR_H
#define AMBROSE_SPAWNERMGR_H

#include "Map.h"
#include "MapObjectSpawner.h"
#include "ReloadableStore.h"
#include "Types.h"
#include "ZoneMgr.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
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
    uint64 PathId = 0;

    bool operator==(ZoneSpawnEntry const&) const = default;
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
    bool Spawns() const noexcept;
    uint32 TotalChance() const noexcept;
};

class ZoneSpawners
{
public:
    ZoneSpawners() = default;
    explicit ZoneSpawners(std::map<std::string, std::vector<ZoneSpawner>, std::less<>> byZone);

    std::vector<ZoneSpawner> const* In(std::string_view zone) const;
    std::size_t Count() const noexcept;
    std::size_t ZoneCount() const noexcept;

private:
    std::map<std::string, std::vector<ZoneSpawner>, std::less<>> _byZone;
};

struct SpawnerContext
{
    Map::Clock::time_point Now;
    std::chrono::milliseconds ReleaseDelay{ 0 };
    float RespawnRate = 1.0f;
    MapObjectSources Sources;
    std::function<uint32(uint32)> Roll;
};

class SpawnerMgr
{
public:
    static constexpr std::string_view ReloadTarget = "zone_spawner";
    static constexpr uint32 MaxSpawnsPerSpawner = 1000;
    static constexpr uint32 MaxRespawnSeconds = 7 * 24 * 60 * 60;
    static constexpr uint32 DefaultDespawnEffect = 0;

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
    static std::chrono::milliseconds RespawnDelay(ZoneSpawner const& spawner, float rate);

    SpawnerContext WorldContext(Map::Clock::time_point now, std::chrono::milliseconds releaseDelay) const;
    MapObjectChanges UpdateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay);
    static MapObjectChanges PopulateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay);

private:
    SpawnerMgr() = default;

    ReloadableStore<ZoneSpawners> _spawners;
    RateReader _rate;
};

#define sSpawnerMgr SpawnerMgr::Instance()

#endif
