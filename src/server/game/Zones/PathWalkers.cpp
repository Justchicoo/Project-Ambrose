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

    std::size_t IndexOf(ZonePath const& path, uint64 nodeId)
    {
        for (std::size_t index = 0; index < path.Nodes.size(); ++index)
            if (path.Nodes[index].Id == nodeId)
                return index;
        return 0;
    }

    PathEmoteEncoder& WorldEmoteEncoder()
    {
        static PathEmoteEncoder encoder;
        return encoder;
    }

    struct NodeOutcome
    {
        bool Teleported = false;
        bool Despawn = false;
        bool Halt = false;
    };

    uint32 Roll(PathWalkHooks const& hooks, uint32 total)
    {
        return hooks.Roll && total > 0 ? hooks.Roll(total) % total : 0;
    }

    bool Passes(PathWalkHooks const& hooks, int32 chance)
    {
        return chance > 0 && static_cast<int32>(Roll(hooks, 100)) < chance;
    }

    std::chrono::duration<double> Between(PathWalkHooks const& hooks, float least, float most)
    {
        double const low = std::isfinite(least) ? std::max(0.0, static_cast<double>(least)) : 0.0;
        double const high = std::isfinite(most) ? std::max(low, static_cast<double>(most)) : low;
        return std::chrono::duration<double>(low + (high - low) * Roll(hooks, 1001) / 1000.0);
    }

    NodeOutcome RunNode(Map& map, uint64 id, MapWalker& walker, uint32 nodeId, PathLookup const& paths, PathWalkHooks const& hooks, MapObjectChanges& changes)
    {
        NodeOutcome outcome;
        if (walker.PauseChance > 0 && walker.PauseSeconds > 0.0f && Passes(hooks, static_cast<int32>(walker.PauseChance)))
            walker.Generator.Hold(std::chrono::duration<double>(walker.PauseSeconds));
        if (!walker.Actions)
            return outcome;
        for (PathAction const* const action : walker.Actions->At(walker.PathId, nodeId))
        {
            if (!Passes(hooks, action->Chance))
                continue;
            switch (action->Kind)
            {
                case PathActionKind::Pause:
                    walker.Generator.Hold(Between(hooks, action->MinSeconds, action->MaxSeconds));
                    break;
                case PathActionKind::Emote:
                {
                    std::optional<std::string> data = hooks.Emote ? hooks.Emote(action->Name) : std::nullopt;
                    if (!data)
                    {
                        LOG_DEBUG("server.zones", "Object {} in instance {} of {} cannot play the emote {} at node {} of path {}", id, map.GetDynamicZoneId(), map.GetZonePath(),
                            action->Name, nodeId, walker.PathId);
                        break;
                    }
                    if (walker.Emoting)
                        changes.States.push_back({ id, PathWalkers::IdleState, {} });
                    changes.States.push_back({ id, PathWalkers::EmotingState, std::move(*data) });
                    walker.Emoting = true;
                    walker.Generator.Hold(Between(hooks, action->MinSeconds, action->MaxSeconds));
                    break;
                }
                case PathActionKind::Despawn:
                    outcome.Despawn = true;
                    return outcome;
                case PathActionKind::ZoneEvent:
                    if (!action->Name.empty())
                        changes.ZoneEvents.push_back(action->Name);
                    break;
                case PathActionKind::ChangeDirection:
                    walker.Generator.Reverse();
                    break;
                case PathActionKind::ChangePath:
                {
                    ZonePath const* const destination = paths ? paths(action->DestinationPathId) : nullptr;
                    std::string error;
                    std::optional<PathMovementGenerator> moved;
                    if (destination && destination->Nodes.size() >= 2 && action->DestinationNodeId >= 0)
                        moved = PathMovementGenerator::Create(RouteOf(*destination), IndexOf(*destination, static_cast<uint64>(action->DestinationNodeId)),
                            walker.Generator.GetSpeed(), 1.0f, walker.Traversal, walker.Generator.GetDirection(), error);
                    if (!moved)
                    {
                        LOG_INFO("server.zones", "Object {} in instance {} of {} stays on path {}: path {} {}", id, map.GetDynamicZoneId(), map.GetZonePath(), walker.PathId,
                            action->DestinationPathId, error.empty() ? "is not in the zone" : error);
                        break;
                    }
                    PathMovementPosition const from = walker.Generator.GetPosition();
                    walker.Generator = std::move(*moved);
                    walker.PathId = destination->Id;
                    outcome.Teleported = outcome.Teleported || walker.Generator.GetPosition() != from;
                    return outcome;
                }
                case PathActionKind::TeleportToNode:
                    if (action->DestinationNodeId < 0 || !walker.Generator.JumpTo(static_cast<uint32>(action->DestinationNodeId)))
                    {
                        LOG_INFO("server.zones", "Object {} in instance {} of {} cannot jump to node {} of path {}", id, map.GetDynamicZoneId(), map.GetZonePath(),
                            action->DestinationNodeId, walker.PathId);
                        break;
                    }
                    outcome.Teleported = true;
                    outcome.Halt = !action->ContinuePathing;
                    return outcome;
            }
        }
        return outcome;
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
    behavior.Actions = PathActions::Read(*pathBehavior);
    behavior.PauseChance = static_cast<uint32>(std::clamp<int64>(WholeOf(*pathBehavior, "m_pauseChance").value_or(0), 0, 100));
    std::optional<float> const pause = RealOf(*pathBehavior, "m_timeToPause");
    behavior.PauseSeconds = pause && std::isfinite(*pause) && *pause > 0.0f ? *pause : 0.0f;
    return behavior;
}

