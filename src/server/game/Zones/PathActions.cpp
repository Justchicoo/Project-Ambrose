/*
 * Project Ambrose by Imjustchico
 * Reads a PathBehaviorTemplate's m_actionList into the path actions a walker runs, skipping action classes the server does not run, and finds those keyed to a node in priority order.
 */

#include "PathActions.h"
#include "PropertyObject.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace
{
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

    float RealOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        float const* const held = value ? value->GetIf<float>() : nullptr;
        return held ? *held : 0.0f;
    }

    std::string TextOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        std::string const* const held = value ? value->GetIf<std::string>() : nullptr;
        return held ? *held : std::string();
    }

    std::optional<PathActionKind> KindOf(std::string_view className)
    {
        if (className == PathActions::PauseClass) return PathActionKind::Pause;
        if (className == PathActions::EmoteClass) return PathActionKind::Emote;
        if (className == PathActions::DespawnClass) return PathActionKind::Despawn;
        if (className == PathActions::ZoneEventClass) return PathActionKind::ZoneEvent;
        if (className == PathActions::ChangeDirectionClass) return PathActionKind::ChangeDirection;
        if (className == PathActions::ChangePathClass) return PathActionKind::ChangePath;
        if (className == PathActions::TeleportToNodeClass) return PathActionKind::TeleportToNode;
        return std::nullopt;
    }
}

PathActions::PathActions(std::vector<PathAction> actions) : _actions(std::move(actions))
{
    std::stable_sort(_actions.begin(), _actions.end(), [](PathAction const& left, PathAction const& right) { return left.Priority > right.Priority; });
}

std::shared_ptr<PathActions const> PathActions::Read(PropertyObject const& pathBehavior)
{
    PropertyValue const* const list = pathBehavior.Get("m_actionList");
    PropertyValue::List const* const entries = list ? list->GetList() : nullptr;
    if (!entries || entries->empty())
        return nullptr;
    std::vector<PathAction> actions;
    for (PropertyValue const& entry : *entries)
    {
        PropertyObject const* const object = entry.AsObject();
        std::optional<PathActionKind> const kind = object ? KindOf(object->GetClass().Name) : std::nullopt;
        if (!kind)
            continue;
        PathAction action;
        action.Kind = *kind;
        action.PathId = static_cast<uint64>(WholeOf(*object, "m_nPathID").value_or(0));
        action.NodeId = WholeOf(*object, "m_nNodeID").value_or(0);
        action.Priority = static_cast<int32>(WholeOf(*object, "m_nPriority").value_or(0));
        action.Chance = static_cast<int32>(WholeOf(*object, "m_nChance").value_or(100));
        switch (*kind)
        {
            case PathActionKind::Pause:
                action.MinSeconds = RealOf(*object, "m_fMinPauseTime");
                action.MaxSeconds = RealOf(*object, "m_fMaxPauseTime");
                break;
            case PathActionKind::Emote:
                action.Name = TextOf(*object, "m_sEmoteName");
                action.MinSeconds = RealOf(*object, "m_fMinTime");
                action.MaxSeconds = RealOf(*object, "m_fMaxTime");
                break;
            case PathActionKind::ZoneEvent:
                action.Name = TextOf(*object, "m_zoneEvent");
                break;
            case PathActionKind::ChangePath:
                action.DestinationPathId = static_cast<uint64>(WholeOf(*object, "m_nDestinationPathID").value_or(0));
                action.DestinationNodeId = WholeOf(*object, "m_nDestinationNodeID").value_or(0);
                break;
            case PathActionKind::TeleportToNode:
            {
                action.DestinationNodeId = WholeOf(*object, "m_nDestinationNodeID").value_or(0);
                PropertyValue const* const keepWalking = object->Get("m_bContinuePathing");
                bool const* const held = keepWalking ? keepWalking->GetIf<bool>() : nullptr;
                action.ContinuePathing = held ? *held : true;
                break;
            }
            case PathActionKind::Despawn:
            case PathActionKind::ChangeDirection:
                break;
        }
        actions.push_back(std::move(action));
    }
    if (actions.empty())
        return nullptr;
    return std::make_shared<PathActions const>(std::move(actions));
}

std::vector<PathAction const*> PathActions::At(uint64 pathId, uint64 nodeId) const
{
    std::vector<PathAction const*> found;
    for (PathAction const& action : _actions)
        if (action.PathId == pathId && action.NodeId >= 0 && static_cast<uint64>(action.NodeId) == nodeId)
            found.push_back(&action);
    return found;
}
