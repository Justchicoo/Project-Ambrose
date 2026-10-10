/*
 * Project Ambrose by Imjustchico
 * Every zone's paths from zone_path and zone_path_node (sZonePathMgr), each with its nodes in the order the path visits them, read at start and again by `.reload zone_path`, which builds the new set off to the side, validates it and swaps it in only when every row is good, keeping the old set and reporting each error otherwise.
 */

#ifndef AMBROSE_ZONEPATHMGR_H
#define AMBROSE_ZONEPATHMGR_H

#include "PropertyValue.h"
#include "ReloadableStore.h"
#include "Types.h"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ZonePathNode
{
    uint64 Id = 0;
    PropertyTypes::Vector3D Position;
    float Radius = 0.0f;
    float Direction = 0.0f;
    float Roll = 0.0f;

    bool operator==(ZonePathNode const&) const = default;
};

struct ZonePath
{
    uint64 Id = 0;
    std::string Name;
    std::vector<ZonePathNode> Nodes;

    bool operator==(ZonePath const&) const = default;
};

struct ZonePathNodeRow
{
    std::string Zone;
    uint64 PathId = 0;
    uint32 Position = 0;
    ZonePathNode Node;
};

class ZonePaths
{
public:
    ZonePaths() = default;
    explicit ZonePaths(std::map<std::string, std::map<uint64, ZonePath>, std::less<>> byZone);

    ZonePath const* Find(std::string_view zone, uint64 pathId) const;
    std::size_t Count() const noexcept;
    std::size_t NodeCount() const noexcept;
    std::size_t ZoneCount() const noexcept { return _byZone.size(); }

private:
    std::map<std::string, std::map<uint64, ZonePath>, std::less<>> _byZone;
};

class ZonePathMgr
{
public:
    static constexpr std::string_view ReloadTarget = "zone_path";

    static ZonePathMgr& Instance();

    ZonePathMgr(ZonePathMgr const&) = delete;
    ZonePathMgr& operator=(ZonePathMgr const&) = delete;

    static std::optional<ZonePaths> Build(std::vector<std::pair<std::string, ZonePath>> paths, std::vector<ZonePathNodeRow> nodes, std::set<std::string, std::less<>> const& zones,
        std::vector<std::string>& errors);

    static std::optional<uint64> CountRows();

    bool Load(std::vector<std::string>& errors);
    void RegisterReloadTargets();
    void Replace(ZonePaths paths);
    void Clear();

    std::shared_ptr<ZonePaths const> Get() const { return _paths.Get(); }
    uint64 GetGeneration() const noexcept { return _paths.GetGeneration(); }

private:
    ZonePathMgr() = default;

    ReloadableStore<ZonePaths> _paths;
};

#define sZonePathMgr ZonePathMgr::Instance()

#endif
