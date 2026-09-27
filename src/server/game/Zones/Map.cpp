/*
 * Project Ambrose by Imjustchico
 * A wizard joining an instance takes a mobile id from the player range and cancels any take-down that was waiting, and one leaving gives the id back to cool; a wizard already in the instance keeps the id it has rather than being given a second. An object is found by its global id or by the zone row it was spawned from, and one removed gives its mobile id back to cool the way a wizard's does. An instance with nobody in it is due for take-down once the moment it recorded has passed, never before, and an instance that has never had anybody in it is not due at all, because it was made for somebody who is about to arrive.
 */

#include "Map.h"

#include <algorithm>
#include <utility>

Map::Map(uint32 dynamicZoneId, std::string zonePath, bool isPublic)
    : _dynamicZoneId(dynamicZoneId), _zonePath(std::move(zonePath)), _public(isPublic)
{
}

std::optional<uint16> Map::AddPlayer(uint64 characterGuid, Clock::time_point now)
{
    if (auto const found = _players.find(characterGuid); found != _players.end())
    {
        _unloadAt.reset();
        return found->second;
    }
    std::optional<uint16> const id = _mobileIds.Allocate(MobileIdAllocator::Range::Player, now);
    if (!id)
        return std::nullopt;
    _players.emplace(characterGuid, *id);
    _unloadAt.reset();
    return id;
}

bool Map::RemovePlayer(uint64 characterGuid, Clock::time_point now, std::chrono::milliseconds releaseDelay, std::chrono::milliseconds unloadDelay)
{
    auto const found = _players.find(characterGuid);
    if (found == _players.end())
        return false;
    _mobileIds.Release(found->second, now, releaseDelay);
    _players.erase(found);
    _playerObjects.erase(characterGuid);
    if (_players.empty())
        _unloadAt = now + unloadDelay;
    return true;
}

std::optional<uint16> Map::GetMobileId(uint64 characterGuid) const
{
    auto const found = _players.find(characterGuid);
    if (found == _players.end())
        return std::nullopt;
    return found->second;
}

bool Map::HasPlayer(uint64 characterGuid) const
{
    return _players.contains(characterGuid);
}

std::size_t Map::GetPlayerCount() const noexcept
{
    return _players.size();
}

bool Map::IsEmpty() const noexcept
{
    return _players.empty();
}

bool Map::IsDue(Clock::time_point now) const noexcept
{
    return _players.empty() && _unloadAt && *_unloadAt <= now;
}

MapObject const* Map::FindObject(uint64 globalId) const
{
    auto const found = std::find_if(_objects.begin(), _objects.end(), [globalId](MapObject const& object) { return object.GlobalId == globalId; });
    return found == _objects.end() ? nullptr : &*found;
}

MapObject const* Map::FindSpawn(uint64 spawnId) const
{
    auto const found = std::find_if(_objects.begin(), _objects.end(), [spawnId](MapObject const& object) { return object.Spawn.Id == spawnId; });
    return found == _objects.end() ? nullptr : &*found;
}

std::vector<uint64> Map::GetPlayers() const
{
    std::vector<uint64> players;
    players.reserve(_players.size());
    for (auto const& [guid, mobileId] : _players)
        players.push_back(guid);
    return players;
}

MapPlayerObject const* Map::FindPlayerObject(uint64 characterGuid) const
{
    auto const found = _playerObjects.find(characterGuid);
    return found == _playerObjects.end() ? nullptr : &found->second;
}

bool Map::SetPlayerObject(uint64 characterGuid, uint64 globalId, std::vector<uint8> data)
{
    auto const found = _players.find(characterGuid);
    if (found == _players.end())
        return false;
    _playerObjects.insert_or_assign(characterGuid, MapPlayerObject{ globalId, found->second, std::move(data) });
    return true;
}

void Map::AddObject(MapObject object)
{
    _objects.push_back(std::move(object));
}

std::optional<MapObject> Map::RemoveSpawn(uint64 spawnId, Clock::time_point now, std::chrono::milliseconds releaseDelay)
{
    auto const found = std::find_if(_objects.begin(), _objects.end(), [spawnId](MapObject const& object) { return object.Spawn.Id == spawnId; });
    if (found == _objects.end())
        return std::nullopt;
    MapObject removed = std::move(*found);
    _objects.erase(found);
    if (removed.MobileId != 0)
        _mobileIds.Release(removed.MobileId, now, releaseDelay);
    return removed;
}
