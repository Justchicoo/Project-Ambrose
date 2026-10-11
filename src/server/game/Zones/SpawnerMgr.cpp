/*
 * Project Ambrose by Imjustchico
 * Reads the spawners and their entries zone by zone and refuses a spawner whose zone no template holds, whose count or respawn time is past what a zone can mean, or whose entry names no template, a loading type the client does not have or a chance above a hundred, and an entry of a spawner that is not there; a spawner or entry with requirements fails closed until the requirement engine exists, so it places nothing. In an instance, a new set of spawners is met by keeping each spawner's live objects that its entries still place, up to its count, and taking away the rest; then every spawner tops itself up to its count, less the respawns still waiting, at once, which is how an instance first fills and how a raised count spawns only the difference. A placement that fails waits a minute before it is tried again rather than every tick. Entries are chosen by their chances, and when none of a spawner's entries has one, as in the shipped zones, by equal shares. An entry that stands on a path and names no place of its own, as every shipped entry does, is placed on a node of that path: any node for SNT_RANDOM, one no live object of its spawner holds for SNT_RANDOM_UNIQUE while one is free, the first or last for SNT_FIRST and SNT_LAST, and the node its start_node names for SNT_SPECIFIC, else the first, facing the node's direction when it has one; when its path is not loaded it waits a minute and tries again, so nothing piles up at the zone's origin. An object placed on a path whose template walks one starts walking it from that node. A scale of 0 reads as full size. A trigger's ResSpawn result starts its spawner in that instance and fills it at once; a ResDespawn stops it and takes its objects, or only those of the template it names, away with the effect it names, the KiStringHash of that name with the wizard who fired the trigger as killer, or plainly when it names none. Spawn results whose bytes the zones were extracted without are counted and skipped, and one that does not decode fails the load.
 */

#include "SpawnerMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"
#include "StringHash.h"
#include "ZonePathMgr.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace
{
    constexpr std::chrono::seconds FailedPlacementRetry{ 60 };

    bool Eligible(ZoneSpawnEntry const& entry) noexcept
    {
        return !entry.Object.HasSpawnRequirements;
    }

    std::optional<bool> SwitchOf(MapSpawnerState const& state, uint32 index)
    {
        auto const found = state.Switched.find(index);
        return found == state.Switched.end() ? std::nullopt : std::optional<bool>(found->second);
    }

    bool PlacesRow(ZoneSpawner const& spawner, ZoneObjectSpawn const& row)
    {
        return std::any_of(spawner.Entries.begin(), spawner.Entries.end(), [&row](ZoneSpawnEntry const& entry)
        {
            ZoneObjectSpawn placed = entry.Object;
            placed.Id = row.Id;
            if (!entry.HasPlace())
            {
                placed.Position = row.Position;
                placed.Orientation = row.Orientation;
            }
            return Eligible(entry) && placed == row;
        });
    }

    ZoneSpawnEntry const* Choose(ZoneSpawner const& spawner, SpawnerContext const& context)
    {
        uint32 const total = spawner.TotalChance();
        if (total == 0)
            return nullptr;
        uint32 roll = context.Roll ? context.Roll(total) % total : 0;
        for (ZoneSpawnEntry const& entry : spawner.Entries)
        {
            uint32 const weight = spawner.WeightOf(entry);
            if (weight == 0)
                continue;
            if (roll < weight)
                return &entry;
            roll -= weight;
        }
        return nullptr;
    }

    void TakeAway(Map& map, uint64 globalId, SpawnerContext const& context, MapObjectChanges& changes)
    {
        if (map.RemoveObject(globalId, context.Now, context.ReleaseDelay))
            changes.Removed.push_back(globalId);
    }

    void MeetNewSet(Map& map, std::vector<ZoneSpawner> const& spawners, SpawnerContext const& context, MapObjectChanges& changes)
    {
        MapSpawnerState& state = map.GetSpawnerState();
        for (auto live = state.Spawners.begin(); live != state.Spawners.end();)
        {
            auto const spawner = std::find_if(spawners.begin(), spawners.end(), [index = live->first](ZoneSpawner const& candidate) { return candidate.Index == index; });
            std::vector<uint64> kept;
            for (uint64 const id : live->second.Alive)
            {
                MapObject const* const object = map.FindObject(id);
                if (!object)
                    continue;
                if (spawner != spawners.end() && spawner->Spawns(SwitchOf(state, live->first)) && kept.size() < spawner->MaxSpawns && PlacesRow(*spawner, object->Spawn))
                    kept.push_back(id);
                else
                    TakeAway(map, id, context, changes);
            }
            if (spawner == spawners.end() || !spawner->Spawns(SwitchOf(state, live->first)))
            {
                live = state.Spawners.erase(live);
                continue;
            }
            live->second.Alive = std::move(kept);
            live->second.RespawnSeconds = spawner->RespawnSeconds;
            std::vector<Map::Clock::time_point>& respawns = live->second.Respawns;
            std::sort(respawns.begin(), respawns.end());
            std::size_t const room = spawner->MaxSpawns - live->second.Alive.size();
            if (respawns.size() > room)
                respawns.resize(room);
            ++live;
        }
    }
}

