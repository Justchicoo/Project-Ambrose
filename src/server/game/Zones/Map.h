/*
 * Project Ambrose by Imjustchico
 * One running instance of a zone: the dynamic zone id the client is told, the zone it is an instance of, the wizards standing in it, the objects its zone places that the server sends, each with its ids and the Data MSG_NEWOBJECT carries encoded once for every wizard who comes, the generations of the zone rows, classes, tables and templates they were built from, the mobile ids it has handed out, and the objects its zone's spawners and game masters placed while it runs, each marked with where it came from, with what each spawner holds alive and the times its next objects return. It lives on the world thread and is touched nowhere else, so it holds no lock. When its last wizard leaves it remembers when it may be taken down, with the delay read at that moment, so a changed Zone.UnloadDelay applies to the next instance that empties and a wizard who comes back before then finds the same instance still there.
 */

#ifndef AMBROSE_MAP_H
#define AMBROSE_MAP_H

#include "MobileIdAllocator.h"
#include "Types.h"
#include "ZoneMgr.h"

#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct MapObjectStamp
{
    uint64 Rows = 0;
    uint64 Catalog = 0;
    uint64 Types = 0;
    uint64 Behaviors = 0;
    uint64 Templates = 0;

    bool operator==(MapObjectStamp const&) const = default;
    bool BuildsAlike(MapObjectStamp const& other) const noexcept
    {
        return Catalog == other.Catalog && Types == other.Types && Behaviors == other.Behaviors && Templates == other.Templates;
    }
};

enum class MapObjectOrigin : uint8
{
    Zone,
    Spawner,
    Command
};

struct MapSpawnerLive
{
    uint32 RespawnSeconds = 0;
    std::vector<uint64> Alive;
    std::vector<std::chrono::steady_clock::time_point> Respawns;
};

struct MapSpawnerState
{
    static constexpr uint64 FirstSpawnId = uint64{ 1 } << 62;

    std::optional<uint64> Generation;
    std::map<uint32, MapSpawnerLive> Spawners;
    uint64 NextSpawnId = FirstSpawnId;
};

struct MapObject
{
    ZoneObjectSpawn Spawn;
    MapObjectOrigin Origin = MapObjectOrigin::Zone;
    uint32 SpawnerIndex = 0;
    uint64 GlobalId = 0;
    uint64 PermId = 0;
    uint16 MobileId = 0;
    bool Critical = false;
    bool ExemptFromAoi = false;
    std::vector<uint8> Data;
};

class Map
{
public:
    using Clock = std::chrono::steady_clock;

    Map(uint32 dynamicZoneId, std::string zonePath, bool isPublic);

    Map(Map const&) = delete;
    Map& operator=(Map const&) = delete;

    uint32 GetDynamicZoneId() const noexcept { return _dynamicZoneId; }
    std::string const& GetZonePath() const noexcept { return _zonePath; }
    bool IsPublic() const noexcept { return _public; }

    std::optional<uint16> AddPlayer(uint64 characterGuid, Clock::time_point now);
    bool RemovePlayer(uint64 characterGuid, Clock::time_point now, std::chrono::milliseconds releaseDelay, std::chrono::milliseconds unloadDelay);
    std::optional<uint16> GetMobileId(uint64 characterGuid) const;
    bool HasPlayer(uint64 characterGuid) const;
    std::size_t GetPlayerCount() const noexcept;
    bool IsEmpty() const noexcept;

    std::optional<Clock::time_point> GetUnloadAt() const noexcept { return _unloadAt; }
    bool IsDue(Clock::time_point now) const noexcept;

    MobileIdAllocator& GetMobileIds() noexcept { return _mobileIds; }
    MobileIdAllocator const& GetMobileIds() const noexcept { return _mobileIds; }

    std::vector<MapObject> const& GetObjects() const noexcept { return _objects; }
    MapObject const* FindObject(uint64 globalId) const;
    MapObject const* FindSpawn(uint64 spawnId) const;
    std::vector<uint64> GetPlayers() const;
    void AddObject(MapObject object);
    std::optional<MapObject> RemoveSpawn(uint64 spawnId, Clock::time_point now, std::chrono::milliseconds releaseDelay);
    std::optional<MapObject> RemoveObject(uint64 globalId, Clock::time_point now, std::chrono::milliseconds releaseDelay);
    MapSpawnerState& GetSpawnerState() noexcept { return _spawners; }
    MapSpawnerState const& GetSpawnerState() const noexcept { return _spawners; }
    std::optional<MapObjectStamp> const& GetObjectStamp() const noexcept { return _objectStamp; }
    void SetObjectStamp(MapObjectStamp const& stamp) noexcept { _objectStamp = stamp; }

private:
    uint32 _dynamicZoneId;
    std::string _zonePath;
    bool _public;
    std::map<uint64, uint16> _players;
    MobileIdAllocator _mobileIds;
    std::vector<MapObject> _objects;
    std::optional<MapObjectStamp> _objectStamp;
    MapSpawnerState _spawners;
    std::optional<Clock::time_point> _unloadAt;
};

#endif
