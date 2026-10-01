/*
 * Project Ambrose by Imjustchico
 * Reads the volumes, triggers and their events zone by zone, refusing a volume whose shape is unknown or whose size or place is not a usable number, a trigger whose cooldown is not, and an event that names a trigger the zone does not hold; swaps a whole good set in at once, dropping each instance's counts and cooldowns with it, and keeps each zone instance's trigger state until its map is forgotten.
 */

#include "ZoneTriggerMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <utility>

ZoneTriggerMgr& ZoneTriggerMgr::Instance()
{
    static ZoneTriggerMgr instance;
    return instance;
}

bool ZoneTriggerMgr::Load(std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so no zone volumes or triggers were read");
        return false;
    }
    std::map<std::string, ZoneTriggerData, std::less<>> zones;
    std::size_t const before = errors.size();
    QueryResult rows;
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `volume_index`, `name`, `shape`, `position_x`, `position_y`, `position_z`, `radius`, `length`, `width`, `depth` FROM `zone_volume`", rows))
    {
        errors.push_back("zone_volume could not be read");
        return false;
    }
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            ZoneVolume volume{ row[1].Get<uint32>(), row[2].Get<std::string>(), ZoneVolume::ShapeOf(row[3].Get<std::string>()), row[4].Get<float>(), row[5].Get<float>(),
                row[6].Get<float>(), row[7].Get<float>(), row[8].Get<float>(), row[9].Get<float>(), row[10].Get<float>() };
            std::string const zone = row[0].Get<std::string>();
            bool const sized = volume.Shape == VolumeShape::Box ? volume.Width > 0.0f && volume.Depth > 0.0f && volume.Length > 0.0f
                : volume.Shape == VolumeShape::Cylinder ? volume.Radius > 0.0f && volume.Length > 0.0f : volume.Radius > 0.0f;
            if (volume.Shape == VolumeShape::Unknown)
                errors.push_back(fmt::format("{} volume {} ({}) has the shape {}, which Ambrose does not know", zone, volume.Index, volume.Name, row[3].Get<std::string>()));
            else if (!sized || !std::isfinite(volume.X) || !std::isfinite(volume.Y) || !std::isfinite(volume.Z))
                errors.push_back(fmt::format("{} volume {} ({}) has no usable size or place", zone, volume.Index, volume.Name));
            else
                zones[zone].Volumes.push_back(std::move(volume));
        } while (rows->NextRow());
    }
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `trigger_index`, `name`, `trigger_max`, `cooldown`, `requirements` IS NOT NULL AND LENGTH(`requirements`) > 0 FROM `zone_trigger`", rows))
    {
        errors.push_back("zone_trigger could not be read");
        return false;
    }
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            ZoneTrigger trigger{ row[1].Get<uint32>(), row[2].Get<std::string>(), row[3].Get<int32>(), row[4].Get<float>(), row[5].Get<int64>() != 0, {} };
            if (!std::isfinite(trigger.CooldownSeconds) || trigger.CooldownSeconds < 0.0f)
                errors.push_back(fmt::format("{} trigger {} ({}) has a cooldown that is not a number of seconds", row[0].Get<std::string>(), trigger.Index, trigger.Name));
            else
                zones[row[0].Get<std::string>()].Triggers.push_back(std::move(trigger));
        } while (rows->NextRow());
    }
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `owner`, `owner_index`, `kind`, `event_name` FROM `zone_trigger_event` ORDER BY `zone_path`, `owner`, `owner_index`, `kind`, `position`", rows))
    {
        errors.push_back("zone_trigger_event could not be read");
        return false;
    }
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            std::string const zone = row[0].Get<std::string>();
            std::string const owner = row[1].Get<std::string>();
            uint32 const index = row[2].Get<uint32>();
            std::string const kind = row[3].Get<std::string>();
            std::string event = row[4].Get<std::string>();
            ZoneTriggerData& data = zones[zone];
            if (owner == "volume")
            {
                bool const known = std::any_of(data.Volumes.begin(), data.Volumes.end(), [index](ZoneVolume const& volume) { return volume.Index == index; });
                if (!known)
                    continue;
                if (kind == "enter")
                    data.EnterEvents[index].push_back(std::move(event));
                else if (kind == "exit")
                    data.ExitEvents[index].push_back(std::move(event));
                continue;
            }
            auto const trigger = std::find_if(data.Triggers.begin(), data.Triggers.end(), [index](ZoneTrigger const& candidate) { return candidate.Index == index; });
            if (trigger == data.Triggers.end())
                errors.push_back(fmt::format("{} has a {} event for trigger {}, which the zone does not hold", zone, kind, index));
            else if (kind == "fire")
                trigger->FireEvents.push_back(std::move(event));
        } while (rows->NextRow());
    }
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `event_name` FROM `zone_client_event`", rows))
    {
        errors.push_back("zone_client_event could not be read");
        return false;
    }
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            std::string event = row[1].Get<std::string>();
            if (event.empty() || event == EnterZoneEvent)
                errors.push_back(fmt::format("{} lets clients post the event '{}', which only the server may post", row[0].Get<std::string>(), event));
            else
                zones[row[0].Get<std::string>()].ClientEvents.insert(std::move(event));
        } while (rows->NextRow());
    }
    for (auto const& [zone, data] : zones)
        for (auto const* events : { &data.EnterEvents, &data.ExitEvents })
            for (auto const& [index, names] : *events)
                for (std::string const& name : names)
                    if (data.ClientEvents.contains(name))
                        errors.push_back(fmt::format("{} lets clients post '{}', which volume {} posts as a wizard walks, so a client could fake the walk", zone, name, index));
    if (errors.size() != before)
        return false;
    std::size_t volumes = 0;
    std::size_t triggers = 0;
    for (auto const& [zone, data] : zones)
    {
        volumes += data.Volumes.size();
        triggers += data.Triggers.size();
    }
    std::size_t const zoneCount = zones.size();
    Replace(std::move(zones));
    LOG_INFO("server.world", "Loaded {} zone volume(s) and {} trigger(s) in {} zone(s)", volumes, triggers, zoneCount);
    return true;
}

void ZoneTriggerMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(ReloadTarget), [this](std::vector<std::string>& errors) { return Load(errors); });
}

void ZoneTriggerMgr::Replace(std::map<std::string, ZoneTriggerData, std::less<>> zones)
{
    std::map<std::string, std::shared_ptr<ZoneTriggerData const>, std::less<>> shared;
    for (auto& [zone, data] : zones)
        shared.emplace(zone, std::make_shared<ZoneTriggerData const>(std::move(data)));
    std::lock_guard const lock(_mutex);
    _zones = std::move(shared);
    _instances.clear();
}

void ZoneTriggerMgr::Clear()
{
    std::lock_guard const lock(_mutex);
    _zones.clear();
    _instances.clear();
}

std::shared_ptr<ZoneTriggerData const> ZoneTriggerMgr::Find(std::string_view zone) const
{
    std::lock_guard const lock(_mutex);
    auto const found = _zones.find(zone);
    return found == _zones.end() ? nullptr : found->second;
}

std::vector<std::string> ZoneTriggerMgr::Post(uint32 mapId, std::string_view zone, std::string_view event, uint64 wizard, ZoneTriggers::Clock::time_point now)
{
    std::lock_guard const lock(_mutex);
    auto const data = _zones.find(zone);
    if (data == _zones.end())
        return {};
    auto instance = _instances.find(mapId);
    if (instance == _instances.end())
        instance = _instances.emplace(mapId, ZoneTriggers(data->second->Triggers)).first;
    std::vector<std::string> names;
    for (ZoneTrigger const* trigger : instance->second.Post(event, wizard, now))
        names.push_back(trigger->Name);
    return names;
}

void ZoneTriggerMgr::ForgetMap(uint32 mapId)
{
    std::lock_guard const lock(_mutex);
    _instances.erase(mapId);
}
