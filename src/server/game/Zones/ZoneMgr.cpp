/*
 * Project Ambrose by Imjustchico
 * Reads the three zone tables into stores that are swapped in whole. A location or an object naming a zone no template knows is a refusal with that row named rather than a row quietly dropped, because a place nothing can load is how a wizard ends up nowhere, and the refusal leaves the rows already serving in place; so is an object whose loading type is none the client has. Only the first few problems of a build are reported, because a table that is wrong is usually wrong in every row and an operator needs the shape of it rather than all of it.
 */

#include "ZoneMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "WorldDatabase.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace
{
    constexpr char const* ZoneLog = "server.zones";

    void Report(std::vector<std::string>& errors, std::string problem)
    {
        if (errors.size() < ZoneMgr::MaxReportedErrors)
            errors.push_back(std::move(problem));
        else if (errors.size() == ZoneMgr::MaxReportedErrors)
            errors.emplace_back("more rows are wrong than are worth listing; fix these and load again");
    }
}

ZoneTemplates::ZoneTemplates(std::map<std::string, ZoneTemplate, std::less<>> byPath) : _byPath(std::move(byPath))
{
}

ZoneTemplate const* ZoneTemplates::Find(std::string_view path) const
{
    auto const found = _byPath.find(path);
    return found == _byPath.end() ? nullptr : &found->second;
}

bool ZoneTemplates::Has(std::string_view path) const
{
    return _byPath.contains(path);
}

std::size_t ZoneTemplates::Count() const noexcept
{
    return _byPath.size();
}

ZoneLocations::ZoneLocations(std::map<std::string, std::vector<ZoneLocation>, std::less<>> byZone) : _byZone(std::move(byZone))
{
}

std::vector<ZoneLocation> const* ZoneLocations::In(std::string_view zone) const
{
    auto const found = _byZone.find(zone);
    return found == _byZone.end() ? nullptr : &found->second;
}

ZonePlace ZoneLocations::Find(std::string_view zone, std::string_view name) const
{
    ZonePlace place;
    std::vector<ZoneLocation> const* const inside = In(zone);
    if (inside == nullptr || inside->empty())
    {
        place.Result = inside == nullptr ? ZoneLookup::UnknownZone : ZoneLookup::NoLocations;
        return place;
    }
    for (ZoneLocation const& location : *inside)
        if (location.Name == name)
        {
            place.Result = ZoneLookup::Ok;
            place.Location = location;
            return place;
        }
    for (ZoneLocation const& location : *inside)
        if (location.Name == StartName)
        {
            place.Result = ZoneLookup::FellBackToStart;
            place.Location = location;
            return place;
        }
    place.Result = ZoneLookup::NoLocations;
    return place;
}

std::size_t ZoneLocations::Count() const noexcept
{
    std::size_t total = 0;
    for (auto const& [zone, locations] : _byZone)
        total += locations.size();
    return total;
}

std::size_t ZoneLocations::ZoneCount() const noexcept
{
    return _byZone.size();
}

ZoneObjects::ZoneObjects(std::map<std::string, std::vector<ZoneObjectSpawn>, std::less<>> byZone) : _byZone(std::move(byZone))
{
}

std::vector<ZoneObjectSpawn> const* ZoneObjects::In(std::string_view zone) const
{
    auto const found = _byZone.find(zone);
    return found == _byZone.end() ? nullptr : &found->second;
}

std::size_t ZoneObjects::Count() const noexcept
{
    std::size_t total = 0;
    for (auto const& [zone, objects] : _byZone)
        total += objects.size();
    return total;
}

std::size_t ZoneObjects::ZoneCount() const noexcept
{
    return _byZone.size();
}

GameTeleports::GameTeleports(std::map<std::string, std::vector<GameTelePoint>, std::less<>> byZone) : _byZone(std::move(byZone))
{
}

GameTelePoint const* GameTeleports::Find(std::string_view zone, std::string_view name) const
{
    auto const found = _byZone.find(zone);
    if (found == _byZone.end())
        return nullptr;
    for (GameTelePoint const& point : found->second)
        if (point.Name == name)
            return &point;
    return nullptr;
}

bool GameTeleports::Add(std::string_view zone, GameTelePoint point)
{
    std::vector<GameTelePoint>& points = _byZone[std::string(zone)];
    if (point.Name.empty() || Find(zone, point.Name))
        return false;
    points.push_back(std::move(point));
    return true;
}

bool GameTeleports::Remove(std::string_view zone, std::string_view name)
{
    auto found = _byZone.find(zone);
    if (found == _byZone.end())
        return false;
    std::vector<GameTelePoint>& points = found->second;
    auto const point = std::find_if(points.begin(), points.end(), [name](GameTelePoint const& candidate) { return candidate.Name == name; });
    if (point == points.end())
        return false;
    points.erase(point);
    if (points.empty())
        _byZone.erase(found);
    return true;
}

