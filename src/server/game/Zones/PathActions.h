/*
 * Project Ambrose by Imjustchico
 * The path actions a PathBehaviorTemplate's m_actionList gives a walking object, each keyed to the path and node it fires on with its priority and percent chance: a pause, an emote held for a while, a despawn, a zone event, a turn back the way it came, a move onto another path and a jump to another node of its path, read from the template once and found by the node a walker reaches, highest priority first.
 */

#ifndef AMBROSE_PATHACTIONS_H
#define AMBROSE_PATHACTIONS_H

#include "Types.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class PropertyObject;

enum class PathActionKind : uint8
{
    Pause,
    Emote,
    Despawn,
    ZoneEvent,
    ChangeDirection,
    ChangePath,
    TeleportToNode
};

struct PathAction
{
    PathActionKind Kind = PathActionKind::Pause;
    uint64 PathId = 0;
    int64 NodeId = 0;
    int32 Priority = 0;
    int32 Chance = 100;
    float MinSeconds = 0.0f;
    float MaxSeconds = 0.0f;
    std::string Name;
    uint64 DestinationPathId = 0;
    int64 DestinationNodeId = 0;
    bool ContinuePathing = true;

    bool operator==(PathAction const&) const = default;
};

class PathActions
{
public:
    static constexpr std::string_view PauseClass = "class PauseAction";
    static constexpr std::string_view EmoteClass = "class PathActionEmote";
    static constexpr std::string_view DespawnClass = "class PathActionDespawn";
    static constexpr std::string_view ZoneEventClass = "class PathActionZoneEvent";
    static constexpr std::string_view ChangeDirectionClass = "class PathActionChangeDirection";
    static constexpr std::string_view ChangePathClass = "class PathActionChangePath";
    static constexpr std::string_view TeleportToNodeClass = "class TeleportToNode";

    explicit PathActions(std::vector<PathAction> actions);

    static std::shared_ptr<PathActions const> Read(PropertyObject const& pathBehavior);

    std::vector<PathAction const*> At(uint64 pathId, uint64 nodeId) const;

private:
    std::vector<PathAction> _actions;
};

#endif