bool ZoneSpawnEntry::HasPlace() const noexcept
{
    return PathId == 0 || Object.Position.X != 0.0f || Object.Position.Y != 0.0f || Object.Position.Z != 0.0f;
}

bool ZoneSpawner::Spawns(std::optional<bool> active) const noexcept
{
    return active.value_or(Active) && !HasRequirements && MaxSpawns > 0 && TotalChance() > 0;
}

uint32 ZoneSpawner::TotalChance() const noexcept
{
    uint32 total = 0;
    for (ZoneSpawnEntry const& entry : Entries)
        total += WeightOf(entry);
    return total;
}

uint32 ZoneSpawner::WeightOf(ZoneSpawnEntry const& entry) const noexcept
{
    if (!Eligible(entry))
        return 0;
    bool const chanced = std::any_of(Entries.begin(), Entries.end(), [](ZoneSpawnEntry const& other) { return Eligible(other) && other.PercentChance > 0; });
    return chanced ? entry.PercentChance : 1;
}

ZoneSpawners::ZoneSpawners(std::map<std::string, std::vector<ZoneSpawner>, std::less<>> byZone) : _byZone(std::move(byZone))
{
}

std::vector<ZoneSpawner> const* ZoneSpawners::In(std::string_view zone) const
{
    auto const found = _byZone.find(zone);
    return found == _byZone.end() ? nullptr : &found->second;
}

std::vector<ZoneSpawnResult> const* ZoneSpawners::ResultsIn(std::string_view zone) const
{
    auto const found = _results.find(zone);
    return found == _results.end() ? nullptr : &found->second;
}

void ZoneSpawners::SetResults(std::map<std::string, std::vector<ZoneSpawnResult>, std::less<>> results)
{
    _results = std::move(results);
}

std::size_t ZoneSpawners::ResultCount() const noexcept
{
    std::size_t count = 0;
    for (auto const& [zone, results] : _results)
        count += results.size();
    return count;
}

std::size_t ZoneSpawners::Count() const noexcept
{
    std::size_t count = 0;
    for (auto const& [zone, spawners] : _byZone)
        count += spawners.size();
    return count;
}

std::size_t ZoneSpawners::ZoneCount() const noexcept
{
    return _byZone.size();
}

SpawnerMgr& SpawnerMgr::Instance()
{
    static SpawnerMgr instance;
    return instance;
}

