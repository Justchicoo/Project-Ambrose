/*
 * Project Ambrose by Imjustchico
 * The spawned objects that walk their path in a zone instance: an object whose template holds both a PathBehaviorTemplate and a PathMovementBehaviorTemplate walks the path its spawn stands on, from the node it was placed on, at m_movementSpeed times m_movementScale, around the path for PT_LOOP and there and back for PT_CHAIN, in the direction m_nPathDirection gives; each pass moves every walker by the time since the last pass while a wizard is in the instance and holds them all still while none is, and hands back each walker's place and facing at most once a send interval, with its move state when it starts or stops, so only the wizards who can see it are told. On each node it reaches it runs the path actions its template keys to that node, each on its percent chance, and the template's chance to pause: it pauses or plays an emote for a while in the NPC Emoting state and goes back to Idle when it walks on, posts a zone event to every wizard in its instance, turns back, moves onto another path, snaps to another node and walks on or stands there, or asks to be despawned. When the paths are reloaded a walker carries on from its next node on its path's new nodes, and stops where it stands when its path is gone.
 */

#ifndef AMBROSE_PATHWALKERS_H
#define AMBROSE_PATHWALKERS_H

#include "Map.h"
#include "MapObjectSpawner.h"
#include "PathActions.h"
#include "PathMovementGenerator.h"
#include "StringHash.h"
#include "Types.h"
#include "ZonePathMgr.h"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct PathWalkBehavior
{
    float Speed = 0.0f;
    float Scale = 1.0f;
    PathTraversalMode Traversal = PathTraversalMode::Loop;
    PathInitialDirection Direction = PathInitialDirection::Forward;
    std::shared_ptr<PathActions const> Actions = {};
    uint32 PauseChance = 0;
    float PauseSeconds = 0.0f;
};

using PathLookup = std::function<ZonePath const*(uint64)>;
using PathEmoteEncoder = std::function<std::optional<std::string>(std::string_view)>;

struct PathWalkHooks
{
    std::function<uint32(uint32)> Roll = {};
    PathEmoteEncoder Emote = {};
    std::vector<uint64>* Despawned = nullptr;
};

class PathWalkers
{
public:
    static constexpr std::string_view PathBehaviorClass = "class PathBehaviorTemplate";
    static constexpr std::string_view MovementBehaviorClass = "class PathMovementBehaviorTemplate";
    static constexpr int64 PathTypeLoop = 0;
    static constexpr int64 PathTypeChain = 1;
    static constexpr int8 StandingState = 0;
    static constexpr int8 WalkingState = 1;
    static constexpr std::string_view EmotingName = "Emoting";
    static constexpr uint32 EmotingState = StringHash::StringId(EmotingName);
    static constexpr std::string_view IdleName = "Idle";
    static constexpr uint32 IdleState = StringHash::StringId(IdleName);

    PathWalkers() = delete;

    static std::optional<PathWalkBehavior> BehaviorOf(ObjectTemplate const& objectTemplate);
    static bool Start(Map& map, uint64 globalId, ZonePath const& path, std::size_t startNode, uint64 pathGeneration, PathWalkBehavior const& behavior, std::string& error);
    static MapObjectChanges Advance(Map& map, Map::Clock::time_point now, std::chrono::milliseconds sendInterval, PathLookup const& paths, uint64 pathGeneration,
        PathWalkHooks const& hooks = {});
    static MapObjectChanges AdvanceFromWorld(Map& map, Map::Clock::time_point now, std::function<uint32(uint32)> roll, std::vector<uint64>& despawned);
    static void SetEmoteEncoder(PathEmoteEncoder encoder);
};

#endif
