/*
 * Project Ambrose by Imjustchico
 * Reads the spawners and their entries zone by zone and refuses a spawner whose zone no template holds, whose count or respawn time is past what a zone can mean, or whose entry names no template, a loading type the client does not have or a chance above a hundred, and an entry of a spawner that is not there; a spawner or entry with requirements fails closed until the requirement engine exists, so it places nothing. In an instance, a new set of spawners is met by keeping each spawner's live objects that its entries still place, up to its count, and taking away the rest; then every spawner tops itself up to its count, less the respawns still waiting, at once, which is how an instance first fills and how a raised count spawns only the difference. A placement that fails waits a minute before it is tried again rather than every tick.
 */

#include "SpawnerMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "ReloadMgr.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace
{
    constexpr std::chrono::seconds FailedPlacementRetry{ 60 };

    bool Eligible(ZoneSpawnEntry const& entry) noexcept
    {
        return entry.PercentChance > 0 && !entry.Object.HasSpawnRequirements;
    }

    bool PlacesRow(ZoneSpawner const& spawner, ZoneObjectSpawn const& row)
    {
        return std::any_of(spawner.Entries.begin(), spawner.Entries.end(), [&row](ZoneSpawnEntry const& entry)
        {
            ZoneObjectSpawn placed = entry.Object;
            placed.Id = row.Id;
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
            if (!Eligible(entry))
                continue;
            if (roll < entry.PercentChance)
                return &entry;
            roll -= entry.PercentChance;
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
                if (spawner != spawners.end() && spawner->Spawns() && kept.size() < spawner->MaxSpawns && PlacesRow(*spawner, object->Spawn))
                    kept.push_back(id);
                else
                    TakeAway(map, id, context, changes);
            }
            if (spawner == spawners.end() || !spawner->Spawns())
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

bool ZoneSpawner::Spawns() const noexcept
{
    return Active && !HasRequirements && MaxSpawns > 0 && TotalChance() > 0;
}

uint32 ZoneSpawner::TotalChance() const noexcept
{
    uint32 total = 0;
    for (ZoneSpawnEntry const& entry : Entries)
        if (Eligible(entry))
            total += entry.PercentChance;
    return total;
}

ZoneSpawners::ZoneSpawners(std::map<std::string, std::vector<ZoneSpawner>, std::less<>> byZone) : _byZone(std::move(byZone))
{
}

std::vector<ZoneSpawner> const* ZoneSpawners::In(std::string_view zone) const
{
    auto const found = _byZone.find(zone);
    return found == _byZone.end() ? nullptr : &found->second;
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
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `spawner_index`, `name`, `spawner_id`, `active`, `max_spawns`, `respawn_rate`, "
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
            spawner.RespawnSeconds = row[6].Get<uint32>();
            spawner.HasRequirements = row[7].Get<int64>() != 0;
            spawners.emplace_back(row[0].Get<std::string>(), std::move(spawner));
        } while (rows->NextRow());
    }
    std::vector<std::pair<std::string, std::pair<uint32, ZoneSpawnEntry>>> entries;
    if (!WorldDatabase.TryQuery("SELECT `zone_path`, `spawner_index`, `position`, `percent_chance`, `class_name`, `template_id`, `object_id`, `position_x`, `position_y`, `position_z`, "
        "`orientation_x`, `orientation_y`, `orientation_z`, `scale`, `zone_tag`, `start_state`, `override_name`, `global_dynamic`, `undetectable`, `loading_type`, "
        "`spawn_requirements` IS NOT NULL AND LENGTH(`spawn_requirements`) > 0, `start_node_type`, `path_id` FROM `zone_spawner_entry`", rows))
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
            object.Scale = row[13].Get<float>();
            object.Tag = row[14].Get<std::string>();
            object.StartState = row[15].Get<std::string>();
            object.OverrideName = row[16].Get<std::string>();
            object.GlobalDynamic = row[17].Get<bool>();
            object.Undetectable = row[18].Get<bool>();
            uint32 const loading = row[19].Get<uint32>();
            object.HasSpawnRequirements = row[20].Get<int64>() != 0;
            entry.StartNodeType = row[21].Get<int32>();
            entry.PathId = row[22].Get<uint64>();
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
    std::optional<ZoneSpawners> built = Build(std::move(spawners), std::move(entries), zones, errors);
    if (!built)
        return false;
    LOG_INFO("server.world", "Loaded {} zone spawner(s) in {} zone(s)", built->Count(), built->ZoneCount());
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
        if (!spawner.Spawns())
            continue;
        MapSpawnerLive& live = state.Spawners[spawner.Index];
        live.RespawnSeconds = spawner.RespawnSeconds;
        std::erase_if(live.Alive, [&map](uint64 id) { return map.FindObject(id) == nullptr; });
        std::erase_if(live.Respawns, [&context](Map::Clock::time_point due) { return due <= context.Now; });
        std::size_t const held = live.Alive.size() + live.Respawns.size();
        for (std::size_t count = held; count < spawner.MaxSpawns; ++count)
        {
            ZoneSpawnEntry const* const entry = Choose(spawner, context);
            if (!entry)
                break;
            ZoneObjectSpawn row = entry->Object;
            row.Id = state.NextSpawnId++;
            std::optional<uint64> const placed = MapObjectSpawner::Place(map, row, MapObjectOrigin::Spawner, spawner.Index, context.Sources, context.Now, context.ReleaseDelay, changes);
            if (placed)
                live.Alive.push_back(*placed);
            else
                live.Respawns.push_back(context.Now + std::max<std::chrono::milliseconds>(RespawnDelay(spawner, context.RespawnRate), FailedPlacementRetry));
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
    MapObjectChanges& changes)
{
    MapSpawnerState& state = map.GetSpawnerState();
    ZoneObjectSpawn row;
    row.Id = state.NextSpawnId++;
    row.TemplateId = templateId;
    row.Position = position;
    row.Orientation = { 0.0f, 0.0f, yaw };
    row.Loading = ZoneObjectLoading::DynamicServer;
    changes.DynamicZoneId = map.GetDynamicZoneId();
    return MapObjectSpawner::Place(map, row, MapObjectOrigin::Command, 0, context.Sources, context.Now, context.ReleaseDelay, changes);
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
    MapObjectChanges changes = Update(map, list ? *list : none, GetGeneration(), WorldContext(now, releaseDelay));
    if (first && !changes.Added.empty())
        LOG_INFO("server.zones", "{} spawner(s) placed {} object(s) in instance {} of {}", list ? list->size() : 0, changes.Added.size(), map.GetDynamicZoneId(), map.GetZonePath());
    return changes;
}

MapObjectChanges SpawnerMgr::PopulateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay)
{
    MapObjectChanges changes = MapObjectSpawner::PopulateFromWorld(map, now, releaseDelay);
    changes.Absorb(sSpawnerMgr.UpdateFromWorld(map, now, releaseDelay));
    return changes;
}