std::size_t GameTeleports::Count() const noexcept
{
    std::size_t total = 0;
    for (auto const& [zone, points] : _byZone)
        total += points.size();
    return total;
}

ZoneMgr& ZoneMgr::Instance()
{
    static ZoneMgr manager;
    return manager;
}

ZoneTemplates ZoneMgr::ReadTemplates(PreparedResultSet* result, std::vector<std::string>& errors)
{
    std::map<std::string, ZoneTemplate, std::less<>> byPath;
    if (!result)
        return ZoneTemplates(std::move(byPath));
    do
    {
        Field const* const row = result->Fetch();
        ZoneTemplate zone;
        zone.Path = row[0].Get<std::string>();
        if (zone.Path.empty())
        {
            Report(errors, "a zone_template row has no zone path");
            continue;
        }
        zone.DisplayNameKey = row[1].Get<std::string>();
        if (!row[2].IsNull())
            zone.FarClip = row[2].Get<float>();
        if (!row[3].IsNull())
            zone.HealingPerMinute = row[3].Get<int32>();
        if (!row[4].IsNull())
            zone.SoftLimit = row[4].Get<int32>();
        if (!row[5].IsNull())
            zone.HardLimit = row[5].Get<int32>();
        zone.NoMounts = !row[6].IsNull() && row[6].Get<uint8>() != 0;
        std::string const path = zone.Path;
        if (!byPath.emplace(path, std::move(zone)).second)
            Report(errors, fmt::format("zone_template holds {} twice", path));
    } while (result->NextRow());
    return ZoneTemplates(std::move(byPath));
}

ZoneLocations ZoneMgr::ReadLocations(PreparedResultSet* result, ZoneTemplates const& templates, std::vector<std::string>& errors)
{
    std::map<std::string, std::vector<ZoneLocation>, std::less<>> byZone;
    if (!result)
        return ZoneLocations(std::move(byZone));
    do
    {
        Field const* const row = result->Fetch();
        uint64 const id = row[0].Get<uint64>();
        std::string const zone = row[1].Get<std::string>();
        if (!templates.Has(zone))
        {
            Report(errors, fmt::format("zone_location row {} names zone {}, which zone_template does not hold", id, zone));
            continue;
        }
        ZoneLocation location;
        location.Id = id;
        location.Name = row[2].Get<std::string>();
        if (location.Name.empty())
        {
            Report(errors, fmt::format("zone_location row {} of zone {} has no name", id, zone));
            continue;
        }
        location.X = row[3].Get<float>();
        location.Y = row[4].Get<float>();
        location.Z = row[5].Get<float>();
        location.Yaw = row[6].Get<float>();
        byZone[zone].push_back(std::move(location));
    } while (result->NextRow());
    return ZoneLocations(std::move(byZone));
}

ZoneObjects ZoneMgr::ReadObjects(PreparedResultSet* result, ZoneTemplates const& templates, std::vector<std::string>& errors)
{
    std::map<std::string, std::vector<ZoneObjectSpawn>, std::less<>> byZone;
    if (!result)
        return ZoneObjects(std::move(byZone));
    do
    {
        Field const* const row = result->Fetch();
        uint64 const id = row[0].Get<uint64>();
        std::string const zone = row[1].Get<std::string>();
        if (!templates.Has(zone))
        {
            Report(errors, fmt::format("zone_object row {} names zone {}, which zone_template does not hold", id, zone));
            continue;
        }
        ZoneObjectSpawn object;
        object.Id = id;
        object.ClassName = row[2].Get<std::string>();
        object.TemplateId = row[3].Get<uint64>();
        object.ObjectId = row[4].Get<uint32>();
        object.Position = { row[5].Get<float>(), row[6].Get<float>(), row[7].Get<float>() };
        object.Orientation = { row[8].Get<float>(), row[9].Get<float>(), row[10].Get<float>() };
        object.Scale = row[11].Get<float>();
        object.Tag = row[12].Get<std::string>();
        object.StartState = row[13].Get<std::string>();
        object.OverrideName = row[14].Get<std::string>();
        object.GlobalDynamic = row[15].Get<uint8>() != 0;
        object.Undetectable = row[16].Get<uint8>() != 0;
        uint8 const loading = row[17].Get<uint8>();
        if (loading > static_cast<uint8>(ZoneObjectLoading::DynamicServer))
        {
            Report(errors, fmt::format("zone_object row {} of zone {} has loading type {}, which the client does not have", id, zone, loading));
            continue;
        }
        object.Loading = static_cast<ZoneObjectLoading>(loading);
        object.HasSpawnRequirements = row[18].Get<uint8>() != 0;
        byZone[zone].push_back(std::move(object));
    } while (result->NextRow());
    return ZoneObjects(std::move(byZone));
}