std::optional<ZoneSpawners> SpawnerMgr::Build(std::vector<std::pair<std::string, ZoneSpawner>> spawners,
    std::vector<std::pair<std::string, std::pair<uint32, ZoneSpawnEntry>>> entries, std::set<std::string, std::less<>> const& zones, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    std::map<std::string, std::vector<ZoneSpawner>, std::less<>> byZone;
    for (auto& [zone, spawner] : spawners)
    {
        if (!zones.contains(zone))
            errors.push_back(fmt::format("zone_spawner {} {} ({}) names a zone no zone_template row holds", zone, spawner.Index, spawner.Name));
        else if (spawner.MaxSpawns > MaxSpawnsPerSpawner)
            errors.push_back(fmt::format("zone_spawner {} {} ({}) keeps {} alive, more than the {} a spawner may", zone, spawner.Index, spawner.Name, spawner.MaxSpawns, MaxSpawnsPerSpawner));
        else if (spawner.RespawnSeconds > MaxRespawnSeconds)
            errors.push_back(fmt::format("zone_spawner {} {} ({}) respawns after {} seconds, longer than the week a spawner may wait", zone, spawner.Index, spawner.Name,
                spawner.RespawnSeconds));
        else
            byZone[zone].push_back(std::move(spawner));
    }
    for (auto& [zone, held] : entries)
    {
        auto& [index, entry] = held;
        auto const found = byZone.find(zone);
        auto const spawner = found == byZone.end() ? std::vector<ZoneSpawner>::iterator{}
            : std::find_if(found->second.begin(), found->second.end(), [index](ZoneSpawner const& candidate) { return candidate.Index == index; });
        if (found == byZone.end() || spawner == found->second.end())
            errors.push_back(fmt::format("zone_spawner_entry {} {} {} belongs to a spawner that is not there", zone, index, entry.Position));
        else if (entry.Object.TemplateId == 0)
            errors.push_back(fmt::format("zone_spawner_entry {} {} {} places no template", zone, index, entry.Position));
        else if (entry.PercentChance > 100)
            errors.push_back(fmt::format("zone_spawner_entry {} {} {} has a chance of {}, above a hundred", zone, index, entry.Position, entry.PercentChance));
        else
            spawner->Entries.push_back(std::move(entry));
    }
    if (errors.size() != before)
        return std::nullopt;
    for (auto& [zone, list] : byZone)
    {
        std::sort(list.begin(), list.end(), [](ZoneSpawner const& left, ZoneSpawner const& right) { return left.Index < right.Index; });
        for (ZoneSpawner& spawner : list)
            std::sort(spawner.Entries.begin(), spawner.Entries.end(), [](ZoneSpawnEntry const& left, ZoneSpawnEntry const& right) { return left.Position < right.Position; });
    }
    return ZoneSpawners(std::move(byZone));
}

