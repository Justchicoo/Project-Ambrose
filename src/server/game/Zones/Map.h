/*
 * Project Ambrose by Imjustchico
 * One running instance of a zone: the dynamic zone id the client is told, the zone it is an instance of, the wizards standing in it and the mobile ids it has handed out. It lives on the world thread and is touched nowhere else, so it holds no lock. When its last wizard leaves it remembers when it may be taken down, with the delay read at that moment, so a changed Zone.UnloadDelay applies to the next instance that empties and a wizard who comes back before then finds the same instance still there.
 */

#ifndef AMBROSE_MAP_H
#define AMBROSE_MAP_H

#include "MobileIdAllocator.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <map>
#include <optional>
#include <string>

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

private:
    uint32 _dynamicZoneId;
    std::string _zonePath;
    bool _public;
    std::map<uint64, uint16> _players;
    MobileIdAllocator _mobileIds;
    std::optional<Clock::time_point> _unloadAt;
};

#endif
