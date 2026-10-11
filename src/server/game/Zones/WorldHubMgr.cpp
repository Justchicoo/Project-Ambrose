/*
 * Project Ambrose by Imjustchico
 * Reads server and client world names with hub and universe arrival rows as a complete candidate set, verifies names and referenced places against the loaded zones, then swaps only a wholly valid set into service.
 */

#include "WorldHubMgr.h"

#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "ZoneMgr.h"

#include <fmt/format.h>

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

WorldHubMgr& WorldHubMgr::Instance()
{
    static WorldHubMgr instance;
    return instance;
}

bool WorldHubMgr::Load(std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so no world_hub rows were read");
        return false;
    }
    if (!sZoneMgr.GetTemplates())
    {
        errors.push_back("the zones are not loaded, so no world hub can be checked");
        return false;
    }

    QueryResult rows;
    if (!WorldDatabase.TryQuery("SELECT `world`, `client_world`, `hub_zone`, `hub_location`, `universe_zone`, `universe_location` FROM `world_hub` ORDER BY `route_id`", rows))
    {
        errors.push_back("world_hub could not be read");
        return false;
    }

    Table table;
    std::size_t const before = errors.size();
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            WorldHub hub{ row[0].Get<std::string>(), row[1].Get<std::string>(), row[2].Get<std::string>(), row[3].Get<std::string>(), row[4].Get<std::string>(),
                row[5].Get<std::string>() };
            table.push_back(std::move(hub));
        } while (rows->NextRow());
    }
    if (!Validate(table, errors) || errors.size() != before)
        return false;

    LOG_INFO("server.world", "Loaded {} world-hub route(s) from world_hub", table.size());
    Replace(std::move(table));
    return true;
}

bool WorldHubMgr::Validate(Table const& table, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    std::shared_ptr<ZoneTemplates const> const templates = sZoneMgr.GetTemplates();
    if (!templates)
    {
        errors.push_back("the zones are not loaded, so no world hub can be checked");
        return false;
    }
    std::set<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>> routes;
    std::map<std::string, std::string, std::less<>> clientWorlds;
    std::map<std::string, std::string, std::less<>> serverWorlds;
    for (WorldHub const& hub : table)
    {
        if (!routes.emplace(hub.World, hub.ClientWorld, hub.HubZone, hub.HubLocation, hub.UniverseZone, hub.UniverseLocation).second)
            errors.push_back(fmt::format("world_hub repeats the route for world '{}' through '{}'", hub.World, hub.HubZone));
        if (hub.World.empty())
            errors.push_back("world_hub has an empty world name");
        if (hub.ClientWorld.empty())
            errors.push_back(fmt::format("world '{}' has an empty client world name", hub.World));
        auto const [clientWorld, newClientWorld] = clientWorlds.emplace(hub.World, hub.ClientWorld);
        if (!newClientWorld && clientWorld->second != hub.ClientWorld)
            errors.push_back(fmt::format("world '{}' has conflicting client world names '{}' and '{}'", hub.World, clientWorld->second, hub.ClientWorld));
        auto const [serverWorld, newServerWorld] = serverWorlds.emplace(hub.ClientWorld, hub.World);
        if (!newServerWorld && serverWorld->second != hub.World)
            errors.push_back(fmt::format("client world '{}' maps to both '{}' and '{}'", hub.ClientWorld, serverWorld->second, hub.World));
        if (hub.HubZone.empty() || !templates->Has(hub.HubZone))
            errors.push_back(fmt::format("world '{}' names hub zone '{}', which the zones do not hold", hub.World, hub.HubZone));
        else if (hub.HubLocation.empty() || sZoneMgr.FindPlace(hub.HubZone, hub.HubLocation).Result != ZoneLookup::Ok)
            errors.push_back(fmt::format("world '{}' names hub location '{}' in '{}', which the zones do not hold", hub.World, hub.HubLocation, hub.HubZone));
        if (hub.UniverseZone.empty() || !templates->Has(hub.UniverseZone))
            errors.push_back(fmt::format("world '{}' names universe zone '{}', which the zones do not hold", hub.World, hub.UniverseZone));
        else if (hub.UniverseLocation.empty() || sZoneMgr.FindPlace(hub.UniverseZone, hub.UniverseLocation).Result != ZoneLookup::Ok)
            errors.push_back(fmt::format("world '{}' names universe location '{}' in '{}', which the zones do not hold", hub.World, hub.UniverseLocation, hub.UniverseZone));
    }
    return errors.size() == before;
}

void WorldHubMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(ReloadTarget), [this](std::vector<std::string>& errors) { return Load(errors); },
        { std::string(ZoneMgr::TemplateTarget), std::string(ZoneMgr::LocationTarget) });
}

void WorldHubMgr::Replace(Table table)
{
    auto shared = std::make_shared<Table const>(std::move(table));
    std::lock_guard const lock(_mutex);
    _table = std::move(shared);
}

void WorldHubMgr::Clear()
{
    Replace({});
}

std::optional<WorldHub> WorldHubMgr::Find(std::string_view world) const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    auto const found = std::find_if(table->begin(), table->end(), [world](WorldHub const& hub) { return hub.World == world; });
    return found == table->end() ? std::nullopt : std::optional<WorldHub>(*found);
}

std::optional<WorldHub> WorldHubMgr::FindForClientWorld(std::string_view clientWorld) const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    return FindForClientWorld(*table, clientWorld);
}

std::optional<WorldHub> WorldHubMgr::FindForClientWorld(Table const& table, std::string_view clientWorld)
{
    auto const found = std::find_if(table.begin(), table.end(), [clientWorld](WorldHub const& hub) { return hub.ClientWorld == clientWorld; });
    return found == table.end() ? std::nullopt : std::optional<WorldHub>(*found);
}

std::optional<WorldHub> WorldHubMgr::FindForUniverseZone(std::string_view zone) const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    return FindForUniverseZone(*table, zone);
}

std::optional<WorldHub> WorldHubMgr::FindForUniverseZone(Table const& table, std::string_view zone)
{
    auto const found = std::find_if(table.begin(), table.end(), [zone](WorldHub const& hub)
    {
        return zone == hub.UniverseZone || (zone.size() > hub.UniverseZone.size() && zone.starts_with(hub.UniverseZone) && zone[hub.UniverseZone.size()] == '/');
    });
    return found == table.end() ? std::nullopt : std::optional<WorldHub>(*found);
}

std::vector<WorldHub> WorldHubMgr::FindUnlocked(Table const& table, std::vector<std::string> const& unlockedWorlds)
{
    std::set<std::string, std::less<>> included;
    std::vector<WorldHub> unlocked;
    for (WorldHub const& hub : table)
        if (std::find(unlockedWorlds.begin(), unlockedWorlds.end(), hub.World) != unlockedWorlds.end() && included.insert(hub.World).second)
            unlocked.push_back(hub);
    return unlocked;
}

std::optional<WorldHub> WorldHubMgr::FindForZone(std::string_view zone) const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    return FindForZone(*table, zone);
}

std::optional<WorldHub> WorldHubMgr::FindForZone(Table const& table, std::string_view zone)
{
    auto matchesZoneRoot = [zone](std::string_view routeZone)
    {
        std::size_t const separator = routeZone.find('/');
        std::string_view const root = separator == std::string_view::npos ? routeZone : routeZone.substr(0, separator);
        return zone == root || (zone.size() > root.size() && zone.starts_with(root) && zone[root.size()] == '/');
    };
    auto const found = std::find_if(table.begin(), table.end(), [&matchesZoneRoot](WorldHub const& hub)
    {
        return matchesZoneRoot(hub.HubZone) || matchesZoneRoot(hub.UniverseZone);
    });
    return found == table.end() ? std::nullopt : std::optional<WorldHub>(*found);
}

std::vector<WorldHub> WorldHubMgr::FindAll(std::string_view world) const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    std::vector<WorldHub> hubs;
    for (WorldHub const& hub : *table)
        if (hub.World == world)
            hubs.push_back(hub);
    return hubs;
}

std::vector<WorldHub> WorldHubMgr::All() const
{
    std::shared_ptr<Table const> table;
    {
        std::lock_guard const lock(_mutex);
        table = _table;
    }
    std::vector<WorldHub> hubs;
    hubs.reserve(table->size());
    for (WorldHub const& hub : *table)
        hubs.push_back(hub);
    return hubs;
}