bool SpawnerMgr::Load(std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so no zone spawners were read");
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
    std::vector<std::pair<std::string, ZoneSpawner>> spawners;
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `spawner_index`, `name`, `spawner_id`, `active`, `max_spawns`, GREATEST(`spawn_time`, 0), "
        "`global_dynamic_reqs` IS NOT NULL AND LENGTH(`global_dynamic_reqs`) > 0 FROM `zone_spawner`", rows))
    {
        errors.push_back("zone_spawner could not be read");
        return false;
    }
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            ZoneSpawner spawner;
            spawner.Index = row[1].Get<uint32>();
            spawner.Name = row[2].Get<std::string>();
            spawner.SpawnerId = row[3].Get<uint64>();
            spawner.Active = row[4].Get<bool>();
            spawner.MaxSpawns = row[5].Get<uint32>();
            spawner.RespawnSeconds = static_cast<uint32>(row[6].Get<int64>());
            spawner.HasRequirements = row[7].Get<int64>() != 0;
            spawners.emplace_back(row[0].Get<std::string>(), std::move(spawner));
        } while (rows->NextRow());
    }
    std::vector<std::pair<std::string, std::pair<uint32, ZoneSpawnEntry>>> entries;
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `spawner_index`, `position`, `percent_chance`, `class_name`, `template_id`, `object_id`, `position_x`, `position_y`, `position_z`, "
        "`orientation_x`, `orientation_y`, `orientation_z`, `scale`, `zone_tag`, `start_state`, `override_name`, `global_dynamic`, `undetectable`, `loading_type`, "
        "`spawn_requirements` IS NOT NULL AND LENGTH(`spawn_requirements`) > 0, `start_node_type`, `path_id`, `start_node`, `unique_loc` FROM `zone_spawner_entry`", rows))
    {
        errors.push_back("zone_spawner_entry could not be read");
        return false;
    }
    if (rows)
    {
        do
        {
            Field const* row = rows->Fetch();
            ZoneSpawnEntry entry;
            entry.Position = row[2].Get<uint32>();
            entry.PercentChance = row[3].Get<uint32>();
            ZoneObjectSpawn& object = entry.Object;
            object.ClassName = row[4].Get<std::string>();
            object.TemplateId = row[5].Get<uint64>();
            object.ObjectId = row[6].Get<uint32>();
            object.Position = { row[7].Get<float>(), row[8].Get<float>(), row[9].Get<float>() };
            object.Orientation = { row[10].Get<float>(), row[11].Get<float>(), row[12].Get<float>() };
            object.Scale = row[13].Get<float>() > 0.0f ? row[13].Get<float>() : 1.0f;
            object.Tag = row[14].Get<std::string>();
            object.StartState = row[15].Get<std::string>();
            object.OverrideName = row[16].Get<std::string>();
            object.GlobalDynamic = row[17].Get<bool>();
            object.Undetectable = row[18].Get<bool>();
            uint32 const loading = row[19].Get<uint32>();
            object.HasSpawnRequirements = row[20].Get<int64>() != 0;
            entry.StartNodeType = row[21].Get<int32>();
            entry.PathId = row[22].Get<uint64>();
            entry.StartNode = row[23].Get<uint32>();
            entry.UniqueLoc = row[24].Get<int32>();
            std::string zone = row[0].Get<std::string>();
            uint32 const index = row[1].Get<uint32>();
            if (loading > static_cast<uint32>(ZoneObjectLoading::DynamicServer))
            {
                errors.push_back(fmt::format("zone_spawner_entry {} {} {} has the loading type {}, which the client does not have", zone, index, entry.Position, loading));
                continue;
            }
            object.Loading = static_cast<ZoneObjectLoading>(loading);
            entries.emplace_back(std::move(zone), std::make_pair(index, std::move(entry)));
        } while (rows->NextRow());
    }
    std::map<std::string, std::vector<ZoneSpawnResult>, std::less<>> results;
    if (!WorldDatabase.TryQuery(fmt::format("SELECT r.`zone_path`, t.`name`, r.`position`, r.`data` IS NOT NULL, r.`data` FROM `zone_trigger_result` r JOIN `zone_trigger` t "
        "ON t.`zone_path` = r.`zone_path` AND t.`trigger_index` = r.`trigger_index` WHERE r.`list` = 'results' AND r.`class_name` IN ('{}', '{}') "
        "ORDER BY r.`zone_path`, r.`trigger_index`, r.`position`", SpawnResultClass, DespawnResultClass), rows))
    {
        errors.push_back("zone_trigger_result could not be read");
        return false;
    }
    std::size_t unread = 0;
    if (rows)
    {
        TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
        do
        {
            Field const* row = rows->Fetch();
            if (!row[3].Get<bool>())
            {
                ++unread;
                continue;
            }
            std::string const zone = row[0].Get<std::string>();
            std::string const trigger = row[1].Get<std::string>();
            std::vector<uint8> const bytes = row[4].Get<std::vector<uint8>>();
            std::string error;
            std::optional<ZoneSpawnResult> result = ReadResult(catalog, bytes, error);
            if (!result)
            {
                errors.push_back(fmt::format("{} trigger {} has a spawn result at {} that does not read: {}", zone, trigger, row[2].Get<uint32>(), error));
                continue;
            }
            result->Trigger = trigger;
            result->Position = row[2].Get<uint32>();
            results[zone].push_back(std::move(*result));
        } while (rows->NextRow());
    }
    std::size_t const unplaced = static_cast<std::size_t>(std::count_if(entries.begin(), entries.end(), [](auto const& entry) { return !entry.second.second.HasPlace(); }));
    std::optional<ZoneSpawners> built = Build(std::move(spawners), std::move(entries), zones, errors);
    if (!built)
        return false;
    built->SetResults(std::move(results));
    if (unplaced > 0)
        LOG_INFO("server.world", "{} zone spawner entries stand on a path and are placed on one of its nodes", unplaced);
    if (unread > 0)
        LOG_WARN("server.world", "{} spawn result(s) of the zone triggers hold no bytes, since the zones were extracted before their classes were known; run `extractor zones` again "
            "to read them", unread);
    LOG_INFO("server.world", "Loaded {} zone spawner(s) in {} zone(s) and {} spawn result(s) of their triggers", built->Count(), built->ZoneCount(), built->ResultCount());
    Replace(std::move(*built));
    return true;
}

void SpawnerMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(ReloadTarget), [this](std::vector<std::string>& errors) { return Load(errors); });
}

void SpawnerMgr::Replace(ZoneSpawners spawners)
{
    _spawners.Replace(std::move(spawners));
}

void SpawnerMgr::SetRateReader(RateReader reader)
{
    _rate = std::move(reader);
}

void SpawnerMgr::Clear()
{
    _spawners.Replace(ZoneSpawners{});
    _rate = nullptr;
}

float SpawnerMgr::GetRespawnRate() const
{
    float const rate = _rate ? _rate() : 1.0f;
    return std::isfinite(rate) && rate > 0.0f ? rate : 1.0f;
}

std::size_t SpawnerMgr::PickNode(ZoneSpawnEntry const& entry, ZonePath const& path, std::set<uint64> const& taken, std::function<uint32(uint32)> const& roll)
{
    std::size_t const count = path.Nodes.size();
    auto const any = [&roll](std::size_t total) { return roll ? static_cast<std::size_t>(roll(static_cast<uint32>(total)) % total) : std::size_t{ 0 }; };
    switch (static_cast<SpawnStartNode>(entry.StartNodeType))
    {
        case SpawnStartNode::Random:
            return any(count);
        case SpawnStartNode::RandomUnique:
        {
            std::vector<std::size_t> free;
            for (std::size_t index = 0; index < count; ++index)
                if (!taken.contains(path.Nodes[index].Id))
                    free.push_back(index);
            return free.empty() ? any(count) : free[any(free.size())];
        }
        case SpawnStartNode::Last:
            return count - 1;
        case SpawnStartNode::Specific:
            for (std::size_t index = 0; index < count; ++index)
                if (path.Nodes[index].Id == entry.StartNode)
                    return index;
            return 0;
        case SpawnStartNode::First:
            break;
    }
    return 0;
}

void SpawnerMgr::UseWorldPaths(SpawnerContext& context, Map const& map)
{
    std::shared_ptr<ZonePaths const> paths = sZonePathMgr.Get();
    context.Paths = [paths = std::move(paths), zone = map.GetZonePath()](uint64 id) { return paths ? paths->Find(zone, id) : nullptr; };
    context.PathGeneration = sZonePathMgr.GetGeneration();
}

std::chrono::milliseconds SpawnerMgr::RespawnDelay(ZoneSpawner const& spawner, float rate)
{
    return std::chrono::milliseconds(static_cast<int64>(std::llround(static_cast<double>(spawner.RespawnSeconds) * 1000.0 * rate)));
}

