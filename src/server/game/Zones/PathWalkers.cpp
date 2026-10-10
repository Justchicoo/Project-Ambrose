/*
 * Project Ambrose by Imjustchico
 * Reads a template's path and path movement behaviors by property name, builds a walker's route from its path's nodes and moves each walker of an instance along it, keeping its place on the instance's object so sight and nearest-object queries follow it, and facing it the way it travels.
 */

#include "PathWalkers.h"
#include "Log.h"
#include "PropertyObject.h"
#include "Settings.h"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
    PropertyObject const* BehaviorNamed(ObjectTemplate const& objectTemplate, std::string_view className)
    {
        if (!objectTemplate.Object)
            return nullptr;
        PropertyValue const* const behaviors = objectTemplate.Object->Get("m_behaviors");
        PropertyValue::List const* const list = behaviors ? behaviors->GetList() : nullptr;
        if (!list)
            return nullptr;
        for (PropertyValue const& entry : *list)
            if (PropertyObject const* const behavior = entry.AsObject(); behavior && behavior->GetClass().Name == className)
                return behavior;
        return nullptr;
    }

    std::optional<float> RealOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        float const* const held = value ? value->GetIf<float>() : nullptr;
        return held ? std::optional<float>(*held) : std::nullopt;
    }

    std::optional<int64> WholeOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        if (!value)
            return std::nullopt;
        if (auto const* held = value->GetIf<int32>()) return *held;
        if (auto const* held = value->GetIf<int64>()) return *held;
        if (auto const* held = value->GetIf<uint32>()) return *held;
        if (auto const* held = value->GetIf<uint64>()) return static_cast<int64>(*held);
        return std::nullopt;
    }

    std::vector<PathMovementNode> RouteOf(ZonePath const& path)
    {
        std::vector<PathMovementNode> route;
        route.reserve(path.Nodes.size());
        for (ZonePathNode const& node : path.Nodes)
            route.push_back({ static_cast<uint32>(node.Id), { node.Position.X, node.Position.Y, node.Position.Z }, std::chrono::milliseconds(0) });
        return route;
    }

    PropertyTypes::Vector3D PointOf(PathMovementPosition const& position)
    {
        return { static_cast<float>(position.X), static_cast<float>(position.Y), static_cast<float>(position.Z) };
    }

    std::size_t IndexOf(ZonePath const& path, uint32 nodeId)
    {
        for (std::size_t index = 0; index < path.Nodes.size(); ++index)
            if (path.Nodes[index].Id == nodeId)
                return index;
        return 0;
    }
}

std::optional<PathWalkBehavior> PathWalkers::BehaviorOf(ObjectTemplate const& objectTemplate)
{
    PropertyObject const* const pathBehavior = BehaviorNamed(objectTemplate, PathBehaviorClass);
    PropertyObject const* const movement = BehaviorNamed(objectTemplate, MovementBehaviorClass);
    if (!pathBehavior || !movement)
        return std::nullopt;
    std::optional<float> const speed = RealOf(*movement, "m_movementSpeed");
    std::optional<float> const scale = RealOf(*movement, "m_movementScale");
    if (!speed || !scale || !std::isfinite(*speed) || !std::isfinite(*scale) || *speed <= 0.0f || *scale <= 0.0f)
        return std::nullopt;
    PathWalkBehavior behavior;
    behavior.Speed = *speed;
    behavior.Scale = *scale;
    behavior.Traversal = WholeOf(*pathBehavior, "m_kPathType").value_or(PathTypeLoop) == PathTypeChain ? PathTraversalMode::PingPong : PathTraversalMode::Loop;
    behavior.Direction = WholeOf(*pathBehavior, "m_nPathDirection").value_or(1) < 0 ? PathInitialDirection::Reverse : PathInitialDirection::Forward;
    return behavior;
}

bool PathWalkers::Start(Map& map, uint64 globalId, ZonePath const& path, std::size_t startNode, uint64 pathGeneration, PathWalkBehavior const& behavior, std::string& error)
{
    if (path.Nodes.size() < 2)
        return false;
    std::optional<PathMovementGenerator> generator = PathMovementGenerator::Create(RouteOf(path), startNode, behavior.Speed, behavior.Scale, behavior.Traversal, behavior.Direction, error);
    if (!generator)
        return false;
    map.GetWalkers().insert_or_assign(globalId, MapWalker{ path.Id, pathGeneration, std::move(*generator), behavior.Traversal, behavior.Direction, true, false });
    return true;
}