GameTeleports ZoneMgr::ReadGameTeles(PreparedResultSet* result, ZoneTemplates const& templates, std::vector<std::string>& errors)
{
    std::map<std::string, std::vector<GameTelePoint>, std::less<>> byZone;
    if (!result)
        return GameTeleports(std::move(byZone));
    do
    {
        Field const* const row = result->Fetch();
        std::string const zone = row[0].Get<std::string>();
        std::string const name = row[1].Get<std::string>();
        if (!templates.Has(zone))
        {
            Report(errors, fmt::format("game_tele row {} names zone {}, which zone_template does not hold", name, zone));
            continue;
        }
        if (name.empty() || name.size() > MaxGameTeleNameLength)
        {
            Report(errors, fmt::format("game_tele row in {} has a name that is empty or longer than {} bytes", zone, MaxGameTeleNameLength));
            continue;
        }
        GameTelePoint point;
        point.Name = name;
        point.X = row[2].Get<float>();
        point.Y = row[3].Get<float>();
        point.Z = row[4].Get<float>();
        point.Yaw = row[5].Get<float>();
        if (!std::isfinite(point.X) || !std::isfinite(point.Y) || !std::isfinite(point.Z) || !std::isfinite(point.Yaw))
        {
            Report(errors, fmt::format("game_tele row {} in {} has a non-finite coordinate or direction", name, zone));
            continue;
        }
        std::vector<GameTelePoint>& points = byZone[zone];
        if (std::any_of(points.begin(), points.end(), [&point](GameTelePoint const& existing) { return existing.Name == point.Name; }))
        {
            Report(errors, fmt::format("game_tele holds {} in {} twice", name, zone));
            continue;
        }
        points.push_back(std::move(point));
    } while (result->NextRow());
    return GameTeleports(std::move(byZone));
}

bool ZoneMgr::LoadTemplates(std::vector<std::string>& errors)
{
    auto const statement = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_ZONE_TEMPLATES) : nullptr;
    if (!statement)
    {
        errors.emplace_back("the world database is not open, so the zones cannot be read");
        return false;
    }
    std::vector<std::string> found;
    ZoneTemplates templates = ReadTemplates(WorldDatabase.Query(*statement).get(), found);
    if (!found.empty())
    {
        errors.insert(errors.end(), found.begin(), found.end());
        return false;
    }
    _templates.Replace(std::move(templates));
    return true;
}

bool ZoneMgr::LoadLocations(std::vector<std::string>& errors)
{
    auto const statement = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_ZONE_LOCATIONS) : nullptr;
    if (!statement)
    {
        errors.emplace_back("the world database is not open, so the places inside the zones cannot be read");
        return false;
    }
    std::vector<std::string> found;
    ZoneLocations locations = ReadLocations(WorldDatabase.Query(*statement).get(), *GetTemplates(), found);
    if (!found.empty())
    {
        errors.insert(errors.end(), found.begin(), found.end());
        return false;
    }
    _locations.Replace(std::move(locations));
    return true;
}

bool ZoneMgr::LoadObjects(std::vector<std::string>& errors)
{
    auto const statement = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_ZONE_OBJECTS) : nullptr;
    if (!statement)
    {
        errors.emplace_back("the world database is not open, so the objects in the zones cannot be read");
        return false;
    }
    std::vector<std::string> found;
    ZoneObjects objects = ReadObjects(WorldDatabase.Query(*statement).get(), *GetTemplates(), found);
    if (!found.empty())
    {
        errors.insert(errors.end(), found.begin(), found.end());
        return false;
    }
    _objects.Replace(std::move(objects));
    return true;
}

bool ZoneMgr::LoadGameTeles(std::vector<std::string>& errors)
{
    auto const statement = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_GAME_TELES) : nullptr;
    if (!statement)
    {
        errors.emplace_back("the world database is not open, so GM teleport points cannot be read");
        return false;
    }
    std::vector<std::string> found;
    GameTeleports teleports = ReadGameTeles(WorldDatabase.Query(*statement).get(), *GetTemplates(), found);
    if (!found.empty())
    {
        errors.insert(errors.end(), found.begin(), found.end());
        return false;
    }
    _gameTeles.Replace(std::move(teleports));
    return true;
}

void ZoneMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(TemplateTarget), [this](std::vector<std::string>& errors) { return LoadTemplates(errors); });
    sReloadMgr.Register(std::string(LocationTarget), [this](std::vector<std::string>& errors) { return LoadLocations(errors); },
        { std::string(TemplateTarget) });
    sReloadMgr.Register(std::string(ObjectTarget), [this](std::vector<std::string>& errors) { return LoadObjects(errors); },
        { std::string(TemplateTarget) });
    sReloadMgr.Register(std::string(GameTeleTarget), [this](std::vector<std::string>& errors) { return LoadGameTeles(errors); },
        { std::string(TemplateTarget) });
}