MapObjectChanges SpawnerMgr::Update(Map& map, std::vector<ZoneSpawner> const& spawners, uint64 generation, SpawnerContext const& context)
{
    MapObjectChanges changes;
    changes.DynamicZoneId = map.GetDynamicZoneId();
    MapSpawnerState& state = map.GetSpawnerState();
    if (state.Generation != generation)
    {
        MeetNewSet(map, spawners, context, changes);
        state.Generation = generation;
    }
    for (ZoneSpawner const& spawner : spawners)
    {
        if (!spawner.Spawns(SwitchOf(state, spawner.Index)))
            continue;
        MapSpawnerLive& live = state.Spawners[spawner.Index];
        live.RespawnSeconds = spawner.RespawnSeconds;
        std::erase_if(live.Alive, [&map](uint64 id) { return map.FindObject(id) == nullptr; });
        std::erase_if(live.Nodes, [&map](auto const& held) { return map.FindObject(held.first) == nullptr; });
        std::erase_if(live.Respawns, [&context](Map::Clock::time_point due) { return due <= context.Now; });
        std::size_t const held = live.Alive.size() + live.Respawns.size();
        for (std::size_t count = held; count < spawner.MaxSpawns; ++count)
        {
            ZoneSpawnEntry const* const entry = Choose(spawner, context);
            if (!entry)
                break;
            ZoneObjectSpawn row = entry->Object;
            row.Id = state.NextSpawnId++;
            ZonePath const* path = nullptr;
            std::size_t node = 0;
            if (!entry->HasPlace())
            {
                path = context.Paths ? context.Paths(entry->PathId) : nullptr;
                if (!path || path->Nodes.empty())
                {
                    changes.Problems.push_back({ row.Id, row.TemplateId, false, fmt::format("zone_spawner {} entry {} stands on path {}, which zone_path does not hold for {}", spawner.Index,
                        entry->Position, entry->PathId, map.GetZonePath()) });
                    live.Respawns.push_back(context.Now + std::max<std::chrono::milliseconds>(RespawnDelay(spawner, context.RespawnRate), FailedPlacementRetry));
                    continue;
                }
                std::set<uint64> taken;
                for (auto const& [id, nodeId] : live.Nodes)
                    taken.insert(nodeId);
                node = PickNode(*entry, *path, taken, context.Roll);
                row.Position = path->Nodes[node].Position;
                if (path->Nodes[node].Direction != 0.0f)
                    row.Orientation.Z = path->Nodes[node].Direction;
            }
            std::optional<uint64> const placed = MapObjectSpawner::Place(map, row, MapObjectOrigin::Spawner, spawner.Index, context.Sources, context.Now, context.ReleaseDelay, changes);
            if (!placed)
            {
                live.Respawns.push_back(context.Now + std::max<std::chrono::milliseconds>(RespawnDelay(spawner, context.RespawnRate), FailedPlacementRetry));
                continue;
            }
            live.Alive.push_back(*placed);
            if (!path)
                continue;
            live.Nodes[*placed] = path->Nodes[node].Id;
            TemplateLookup const found = context.Sources.Templates ? context.Sources.Templates(static_cast<uint32>(row.TemplateId)) : TemplateLookup{};
            std::optional<PathWalkBehavior> const behavior = found.Template ? PathWalkers::BehaviorOf(*found.Template) : std::nullopt;
            std::string error;
            if (behavior && !PathWalkers::Start(map, *placed, *path, node, context.PathGeneration, *behavior, error) && !error.empty())
                changes.Problems.push_back({ row.Id, row.TemplateId, false, fmt::format("zone_spawner {} entry {} cannot walk path {}: {}", spawner.Index, entry->Position,
                    entry->PathId, error) });
        }
    }
    return changes;
}

bool SpawnerMgr::Despawn(Map& map, uint64 globalId, std::optional<uint32> effect, uint64 killer, SpawnerContext const& context, MapObjectChanges& changes)
{
    MapObject const* const object = map.FindObject(globalId);
    if (!object || object->Origin == MapObjectOrigin::Zone)
        return false;
    MapObjectOrigin const origin = object->Origin;
    uint32 const index = object->SpawnerIndex;
    if (!map.RemoveObject(globalId, context.Now, context.ReleaseDelay))
        return false;
    changes.DynamicZoneId = map.GetDynamicZoneId();
    if (effect)
        changes.Deleted.push_back({ globalId, killer, *effect });
    else
        changes.Removed.push_back(globalId);
    if (origin != MapObjectOrigin::Spawner)
        return true;
    MapSpawnerState& state = map.GetSpawnerState();
    auto const live = state.Spawners.find(index);
    if (live == state.Spawners.end())
        return true;
    std::erase(live->second.Alive, globalId);
    ZoneSpawner timing;
    timing.RespawnSeconds = live->second.RespawnSeconds;
    live->second.Respawns.push_back(context.Now + RespawnDelay(timing, context.RespawnRate));
    return true;
}

