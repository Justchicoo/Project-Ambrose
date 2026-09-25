/*
 * Project Ambrose by Imjustchico
 * Reads the three zone tables into stores that are swapped in whole. A location or an object naming a zone no template knows is a refusal with that row named rather than a row quietly dropped, because a place nothing can load is how a wizard ends up nowhere, and the refusal leaves the rows already serving in place. Coordinates come out of the JSON the extractor wrote, [x,y,z] for a place, and a direction is taken only as far as its shape allows: one number is a yaw, three are the pitch, yaw and roll the extractor writes for an euler, and four are a quaternion whose yaw is left unanswered rather than converted through a convention this project has not established. Only the first few problems of a build are reported, because a table that is wrong is usually wrong in every row and an operator needs the shape of it rather than all of it.
 */

#include "ZoneMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "WorldDatabase.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <utility>

namespace
{
    constexpr char const* ZoneLog = "server.zones";

    bool Numbers(std::string_view text, std::vector<double>& into)
    {
        nlohmann::json const held = nlohmann::json::parse(text, nullptr, false);
        if (held.is_number())
        {
            into.push_back(held.get<double>());
            return true;
        }
        if (!held.is_array())
            return false;
        for (nlohmann::json const& value : held)
        {
            if (!value.is_number())
                return false;
            into.push_back(value.get<double>());
        }
        return true;
    }

    bool ReadPoint(Field const& field, float& x, float& y, float& z)
    {
        if (field.IsNull())
            return true;
        std::vector<double> parts;
        if (!Numbers(field.Get<std::string>(), parts) || parts.size() < 3)
            return false;
        x = static_cast<float>(parts[0]);
        y = static_cast<float>(parts[1]);
        z = static_cast<float>(parts[2]);
        return true;
    }

    bool ReadYaw(Field const& field, std::optional<float>& yaw)
    {
        if (field.IsNull())
            return true;
        std::vector<double> parts;
        if (!Numbers(field.Get<std::string>(), parts))
            return false;
        if (parts.size() == 1)
            yaw = static_cast<float>(parts[0]);
        else if (parts.size() == 3)
            yaw = static_cast<float>(parts[1]);
        else if (parts.size() != 4)
            return false;
        return true;
    }

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
        if (!ReadPoint(row[3], location.X, location.Y, location.Z))
        {
            Report(errors, fmt::format("zone_location row {} of zone {} has a location this server cannot read", id, zone));
            continue;
        }
        if (!ReadYaw(row[4], location.Yaw))
        {
            Report(errors, fmt::format("zone_location row {} of zone {} has a direction this server cannot read", id, zone));
            continue;
        }
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
        object.ObjectId = row[2].Get<uint32>();
        if (!row[3].IsNull())
            object.TemplateId = row[3].Get<uint64>();
        if (!ReadPoint(row[4], object.X, object.Y, object.Z))
        {
            Report(errors, fmt::format("zone_object row {} of zone {} has a location this server cannot read", id, zone));
            continue;
        }
        if (!ReadYaw(row[5], object.Yaw))
        {
            Report(errors, fmt::format("zone_object row {} of zone {} has an orientation this server cannot read", id, zone));
            continue;
        }
        if (!row[6].IsNull())
            object.Scale = row[6].Get<float>();
        object.Tag = row[7].Get<std::string>();
        if (!row[8].IsNull())
            object.StartState = row[8].Get<int64>();
        if (!row[9].IsNull())
            object.LoadingType = row[9].Get<int64>();
        byZone[zone].push_back(std::move(object));
    } while (result->NextRow());
    return ZoneObjects(std::move(byZone));
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

void ZoneMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(TemplateTarget), [this](std::vector<std::string>& errors) { return LoadTemplates(errors); });
    sReloadMgr.Register(std::string(LocationTarget), [this](std::vector<std::string>& errors) { return LoadLocations(errors); },
        { std::string(TemplateTarget) });
    sReloadMgr.Register(std::string(ObjectTarget), [this](std::vector<std::string>& errors) { return LoadObjects(errors); },
        { std::string(TemplateTarget) });
}

ZoneLoadResult ZoneMgr::LoadAll()
{
    ZoneLoadResult outcome;
    auto const started = std::chrono::steady_clock::now();
    bool const templates = LoadTemplates(outcome.Errors);
    bool const locations = templates && LoadLocations(outcome.Errors);
    bool const objects = templates && LoadObjects(outcome.Errors);
    outcome.Took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    outcome.Loaded = templates && locations && objects;
    outcome.Zones = GetTemplates()->Count();
    outcome.Locations = GetLocations()->Count();
    outcome.Objects = GetObjects()->Count();
    if (outcome.Loaded)
        LOG_INFO(ZoneLog, "Loaded {} zone(s), {} named place(s) and {} placed object(s) in {} ms", outcome.Zones, outcome.Locations,
            outcome.Objects, outcome.Took.count());
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
