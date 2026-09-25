/*
 * Project Ambrose by Imjustchico
 * Makes instances with a dynamic zone id taken from a counter that skips any id a running instance still holds and never hands out zero, keeps the public instance of each zone findable by its path, and on each tick takes down every instance whose take-down time has passed, forgetting it as the public instance of its zone so the next wizard to arrive is given a fresh one. With no clock or settings handed in it reads the steady clock and the shipped defaults.
 */

#include "MapMgr.h"

#include <utility>

MapMgr& MapMgr::Instance()
{
    static MapMgr manager;
    return manager;
}

MapMgr::MapMgr() = default;

void MapMgr::SetSettingsReader(SettingsReader reader)
{
    _settings = std::move(reader);
}

void MapMgr::SetClock(ClockReader clock)
{
    _clock = std::move(clock);
}

MapMgr::Clock::time_point MapMgr::Now() const
{
    return _clock ? _clock() : Clock::now();
}

MapSettings MapMgr::Settings() const
{
    return _settings ? _settings() : MapSettings{};
}

Map& MapMgr::Make(std::string_view zonePath, bool isPublic)
{
    while (_nextDynamicZoneId == 0 || _maps.contains(_nextDynamicZoneId))
        ++_nextDynamicZoneId;
    uint32 const id = _nextDynamicZoneId++;
    auto made = std::make_unique<Map>(id, std::string(zonePath), isPublic);
    Map& map = *made;
    _maps.emplace(id, std::move(made));
    return map;
}

Map& MapMgr::FindOrCreatePublic(std::string_view zonePath)
{
    if (Map* const found = FindPublic(zonePath))
        return *found;
    Map& made = Make(zonePath, true);
    _public.emplace(std::string(zonePath), made.GetDynamicZoneId());
    return made;
}

Map& MapMgr::CreatePrivate(std::string_view zonePath)
{
    return Make(zonePath, false);
}

Map* MapMgr::Find(uint32 dynamicZoneId)
{
    auto const found = _maps.find(dynamicZoneId);
    return found == _maps.end() ? nullptr : found->second.get();
}

Map* MapMgr::FindPublic(std::string_view zonePath)
{
    auto const found = _public.find(zonePath);
    return found == _public.end() ? nullptr : Find(found->second);
}

std::optional<uint16> MapMgr::AddPlayer(Map& map, uint64 characterGuid)
{
    return map.AddPlayer(characterGuid, Now());
}

bool MapMgr::RemovePlayer(Map& map, uint64 characterGuid)
{
    MapSettings const settings = Settings();
    return map.RemovePlayer(characterGuid, Now(), settings.MobileIdReleaseDelay, settings.UnloadDelay);
}

std::vector<uint32> MapMgr::Update()
{
    Clock::time_point const now = Now();
    std::vector<uint32> removed;
    for (auto at = _maps.begin(); at != _maps.end();)
    {
        if (!at->second->IsDue(now))
        {
            ++at;
            continue;
        }
        if (at->second->IsPublic())
            if (auto const listed = _public.find(at->second->GetZonePath()); listed != _public.end() && listed->second == at->first)
                _public.erase(listed);
        removed.push_back(at->first);
        at = _maps.erase(at);
    }
    return removed;
}

std::size_t MapMgr::GetMapCount() const noexcept
{
    return _maps.size();
}

void MapMgr::Clear()
{
    _maps.clear();
    _public.clear();
    _nextDynamicZoneId = 1;
    _settings = nullptr;
    _clock = nullptr;
}