std::optional<uint64> SpawnerMgr::SpawnTemporary(Map& map, uint64 templateId, PropertyTypes::Vector3D const& position, float yaw, SpawnerContext const& context,
    MapObjectChanges& changes, MapObjectDecorator const& decorate)
{
    MapSpawnerState& state = map.GetSpawnerState();
    ZoneObjectSpawn row;
    row.Id = state.NextSpawnId++;
    row.TemplateId = templateId;
    row.Position = position;
    row.Orientation = { 0.0f, 0.0f, yaw };
    row.Loading = ZoneObjectLoading::DynamicServer;
    changes.DynamicZoneId = map.GetDynamicZoneId();
    return MapObjectSpawner::Place(map, row, MapObjectOrigin::Command, 0, context.Sources, context.Now, context.ReleaseDelay, changes, decorate);
}

MapObject const* SpawnerMgr::FindNearest(Map const& map, PropertyTypes::Vector3D const& position, float range)
{
    MapObject const* nearest = nullptr;
    float best = range * range;
    for (MapObject const& object : map.GetObjects())
    {
        if (object.Origin == MapObjectOrigin::Zone)
            continue;
        float const dx = object.Spawn.Position.X - position.X;
        float const dy = object.Spawn.Position.Y - position.Y;
        float const dz = object.Spawn.Position.Z - position.Z;
        float const distance = dx * dx + dy * dy + dz * dz;
        if (distance <= best)
        {
            best = distance;
            nearest = &object;
        }
    }
    return nearest;
}

std::optional<ZoneSpawnResult> SpawnerMgr::ReadResult(TypeCatalogPtr const& catalog, std::span<uint8 const> data, std::string& error)
{
    if (!catalog)
    {
        error = "no type catalog is loaded";
        return std::nullopt;
    }
    SerializerOptions options;
    options.Versionable = true;
    options.Flags = SerializerFlag::None;
    options.Mask = 0;
    options.AllowNullRoot = false;
    options.AllowTrailingBytes = false;
    DecodeResult const decoded = ObjectSerializer::Decode(catalog, data, options);
    if (!decoded.Ok() || !decoded.Object)
    {
        error = decoded.Detail.empty() ? std::string(ObjectSerializer::GetStatusName(decoded.Status)) : decoded.Detail;
        return std::nullopt;
    }
    ZoneSpawnResult result;
    result.Despawn = decoded.Object->IsA(DespawnResultClass);
    if (!result.Despawn && !decoded.Object->IsA(SpawnResultClass))
    {
        error = fmt::format("it holds a {}", decoded.Object->GetClass().Name);
        return std::nullopt;
    }
    PropertyValue const* const id = decoded.Object->Get("m_spawnID");
    uint64 const* const spawner = id ? id->GetIf<uint64>() : nullptr;
    if (!spawner)
    {
        error = "it names no spawner in m_spawnID";
        return std::nullopt;
    }
    result.SpawnerId = *spawner;
    if (result.Despawn)
    {
        PropertyValue const* const templateId = decoded.Object->Get("m_templateID");
        int32 const* const kind = templateId ? templateId->GetIf<int32>() : nullptr;
        result.TemplateId = kind && *kind > 0 ? static_cast<uint32>(*kind) : 0;
        PropertyValue const* const effect = decoded.Object->Get("m_despawnEffect");
        std::string const* const name = effect ? effect->GetIf<std::string>() : nullptr;
        result.Effect = name ? *name : std::string();
    }
    else
    {
        PropertyValue const* const activate = decoded.Object->Get("m_activate");
        bool const* const on = activate ? activate->GetIf<bool>() : nullptr;
        result.Activate = on && *on;
    }
    return result;
}

