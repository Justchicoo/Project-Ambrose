/*
 * Project Ambrose by Imjustchico
 * Reads zone_teleport whole, checks every row against the zones and their named locations as they stand, an empty destination location meaning Start, and keeps the old table when any row is refused.
 */

#include "ZoneTeleportMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "ZoneMgr.h"

#include <fmt/format.h>

#include <utility>

ZoneTeleportMgr& ZoneTeleportMgr::Instance()
{
    static ZoneTeleportMgr instance;
    return instance;
}

std::optional<ZoneTeleport> ZoneTeleportMgr::FirstWithDestination(Table const& table, std::string_view zone, std::vector<std::string> const& doors)
{
    for (std::string const& door : doors)
    {
        auto const found = table.find(std::make_pair(std::string(zone), door));
        if (found != table.end())
            return found->second;
    }
    return std::nullopt;
}

bool ZoneTeleportMgr::Load(std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so no zone_teleport rows were read");
        return false;
    }
    std::shared_ptr<ZoneTemplates const> const templates = sZoneMgr.GetTemplates();
    if (!templates)
    {
        errors.push_back("the zones are not loaded, so no door destination can be checked");
        return false;
    }
    QueryResult rows;
    if (!WorldDatabase.TryQuery("SELECT `zone`, `trigger_name`, `dest_zone`, `dest_location`, `transition_id`, `same_zone` FROM `zone_teleport`", rows))
    {
        errors.push_back("zone_teleport could not be read");
        return false;
    }
    std::size_t const before = errors.size();
    Table table;
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            ZoneTeleport door{ row[0].Get<std::string>(), row[1].Get<std::string>(), row[2].Get<std::string>(), row[3].Get<std::string>(), row[4].Get<uint32>(),
                row[5].Get<uint8>() != 0 };
            if (door.DestLocation.empty())
                door.DestLocation = std::string(ZoneLocations::StartName);
            std::string const name = fmt::format("{} trigger '{}'", door.Zone, door.TriggerName);
            if (!templates->Has(door.Zone))
                errors.push_back(fmt::format("{} names a zone the zones do not hold", name));
            else if (!templates->Has(door.DestZone))
                errors.push_back(fmt::format("{} leads to {}, which the zones do not hold", name, door.DestZone));
            else if (sZoneMgr.FindPlace(door.DestZone, door.DestLocation).Result != ZoneLookup::Ok)
                errors.push_back(fmt::format("{} leads to {} in {}, which has no location of that name", name, door.DestLocation, door.DestZone));
            else if (door.SameZone != (door.Zone == door.DestZone))
                errors.push_back(fmt::format("{} says same_zone {} but leads {}", name, door.SameZone ? 1 : 0, door.SameZone ? "to another zone" : "within its own zone"));
            else
                table.emplace(std::make_pair(door.Zone, door.TriggerName), std::move(door));
        } while (rows->NextRow());
    }
    if (errors.size() != before)
        return false;
    LOG_INFO("server.world", "Loaded {} door destination(s) from zone_teleport", table.size());
    Replace(std::move(table));
    return true;
}

void ZoneTeleportMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(ReloadTarget), [this](std::vector<std::string>& errors) { return Load(errors); },
        { std::string(ZoneMgr::TemplateTarget), std::string(ZoneMgr::LocationTarget) });
}

void ZoneTeleportMgr::Replace(Table table)
{
    auto shared = std::make_shared<Table const>(std::move(table));
    std::lock_guard const lock(_mutex);
    _table = std::move(shared);
}

void ZoneTeleportMgr::Clear()
{
    Replace({});
}

std::optional<ZoneTeleport> ZoneTeleportMgr::Find(std::string_view zone, std::string_view trigger) const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    auto const found = table->find(std::make_pair(std::string(zone), std::string(trigger)));
    return found == table->end() ? std::nullopt : std::optional<ZoneTeleport>(found->second);
}

std::optional<ZoneTeleport> ZoneTeleportMgr::FirstWithDestination(std::string_view zone, std::vector<std::string> const& doors) const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    return FirstWithDestination(*table, zone, doors);
}

std::vector<ZoneTeleport> ZoneTeleportMgr::InZone(std::string_view zone) const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    std::vector<ZoneTeleport> doors;
    for (auto const& [key, door] : *table)
        if (key.first == zone)
            doors.push_back(door);
    return doors;
}
