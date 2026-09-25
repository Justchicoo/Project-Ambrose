/*
 * Project Ambrose by Imjustchico
 * Every running zone instance (sMapMgr): the public instance of a zone is found or made by its path, a private one is made on request, and each is given a dynamic zone id no other running instance holds. An instance whose last wizard left is taken down on the first tick after its delay has passed. The delays are read from the settings each time they are needed, the unload delay when an instance empties and the mobile id delay when an id is given back, so changing either applies to the next one with nothing restarted. The clock and the settings are handed in, so a test can move time and change a setting between two steps without a server running. Only the world thread touches it.
 */

#ifndef AMBROSE_MAPMGR_H
#define AMBROSE_MAPMGR_H

#include "Map.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct MapSettings
{
    static constexpr std::chrono::seconds DefaultUnloadDelay{ 60 };
    static constexpr std::chrono::milliseconds DefaultMobileIdReleaseDelay{ 2000 };

    std::chrono::milliseconds UnloadDelay = DefaultUnloadDelay;
    std::chrono::milliseconds MobileIdReleaseDelay = DefaultMobileIdReleaseDelay;
};

class MapMgr
{
public:
    using Clock = Map::Clock;
    using SettingsReader = std::function<MapSettings()>;
    using ClockReader = std::function<Clock::time_point()>;

    static MapMgr& Instance();

    MapMgr(MapMgr const&) = delete;
    MapMgr& operator=(MapMgr const&) = delete;

    void SetSettingsReader(SettingsReader reader);
    void SetClock(ClockReader clock);

    Map& FindOrCreatePublic(std::string_view zonePath);
    Map& CreatePrivate(std::string_view zonePath);
    Map* Find(uint32 dynamicZoneId);
    Map* FindPublic(std::string_view zonePath);

    std::optional<uint16> AddPlayer(Map& map, uint64 characterGuid);
    bool RemovePlayer(Map& map, uint64 characterGuid);

    std::vector<uint32> Update();
    std::size_t GetMapCount() const noexcept;
    void Clear();

private:
    MapMgr();

    Map& Make(std::string_view zonePath, bool isPublic);
    Clock::time_point Now() const;
    MapSettings Settings() const;

    std::map<uint32, std::unique_ptr<Map>> _maps;
    std::map<std::string, uint32, std::less<>> _public;
    uint32 _nextDynamicZoneId = 1;
    SettingsReader _settings;
    ClockReader _clock;
};

#define sMapMgr MapMgr::Instance()

#endif
