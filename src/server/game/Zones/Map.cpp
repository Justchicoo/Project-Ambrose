/*
 * Project Ambrose by Imjustchico
 * A wizard joining an instance takes a mobile id from the player range and cancels any take-down that was waiting, and one leaving gives the id back to cool; a wizard already in the instance keeps the id it has rather than being given a second. An instance with nobody in it is due for take-down once the moment it recorded has passed, never before, and an instance that has never had anybody in it is not due at all, because it was made for somebody who is about to arrive.
 */

#include "Map.h"

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