bool PathWalkers::Start(Map& map, uint64 globalId, ZonePath const& path, std::size_t startNode, uint64 pathGeneration, PathWalkBehavior const& behavior, std::string& error)
{
    if (path.Nodes.size() < 2)
        return false;
    std::optional<PathMovementGenerator> generator = PathMovementGenerator::Create(RouteOf(path), startNode, behavior.Speed, behavior.Scale, behavior.Traversal, behavior.Direction, error);
    if (!generator)
        return false;
    map.GetWalkers().insert_or_assign(globalId, MapWalker{ path.Id, pathGeneration, std::move(*generator), behavior.Traversal, behavior.Direction, true, false, behavior.Actions,
        behavior.PauseChance, behavior.PauseSeconds, false });
    return true;
}

MapObjectChanges PathWalkers::Advance(Map& map, Map::Clock::time_point now, std::chrono::milliseconds sendInterval, PathLookup const& paths, uint64 pathGeneration,
    PathWalkHooks const& hooks)
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
        bool const stopAtNodes = walker.Actions || walker.PauseChance > 0;
        std::chrono::duration<double> left = elapsed;
        std::optional<PathMovementStep> step;
        PathMovementPosition walkedFrom = before;
        PathMovementPosition legStart = before;
        bool teleported = false;
        bool despawn = false;
        bool halt = false;
        std::string error;
        for (std::size_t arrivals = 0; arrivals <= PathMovementGenerator::MaxTransitionsPerUpdate; ++arrivals)
        {
            legStart = walker.Generator.GetPosition();
            step = walker.Generator.Advance(left, error, stopAtNodes);
            if (step && step->Position != legStart)
                walkedFrom = legStart;
            if (!step || !step->Arrived)
                break;
            NodeOutcome const outcome = RunNode(map, id, walker, *step->Arrived, paths, hooks, changes);
            teleported = teleported || outcome.Teleported;
            despawn = outcome.Despawn;
            halt = outcome.Halt;
            if (outcome.Teleported)
                walkedFrom = walker.Generator.GetPosition();
            if (despawn || halt)
                break;
            left = step->Leftover;
            step = walker.Generator.Advance(std::chrono::duration<double>(0.0), error, false);
            if (!step || left.count() <= 0.0)
                break;
        }
        if (!step)
        {
            LOG_WARN("server.zones", "Object {} in instance {} of {} stops walking path {}: {}", id, map.GetDynamicZoneId(), map.GetZonePath(), walker.PathId, error);
            stopped.push_back(id);
            continue;
        }
        if (despawn)
        {
            stopped.push_back(id);
            if (hooks.Despawned)
                hooks.Despawned->push_back(id);
            continue;
        }
        PathMovementPosition const after = walker.Generator.GetPosition();
        float yaw = object->Spawn.Orientation.Z;
        double const dx = after.X - walkedFrom.X;
        double const dy = after.Y - walkedFrom.Y;
        if (dx != 0.0 || dy != 0.0)
            yaw = static_cast<float>(std::atan2(-dx, -dy));
        map.MoveObject(id, PointOf(after), yaw);
        bool const moving = !halt && step->Moving;
        if (walker.Emoting && moving)
        {
            changes.States.push_back({ id, IdleState, {} });
            walker.Emoting = false;
        }
        std::optional<int8> state;
        if (moving != walker.Moving || !walker.Shown)
            state = moving ? WalkingState : StandingState;
        if (teleported || halt || (send && (after != before || state)))
        {
            changes.Moved.push_back({ id, object->MobileId, PointOf(after), yaw, state, teleported });
            walker.Moving = moving;
            walker.Shown = true;
        }
        if (halt)
            stopped.push_back(id);
    }
    for (uint64 const id : stopped)
        map.GetWalkers().erase(id);
    return changes;
}

MapObjectChanges PathWalkers::AdvanceFromWorld(Map& map, Map::Clock::time_point now, std::function<uint32(uint32)> roll, std::vector<uint64>& despawned)
{
    std::shared_ptr<ZonePaths const> const paths = sZonePathMgr.Get();
    std::string const zone = map.GetZonePath();
    PathLookup const lookup = [&paths, &zone](uint64 id) { return paths ? paths->Find(zone, id) : nullptr; };
    PathWalkHooks hooks;
    hooks.Roll = std::move(roll);
    hooks.Emote = WorldEmoteEncoder();
    hooks.Despawned = &despawned;
    return Advance(map, now, std::chrono::milliseconds(sSettings.Get<uint32>("Zone.MoveFlushInterval")), lookup, sZonePathMgr.GetGeneration(), hooks);
}

void PathWalkers::SetEmoteEncoder(PathEmoteEncoder encoder)
{
    WorldEmoteEncoder() = std::move(encoder);
}