void SpawnerMgr::RunResults(Map& map, std::vector<ZoneSpawner> const& spawners, std::vector<ZoneSpawnResult> const& results, std::string_view trigger, uint64 wizard,
    SpawnerContext const& context, MapObjectChanges& changes)
{
    MapSpawnerState& state = map.GetSpawnerState();
    changes.DynamicZoneId = map.GetDynamicZoneId();
    for (ZoneSpawnResult const& result : results)
    {
        if (result.Trigger != trigger)
            continue;
        auto const spawner = std::find_if(spawners.begin(), spawners.end(), [&result](ZoneSpawner const& candidate) { return candidate.SpawnerId == result.SpawnerId; });
        if (spawner == spawners.end())
            continue;
        state.Switched[spawner->Index] = !result.Despawn;
        if (!result.Despawn)
            continue;
        auto const live = state.Spawners.find(spawner->Index);
        if (live == state.Spawners.end())
            continue;
        std::vector<uint64> kept;
        for (uint64 const id : live->second.Alive)
        {
            MapObject const* const object = map.FindObject(id);
            if (!object)
                continue;
            if (result.TemplateId != 0 && object->Spawn.TemplateId != result.TemplateId)
            {
                kept.push_back(id);
                continue;
            }
            if (!map.RemoveObject(id, context.Now, context.ReleaseDelay))
                continue;
            if (result.Effect.empty())
                changes.Removed.push_back(id);
            else
                changes.Deleted.push_back({ id, wizard, StringHash::KiStringHash(result.Effect) });
        }
        live->second.Alive = std::move(kept);
        live->second.Respawns.clear();
    }
}

SpawnerContext SpawnerMgr::WorldContext(Map::Clock::time_point now, std::chrono::milliseconds releaseDelay) const
{
    static thread_local std::mt19937 random{ std::random_device{}() };
    SpawnerContext context;
    context.Now = now;
    context.ReleaseDelay = releaseDelay;
    context.RespawnRate = GetRespawnRate();
    context.Sources = MapObjectSpawner::WorldSources();
    context.Roll = [](uint32 total) { return std::uniform_int_distribution<uint32>(0, total - 1)(random); };
    return context;
}

MapObjectChanges SpawnerMgr::UpdateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay)
{
    std::shared_ptr<ZoneSpawners const> const spawners = Get();
    std::vector<ZoneSpawner> const* const list = spawners ? spawners->In(map.GetZonePath()) : nullptr;
    static std::vector<ZoneSpawner> const none;
    bool const first = !map.GetSpawnerState().Generation.has_value();
    SpawnerContext context = WorldContext(now, releaseDelay);
    UseWorldPaths(context, map);
    MapObjectChanges changes = Update(map, list ? *list : none, GetGeneration(), context);
    if (first && !changes.Added.empty())
        LOG_INFO("server.zones", "{} spawner(s) placed {} object(s) in instance {} of {}", list ? list->size() : 0, changes.Added.size(), map.GetDynamicZoneId(), map.GetZonePath());
    return changes;
}

MapObjectChanges SpawnerMgr::TriggerFromWorld(Map& map, std::vector<std::string> const& fired, uint64 wizard, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay)
{
    MapObjectChanges changes;
    std::shared_ptr<ZoneSpawners const> const spawners = Get();
    std::vector<ZoneSpawner> const* const list = spawners ? spawners->In(map.GetZonePath()) : nullptr;
    std::vector<ZoneSpawnResult> const* const results = spawners ? spawners->ResultsIn(map.GetZonePath()) : nullptr;
    if (!list || !results || fired.empty())
        return changes;
    SpawnerContext context = WorldContext(now, releaseDelay);
    UseWorldPaths(context, map);
    for (std::string const& trigger : fired)
        RunResults(map, *list, *results, trigger, wizard, context, changes);
    changes.Absorb(Update(map, *list, GetGeneration(), context));
    if (changes.Changed())
        LOG_INFO("server.zones", "Triggers {} in instance {} of {} spawned {} and took away {} object(s)", fmt::join(fired, ", "), map.GetDynamicZoneId(), map.GetZonePath(),
            changes.Added.size(), changes.Removed.size() + changes.Deleted.size());
    return changes;
}

MapObjectChanges SpawnerMgr::PopulateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay)
{
    MapObjectChanges changes = MapObjectSpawner::PopulateFromWorld(map, now, releaseDelay);
    changes.Absorb(sSpawnerMgr.UpdateFromWorld(map, now, releaseDelay));
    changes.Absorb(PathWalkers::AdvanceFromWorld(map, now));
    return changes;
}