MapObjectChanges PathWalkers::Advance(Map& map, Map::Clock::time_point now, std::chrono::milliseconds sendInterval, PathLookup const& paths, uint64 pathGeneration)
{
    MapObjectChanges changes;
    changes.DynamicZoneId = map.GetDynamicZoneId();
    MapWalkState& walk = map.GetWalkState();
    if (map.GetWalkers().empty() || map.GetPlayerCount() == 0)
    {
        walk.WalkedAt.reset();
        return changes;
    }
    std::chrono::duration<double> const elapsed = walk.WalkedAt ? std::chrono::duration<double>(now - *walk.WalkedAt) : std::chrono::duration<double>(0.0);
    walk.WalkedAt = now;
    bool const send = !walk.SentAt || now - *walk.SentAt >= sendInterval;
    if (send)
        walk.SentAt = now;
    std::vector<uint64> stopped;
    for (auto& [id, walker] : map.GetWalkers())
    {
        MapObject const* const object = map.FindObject(id);
        if (!object)
        {
            stopped.push_back(id);
            continue;
        }
        if (walker.PathGeneration != pathGeneration)
        {
            ZonePath const* const path = paths ? paths(walker.PathId) : nullptr;
            std::string error;
            std::optional<PathMovementGenerator> rerouted;
            if (path && path->Nodes.size() >= 2)
                rerouted = PathMovementGenerator::Create(RouteOf(*path), IndexOf(*path, walker.Generator.GetNextNodeId()), walker.Generator.GetSpeed(), 1.0f, walker.Traversal,
                    walker.Direction, error);
            if (!rerouted)
            {
                LOG_INFO("server.zones", "Object {} in instance {} of {} stops walking: path {} {}", id, map.GetDynamicZoneId(), map.GetZonePath(), walker.PathId,
                    error.empty() ? "is no longer there" : error);
                stopped.push_back(id);
                changes.Moved.push_back({ id, object->MobileId, object->Spawn.Position, object->Spawn.Orientation.Z, StandingState });
                continue;
            }
            walker.Generator = std::move(*rerouted);
            walker.PathGeneration = pathGeneration;
        }
        PathMovementPosition const before = walker.Generator.GetPosition();
        std::string error;
        std::optional<PathMovementStep> const step = walker.Generator.Advance(elapsed, error);
        if (!step)
        {
            LOG_WARN("server.zones", "Object {} in instance {} of {} stops walking path {}: {}", id, map.GetDynamicZoneId(), map.GetZonePath(), walker.PathId, error);
            stopped.push_back(id);
            continue;
        }
        float yaw = object->Spawn.Orientation.Z;
        double const dx = step->Position.X - before.X;
        double const dy = step->Position.Y - before.Y;
        if (dx != 0.0 || dy != 0.0)
            yaw = static_cast<float>(std::atan2(dy, dx));
        map.MoveObject(id, PointOf(step->Position), yaw);
        std::optional<int8> state;
        if (step->Moving != walker.Moving || !walker.Shown)
            state = step->Moving ? WalkingState : StandingState;
        if (send && (step->Changed || state))
        {
            changes.Moved.push_back({ id, object->MobileId, PointOf(step->Position), yaw, state });
            walker.Moving = step->Moving;
            walker.Shown = true;
        }
    }
    for (uint64 const id : stopped)
        map.GetWalkers().erase(id);
    return changes;
}

MapObjectChanges PathWalkers::AdvanceFromWorld(Map& map, Map::Clock::time_point now)
{
    std::shared_ptr<ZonePaths const> const paths = sZonePathMgr.Get();
    std::string const zone = map.GetZonePath();
    PathLookup const lookup = [&paths, &zone](uint64 id) { return paths ? paths->Find(zone, id) : nullptr; };
    return Advance(map, now, std::chrono::milliseconds(sSettings.Get<uint32>("Zone.MoveFlushInterval")), lookup, sZonePathMgr.GetGeneration());
}