ZoneLoadResult ZoneMgr::LoadAll()
{
    ZoneLoadResult outcome;
    auto const started = std::chrono::steady_clock::now();
    bool const templates = LoadTemplates(outcome.Errors);
    bool const locations = templates && LoadLocations(outcome.Errors);
    bool const objects = templates && LoadObjects(outcome.Errors);
    bool const gameTeles = templates && LoadGameTeles(outcome.Errors);
    outcome.Took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    outcome.Loaded = templates && locations && objects && gameTeles;
    outcome.Zones = GetTemplates()->Count();
    outcome.Locations = GetLocations()->Count();
    outcome.Objects = GetObjects()->Count();
    outcome.GameTeles = GetGameTeles()->Count();
    if (outcome.Loaded)
        LOG_INFO(ZoneLog, "Loaded {} zone(s), {} named place(s), {} placed object(s) and {} GM teleport point(s) in {} ms", outcome.Zones, outcome.Locations,
            outcome.Objects, outcome.GameTeles, outcome.Took.count());
    else
        for (std::string const& problem : outcome.Errors)
            LOG_ERROR(ZoneLog, "Zones: {}", problem);
    return outcome;
}

ZonePlace ZoneMgr::FindPlace(std::string_view zone, std::string_view name) const
{
    std::shared_ptr<ZoneTemplates const> const templates = GetTemplates();
    if (!templates->Has(zone))
    {
        ZonePlace place;
        place.Result = ZoneLookup::UnknownZone;
        return place;
    }
    return GetLocations()->Find(zone, name.empty() ? ZoneLocations::StartName : name);
}

std::optional<GameTelePoint> ZoneMgr::FindGameTele(std::string_view zone, std::string_view name) const
{
    std::shared_ptr<GameTeleports const> const teleports = GetGameTeles();
    GameTelePoint const* const point = teleports->Find(zone, name);
    return point == nullptr ? std::nullopt : std::optional<GameTelePoint>(*point);
}

bool ZoneMgr::AddGameTele(std::string_view zone, GameTelePoint point)
{
    if (point.Name.empty() || point.Name.size() > MaxGameTeleNameLength || !std::isfinite(point.X) || !std::isfinite(point.Y) ||
        !std::isfinite(point.Z) || !std::isfinite(point.Yaw) || !GetTemplates()->Has(zone))
        return false;
    GameTeleports teleports = *GetGameTeles();
    if (!teleports.Add(zone, std::move(point)))
        return false;
    _gameTeles.Replace(std::move(teleports));
    return true;
}

bool ZoneMgr::RemoveGameTele(std::string_view zone, std::string_view name)
{
    GameTeleports teleports = *GetGameTeles();
    if (!teleports.Remove(zone, name))
        return false;
    _gameTeles.Replace(std::move(teleports));
    return true;
}

std::optional<std::string> ZoneMgr::Describe(std::string_view zone) const
{
    std::shared_ptr<ZoneTemplates const> const templates = GetTemplates();
    ZoneTemplate const* const found = templates->Find(zone);
    if (found == nullptr)
        return std::nullopt;
    std::vector<ZoneLocation> const* const locations = GetLocations()->In(zone);
    std::vector<ZoneObjectSpawn> const* const objects = GetObjects()->In(zone);
    return fmt::format("{}: display name key {}, {} named place(s), {} placed object(s){}{}", found->Path,
        found->DisplayNameKey.empty() ? "none" : found->DisplayNameKey, locations == nullptr ? 0 : locations->size(),
        objects == nullptr ? 0 : objects->size(),
        found->SoftLimit ? fmt::format(", soft limit {}", *found->SoftLimit) : std::string(),
        found->NoMounts ? ", no mounts" : "");
}

void ZoneMgr::Clear()
{
    _templates.Replace(ZoneTemplates{});
    _locations.Replace(ZoneLocations{});
    _objects.Replace(ZoneObjects{});
    _gameTeles.Replace(GameTeleports{});
}

std::string_view ZoneMgr::GetLookupName(ZoneLookup lookup) noexcept
{
    switch (lookup)
    {
        case ZoneLookup::Ok: return "the place it asked for";
        case ZoneLookup::UnknownZone: return "no zone of that path";
        case ZoneLookup::NoLocations: return "that zone holds no place to stand";
        case ZoneLookup::FellBackToStart: return "that zone's Start, because the place it asked for is not there";
    }
    return "unknown";
}
