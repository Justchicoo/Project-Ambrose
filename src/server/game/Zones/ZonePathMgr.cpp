/*
 * Project Ambrose by Imjustchico
 * Reads the paths and their nodes zone by zone and refuses a path whose zone no template holds, a path with no nodes, a node of a path that is not there, a node list whose positions do not run from 0 without a gap, and a node whose place or direction is not a finite number; and counts the path rows, so a world database extracted before paths were read can be told apart from one whose zones have none.
 */

#include "ZonePathMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>
#include <utility>

ZonePaths::ZonePaths(std::map<std::string, std::map<uint64, ZonePath>, std::less<>> byZone) : _byZone(std::move(byZone))
{
}

ZonePath const* ZonePaths::Find(std::string_view zone, uint64 pathId) const
{
    auto const paths = _byZone.find(zone);
    if (paths == _byZone.end())
        return nullptr;
    auto const path = paths->second.find(pathId);
    return path == paths->second.end() ? nullptr : &path->second;
}

std::size_t ZonePaths::Count() const noexcept
{
    std::size_t count = 0;
    for (auto const& [zone, paths] : _byZone)
        count += paths.size();
    return count;
}

std::size_t ZonePaths::NodeCount() const noexcept
{
    std::size_t count = 0;
    for (auto const& [zone, paths] : _byZone)
        for (auto const& [id, path] : paths)
            count += path.Nodes.size();
    return count;
}

ZonePathMgr& ZonePathMgr::Instance()
{
    static ZonePathMgr instance;
    return instance;
}

std::optional<ZonePaths> ZonePathMgr::Build(std::vector<std::pair<std::string, ZonePath>> paths, std::vector<ZonePathNodeRow> nodes, std::set<std::string, std::less<>> const& zones,
    std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    std::map<std::string, std::map<uint64, ZonePath>, std::less<>> byZone;
    for (auto& [zone, path] : paths)
    {
        if (!zones.contains(zone))
        {
            errors.push_back(fmt::format("zone_path {} {} ({}) names a zone no zone_template holds", zone, path.Id, path.Name));
            continue;
        }
        uint64 const id = path.Id;
        byZone[zone].emplace(id, std::move(path));
    }
    std::sort(nodes.begin(), nodes.end(), [](ZonePathNodeRow const& left, ZonePathNodeRow const& right)
    {
        return std::tie(left.Zone, left.PathId, left.Position) < std::tie(right.Zone, right.PathId, right.Position);
    });
    for (ZonePathNodeRow& row : nodes)
    {
        auto const zone = byZone.find(row.Zone);
        auto const found = zone == byZone.end() ? std::optional<std::map<uint64, ZonePath>::iterator>() : zone->second.find(row.PathId);
        ZonePath* const path = found && *found != zone->second.end() ? &(*found)->second : nullptr;
        if (!path)
        {
            errors.push_back(fmt::format("zone_path_node {} {} {} belongs to a path that is not there", row.Zone, row.PathId, row.Position));
            continue;
        }
        if (row.Position != path->Nodes.size())
        {
            errors.push_back(fmt::format("zone_path_node {} {} {} does not follow the path's node {}", row.Zone, row.PathId, row.Position, path->Nodes.size()));
            continue;
        }
        PropertyTypes::Vector3D const& at = row.Node.Position;
        if (!std::isfinite(at.X) || !std::isfinite(at.Y) || !std::isfinite(at.Z) || !std::isfinite(row.Node.Direction))
        {
            errors.push_back(fmt::format("zone_path_node {} {} {} has a place or direction that is not a number", row.Zone, row.PathId, row.Position));
            continue;
        }
        path->Nodes.push_back(row.Node);
    }
    for (auto const& [zone, list] : byZone)
        for (auto const& [id, path] : list)
            if (path.Nodes.empty())
                errors.push_back(fmt::format("zone_path {} {} ({}) has no nodes", zone, id, path.Name));
    if (errors.size() != before)
        return std::nullopt;
    return ZonePaths(std::move(byZone));
}

std::optional<uint64> ZonePathMgr::CountRows()
{
    QueryResult rows;
    if (!WorldDatabase.IsOpen() || !WorldDatabase.TryQuery("SELECT COUNT(*) FROM `zone_path`", rows) || !rows)
        return std::nullopt;
    return rows->Fetch()[0].Get<uint64>();
}

bool ZonePathMgr::Load(std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so no zone paths were read");
        return false;
    }
    QueryResult rows;
    std::set<std::string, std::less<>> zones;
    if (!WorldDatabase.TryQuery("SELECT `zone_path` FROM `zone_template`", rows))
    {
        errors.push_back("zone_template could not be read");
        return false;
    }
    if (rows)
        do
            zones.insert(rows->Fetch()[0].Get<std::string>());
        while (rows->NextRow());
    std::vector<std::pair<std::string, ZonePath>> paths;
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `path_id`, `name` FROM `zone_path`", rows))
    {
        errors.push_back("zone_path could not be read");
        return false;
    }
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            ZonePath path;
            path.Id = row[1].Get<uint64>();
            path.Name = row[2].Get<std::string>();
            paths.emplace_back(row[0].Get<std::string>(), std::move(path));
        } while (rows->NextRow());
    }
    std::vector<ZonePathNodeRow> nodes;
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `path_id`, `position`, `node_id`, `position_x`, `position_y`, `position_z`, `radius`, `direction`, `roll` FROM `zone_path_node`", rows))
    {
        errors.push_back("zone_path_node could not be read");
        return false;
    }
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            ZonePathNodeRow node;
            node.Zone = row[0].Get<std::string>();
            node.PathId = row[1].Get<uint64>();
            node.Position = row[2].Get<uint32>();
            node.Node.Id = row[3].Get<uint64>();
            node.Node.Position = { row[4].Get<float>(), row[5].Get<float>(), row[6].Get<float>() };
            node.Node.Radius = row[7].Get<float>();
            node.Node.Direction = row[8].Get<float>();
            node.Node.Roll = row[9].Get<float>();
            nodes.push_back(std::move(node));
        } while (rows->NextRow());
    }
    std::optional<ZonePaths> built = Build(std::move(paths), std::move(nodes), zones, errors);
    if (!built)
        return false;
    LOG_INFO("server.world", "Loaded {} zone path(s) with {} node(s) in {} zone(s)", built->Count(), built->NodeCount(), built->ZoneCount());
    Replace(std::move(*built));
    return true;
}

void ZonePathMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(ReloadTarget), [this](std::vector<std::string>& errors) { return Load(errors); });
}

void ZonePathMgr::Replace(ZonePaths paths)
{
    _paths.Replace(std::move(paths));
}

void ZonePathMgr::Clear()
{
    _paths.Replace(ZonePaths{});
}
