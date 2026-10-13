/*
 * Project Ambrose by Imjustchico
 * Tests the path walkers of a zone instance: a walker in an instance with no wizard is held still and nothing is sent, one with a wizard walks its path at its speed and is sent at most once a send interval with its walking state first, facing the way it walks as the client turns a wizard, a reload that moves its path's nodes carries it on from its next node on the new path, and one whose path is gone stops where it stands; and the path actions a walker runs on reaching a node: a pause and the template's chance to pause hold it there, an emote holds it in Emoting and sends it back to Idle as it walks on, a zone event is handed to the instance, a despawn is asked for, a turn sends it back the way it came, a path change carries it onto another path, a jump snaps it to another node and stands it there when it is not to walk on, and an action whose chance is not rolled does nothing.
 */

#include "Map.h"
#include "PathWalkers.h"
#include "ZonePathMgr.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    constexpr char const* Zone = "WizardCity/WC_Streets/WC_Unicorn";
    constexpr uint64 Ghost = 1001;
    constexpr uint64 GhostPath = 82136;

    ZonePath Straight(float length)
    {
        ZonePath path;
        path.Id = GhostPath;
        path.Name = "Path Ghost 01";
        path.Nodes.push_back({ 19, { 0.0f, 0.0f, 0.0f } });
        path.Nodes.push_back({ 20, { length, 0.0f, 0.0f } });
        return path;
    }

    void AddWalker(Map& map, ZonePath const& path, uint64 generation)
    {
        MapObject object;
        object.GlobalId = Ghost;
        object.MobileId = 40000;
        object.Origin = MapObjectOrigin::Spawner;
        object.Spawn.Position = path.Nodes.front().Position;
        map.AddObject(object);
        std::string error;
        ASSERT_TRUE(PathWalkers::Start(map, Ghost, path, 0, generation, PathWalkBehavior{ 100.0f, 1.0f }, error)) << error;
    }

    PathLookup Holding(ZonePath const* path)
    {
        return [path](uint64 id) { return path && path->Id == id ? path : nullptr; };
    }
}

TEST(PathWalkersTest, AnInstanceWithNoWizardHoldsItsWalkersStillAndSendsNothing)
{
    Map map(1, Zone, true);
    ZonePath const path = Straight(1000.0f);
    AddWalker(map, path, 1);
    Map::Clock::time_point const start{};
    for (int pass = 0; pass <= 20; ++pass)
    {
        MapObjectChanges const changes = PathWalkers::Advance(map, start + pass * 1s, 100ms, Holding(&path), 1);
        EXPECT_TRUE(changes.Moved.empty()) << "pass " << pass;
        EXPECT_FALSE(changes.Changed());
    }
    EXPECT_EQ(map.FindObject(Ghost)->Spawn.Position, path.Nodes.front().Position) << "nobody is there to see it walk";
}

TEST(PathWalkersTest, AWalkerWalksItsPathAtItsSpeedAndIsSentAtMostOnceASendInterval)
{
    Map map(1, Zone, true);
    ZonePath const path = Straight(1000.0f);
    AddWalker(map, path, 1);
    Map::Clock::time_point const start{};
    ASSERT_TRUE(map.AddPlayer(7, start));
    MapObjectChanges const first = PathWalkers::Advance(map, start, 100ms, Holding(&path), 1);
    ASSERT_EQ(first.Moved.size(), 1u);
    EXPECT_EQ(first.Moved.front().State, std::optional<int8>(PathWalkers::WalkingState)) << "the first send says it walks";
    EXPECT_EQ(first.Moved.front().MobileId, 40000);

    EXPECT_TRUE(PathWalkers::Advance(map, start + 50ms, 100ms, Holding(&path), 1).Moved.empty()) << "nothing is sent before the interval has passed";
    MapObjectChanges const later = PathWalkers::Advance(map, start + 2s, 100ms, Holding(&path), 1);
    ASSERT_EQ(later.Moved.size(), 1u);
    EXPECT_NEAR(later.Moved.front().Position.X, 200.0f, 0.01f) << "100 units a second for two seconds";
    EXPECT_FALSE(later.Moved.front().State) << "a walker still walking says no new state";
    EXPECT_NEAR(map.FindObject(Ghost)->Spawn.Position.X, 200.0f, 0.01f) << "the instance's object stands where the walker is";
    EXPECT_NEAR(later.Moved.front().Yaw, -std::numbers::pi_v<float> / 2.0f, 0.001f) << "walking along +x it faces a quarter turn clockwise of the client's zero";
}

TEST(PathWalkersTest, AWalkerFacesTheWayItWalksAsTheClientTurnsAWizard)
{
    Map map(1, Zone, true);
    ZonePath path = Straight(1000.0f);
    path.Nodes[1].Position = { 0.0f, 1000.0f, 0.0f };
    AddWalker(map, path, 1);
    Map::Clock::time_point const start{};
    ASSERT_TRUE(map.AddPlayer(7, start));
    PathWalkers::Advance(map, start, 100ms, Holding(&path), 1);
    MapObjectChanges const later = PathWalkers::Advance(map, start + 1s, 100ms, Holding(&path), 1);
    ASSERT_EQ(later.Moved.size(), 1u);
    EXPECT_NEAR(std::abs(later.Moved.front().Yaw), std::numbers::pi_v<float>, 0.001f) << "walking along +y it faces a half turn, as a wizard placed at yaw pi faces up the street";
}

TEST(PathWalkersTest, AReloadedPathCarriesAWalkerOnFromItsNextNodeAndAGonePathStopsIt)
{
    Map map(1, Zone, true);
    ZonePath const path = Straight(1000.0f);
    AddWalker(map, path, 1);
    Map::Clock::time_point const start{};
    ASSERT_TRUE(map.AddPlayer(7, start));
    PathWalkers::Advance(map, start, 100ms, Holding(&path), 1);
    PathWalkers::Advance(map, start + 1s, 100ms, Holding(&path), 1);

    ZonePath moved = path;
    moved.Nodes[1].Position = { 0.0f, 500.0f, 0.0f };
    MapObjectChanges const rerouted = PathWalkers::Advance(map, start + 1s + 200ms, 100ms, Holding(&moved), 2);
    ASSERT_EQ(rerouted.Moved.size(), 1u);
    EXPECT_NEAR(rerouted.Moved.front().Position.Y, 500.0f, 20.1f) << "it carries on from its next node on the new path";
    EXPECT_NEAR(rerouted.Moved.front().Position.X, 0.0f, 20.1f);
    EXPECT_EQ(map.GetWalkers().at(Ghost).PathGeneration, 2u);

    MapObjectChanges const gone = PathWalkers::Advance(map, start + 2s, 100ms, Holding(nullptr), 3);
    ASSERT_EQ(gone.Moved.size(), 1u);
    EXPECT_EQ(gone.Moved.front().State, std::optional<int8>(PathWalkers::StandingState)) << "a walker whose path is gone stands still";
    EXPECT_TRUE(map.GetWalkers().empty());
    ASSERT_NE(map.FindObject(Ghost), nullptr) << "it stays where it stands";
}

TEST(PathWalkersTest, AnObjectTakenAwayStopsWalking)
{
    Map map(1, Zone, true);
    ZonePath const path = Straight(1000.0f);
    AddWalker(map, path, 1);
    ASSERT_TRUE(map.RemoveObject(Ghost, Map::Clock::time_point{}, 0ms));
    EXPECT_TRUE(map.GetWalkers().empty());
}

namespace
{
    constexpr uint64 Second = 82137;

    ZonePath ThreeNodes()
    {
        ZonePath path;
        path.Id = GhostPath;
        path.Name = "Path Ghost 01";
        path.Nodes.push_back({ 19, { 0.0f, 0.0f, 0.0f } });
        path.Nodes.push_back({ 20, { 100.0f, 0.0f, 0.0f } });
        path.Nodes.push_back({ 21, { 200.0f, 0.0f, 0.0f } });
        return path;
    }

    ZonePath Crossing()
    {
        ZonePath path;
        path.Id = Second;
        path.Name = "Path Ghost 02";
        path.Nodes.push_back({ 30, { 100.0f, 0.0f, 0.0f } });
        path.Nodes.push_back({ 31, { 100.0f, 300.0f, 0.0f } });
        return path;
    }

    PathAction At20(PathActionKind kind, int32 chance = 100)
    {
        PathAction action;
        action.Kind = kind;
        action.PathId = GhostPath;
        action.NodeId = 20;
        action.Priority = 100;
        action.Chance = chance;
        return action;
    }

    void AddActing(Map& map, ZonePath const& path, std::vector<PathAction> actions, uint32 pauseChance = 0, float pauseSeconds = 0.0f)
    {
        MapObject object;
        object.GlobalId = Ghost;
        object.MobileId = 40000;
        object.Origin = MapObjectOrigin::Spawner;
        object.Spawn.Position = path.Nodes.front().Position;
        map.AddObject(object);
        PathWalkBehavior behavior{ 100.0f, 1.0f };
        behavior.Actions = std::make_shared<PathActions const>(std::move(actions));
        behavior.PauseChance = pauseChance;
        behavior.PauseSeconds = pauseSeconds;
        std::string error;
        ASSERT_TRUE(PathWalkers::Start(map, Ghost, path, 0, 1, behavior, error)) << error;
    }

    PathLookup Both(ZonePath const& first, ZonePath const& second)
    {
        return [&first, &second](uint64 id) { return id == first.Id ? &first : id == second.Id ? &second : nullptr; };
    }

    PathWalkHooks Rolling(uint32 roll, std::vector<uint64>* despawned = nullptr)
    {
        PathWalkHooks hooks;
        hooks.Roll = [roll](uint32 total) { return std::min(roll, total - 1); };
        hooks.Emote = [](std::string_view animation) { return animation == "Jump" ? std::optional<std::string>("jump-override") : std::nullopt; };
        hooks.Despawned = despawned;
        return hooks;
    }

    void Begin(Map& map, ZonePath const& path, PathWalkHooks const& hooks)
    {
        ASSERT_TRUE(map.AddPlayer(7, Map::Clock::time_point{}));
        PathWalkers::Advance(map, Map::Clock::time_point{}, 100ms, Holding(&path), 1, hooks);
    }
}

TEST(PathWalkersTest, APauseHoldsAWalkerAtItsNodeAndThenItWalksOn)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    PathAction pause = At20(PathActionKind::Pause);
    pause.MinSeconds = 2.0f;
    pause.MaxSeconds = 2.0f;
    AddActing(map, path, { pause });
    PathWalkHooks const hooks = Rolling(0);
    Begin(map, path, hooks);
    Map::Clock::time_point const start{};

    MapObjectChanges const arrived = PathWalkers::Advance(map, start + 1500ms, 100ms, Holding(&path), 1, hooks);
    ASSERT_EQ(arrived.Moved.size(), 1u);
    EXPECT_NEAR(arrived.Moved.front().Position.X, 100.0f, 0.01f) << "it stops on the node it paused at";
    EXPECT_EQ(arrived.Moved.front().State, std::optional<int8>(PathWalkers::StandingState));

    MapObjectChanges const waiting = PathWalkers::Advance(map, start + 2500ms, 100ms, Holding(&path), 1, hooks);
    EXPECT_TRUE(waiting.Moved.empty()) << "a walker holding still is not sent again";
    MapObjectChanges const walking = PathWalkers::Advance(map, start + 3500ms, 100ms, Holding(&path), 1, hooks);
    ASSERT_EQ(walking.Moved.size(), 1u);
    EXPECT_NEAR(walking.Moved.front().Position.X, 150.0f, 0.01f) << "it walks on once two seconds have passed";
    EXPECT_EQ(walking.Moved.front().State, std::optional<int8>(PathWalkers::WalkingState));
}

TEST(PathWalkersTest, TheTemplatesChanceToPauseHoldsAWalkerOnAnyNode)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    AddActing(map, path, {}, 10, 6.0f);
    PathWalkHooks const hooks = Rolling(9);
    Begin(map, path, hooks);
    MapObjectChanges const held = PathWalkers::Advance(map, Map::Clock::time_point{} + 4s, 100ms, Holding(&path), 1, hooks);
    ASSERT_EQ(held.Moved.size(), 1u);
    EXPECT_NEAR(held.Moved.front().Position.X, 100.0f, 0.01f) << "a roll under the 10 percent chance holds it six seconds";
}

TEST(PathWalkersTest, AnEmotePlaysInEmotingAndEndsInIdleAsTheWalkerWalksOn)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    PathAction emote = At20(PathActionKind::Emote);
    emote.Name = "Jump";
    emote.MinSeconds = 1.0f;
    emote.MaxSeconds = 1.0f;
    AddActing(map, path, { emote });
    PathWalkHooks const hooks = Rolling(0);
    Begin(map, path, hooks);
    Map::Clock::time_point const start{};

    MapObjectChanges const playing = PathWalkers::Advance(map, start + 1s, 100ms, Holding(&path), 1, hooks);
    ASSERT_EQ(playing.States.size(), 1u);
    EXPECT_EQ(playing.States.front().GlobalId, Ghost);
    EXPECT_EQ(playing.States.front().State, PathWalkers::EmotingState);
    EXPECT_EQ(playing.States.front().Data, "jump-override");

    MapObjectChanges const done = PathWalkers::Advance(map, start + 2500ms, 100ms, Holding(&path), 1, hooks);
    ASSERT_EQ(done.States.size(), 1u);
    EXPECT_EQ(done.States.front().State, PathWalkers::IdleState);
    EXPECT_TRUE(done.States.front().Data.empty());
    ASSERT_EQ(done.Moved.size(), 1u);
    EXPECT_NEAR(done.Moved.front().Position.X, 150.0f, 0.01f);
}

TEST(PathWalkersTest, AnEmoteTheClientCannotPlayIsSkipped)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    PathAction emote = At20(PathActionKind::Emote);
    emote.Name = "NoSuchAnimation";
    emote.MinSeconds = 1.0f;
    emote.MaxSeconds = 1.0f;
    AddActing(map, path, { emote });
    PathWalkHooks const hooks = Rolling(0);
    Begin(map, path, hooks);
    MapObjectChanges const passed = PathWalkers::Advance(map, Map::Clock::time_point{} + 1500ms, 100ms, Holding(&path), 1, hooks);
    EXPECT_TRUE(passed.States.empty());
    ASSERT_EQ(passed.Moved.size(), 1u);
    EXPECT_NEAR(passed.Moved.front().Position.X, 150.0f, 0.01f) << "it walks straight on";
}

TEST(PathWalkersTest, AZoneEventGoesToTheInstanceOnlyWhenItsChanceIsRolled)
{
    PathAction event = At20(PathActionKind::ZoneEvent, 50);
    event.Name = "TEST1";
    {
        Map map(1, Zone, true);
        ZonePath const path = ThreeNodes();
        AddActing(map, path, { event });
        PathWalkHooks const hooks = Rolling(49);
        Begin(map, path, hooks);
        MapObjectChanges const posted = PathWalkers::Advance(map, Map::Clock::time_point{} + 1500ms, 100ms, Holding(&path), 1, hooks);
        ASSERT_EQ(posted.ZoneEvents.size(), 1u);
        EXPECT_EQ(posted.ZoneEvents.front(), "TEST1");
        EXPECT_EQ(posted.DynamicZoneId, 1u);
    }
    {
        Map map(1, Zone, true);
        ZonePath const path = ThreeNodes();
        AddActing(map, path, { event });
        PathWalkHooks const hooks = Rolling(50);
        Begin(map, path, hooks);
        EXPECT_TRUE(PathWalkers::Advance(map, Map::Clock::time_point{} + 1500ms, 100ms, Holding(&path), 1, hooks).ZoneEvents.empty()) << "a roll of 50 misses a 50 percent chance";
    }
}

TEST(PathWalkersTest, ADespawnIsAskedForAndTheWalkerStops)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    AddActing(map, path, { At20(PathActionKind::Despawn) });
    std::vector<uint64> despawned;
    PathWalkHooks const hooks = Rolling(0, &despawned);
    Begin(map, path, hooks);
    PathWalkers::Advance(map, Map::Clock::time_point{} + 1500ms, 100ms, Holding(&path), 1, hooks);
    EXPECT_EQ(despawned, std::vector<uint64>{ Ghost });
    EXPECT_TRUE(map.GetWalkers().empty());
}

TEST(PathWalkersTest, ATurnSendsAWalkerBackTheWayItCame)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    AddActing(map, path, { At20(PathActionKind::ChangeDirection) });
    PathWalkHooks const hooks = Rolling(0);
    Begin(map, path, hooks);
    MapObjectChanges const back = PathWalkers::Advance(map, Map::Clock::time_point{} + 1500ms, 100ms, Holding(&path), 1, hooks);
    ASSERT_EQ(back.Moved.size(), 1u);
    EXPECT_NEAR(back.Moved.front().Position.X, 50.0f, 0.01f) << "half a second back toward node 19";
    EXPECT_NEAR(back.Moved.front().Yaw, std::numbers::pi_v<float> / 2.0f, 0.001f) << "it faces the way it walks back";
}

TEST(PathWalkersTest, APathChangeCarriesAWalkerOntoAnotherPath)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    ZonePath const crossing = Crossing();
    PathAction change = At20(PathActionKind::ChangePath);
    change.DestinationPathId = Second;
    change.DestinationNodeId = 30;
    AddActing(map, path, { change });
    PathWalkHooks const hooks = Rolling(0);
    Begin(map, path, hooks);
    MapObjectChanges const turned = PathWalkers::Advance(map, Map::Clock::time_point{} + 1500ms, 100ms, Both(path, crossing), 1, hooks);
    ASSERT_EQ(turned.Moved.size(), 1u);
    EXPECT_NEAR(turned.Moved.front().Position.X, 100.0f, 0.01f);
    EXPECT_NEAR(turned.Moved.front().Position.Y, 50.0f, 0.01f) << "it walks up the crossing path from node 30";
    EXPECT_FALSE(turned.Moved.front().Teleport) << "the crossing starts where it stands";
    EXPECT_EQ(map.GetWalkers().at(Ghost).PathId, Second);
}

TEST(PathWalkersTest, AJumpSnapsAWalkerToAnotherNodeAndStandsItThereWhenItIsNotToWalkOn)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    PathAction jump = At20(PathActionKind::TeleportToNode);
    jump.DestinationNodeId = 19;
    jump.ContinuePathing = false;
    AddActing(map, path, { jump });
    PathWalkHooks const hooks = Rolling(0);
    Begin(map, path, hooks);
    MapObjectChanges const snapped = PathWalkers::Advance(map, Map::Clock::time_point{} + 1500ms, 100ms, Holding(&path), 1, hooks);
    ASSERT_EQ(snapped.Moved.size(), 1u);
    EXPECT_TRUE(snapped.Moved.front().Teleport);
    EXPECT_NEAR(snapped.Moved.front().Position.X, 0.0f, 0.01f);
    EXPECT_EQ(snapped.Moved.front().State, std::optional<int8>(PathWalkers::StandingState));
    EXPECT_TRUE(map.GetWalkers().empty());
    EXPECT_NEAR(map.FindObject(Ghost)->Spawn.Position.X, 0.0f, 0.01f);
}

TEST(PathWalkersTest, AJumpThatWalksOnCarriesOnFromTheNodeItSnappedTo)
{
    Map map(1, Zone, true);
    ZonePath const path = ThreeNodes();
    PathAction jump = At20(PathActionKind::TeleportToNode);
    jump.DestinationNodeId = 19;
    AddActing(map, path, { jump });
    PathWalkHooks const hooks = Rolling(0);
    Begin(map, path, hooks);
    MapObjectChanges const snapped = PathWalkers::Advance(map, Map::Clock::time_point{} + 1500ms, 100ms, Holding(&path), 1, hooks);
    ASSERT_EQ(snapped.Moved.size(), 1u);
    EXPECT_TRUE(snapped.Moved.front().Teleport);
    EXPECT_NEAR(snapped.Moved.front().Position.X, 50.0f, 0.01f) << "the half second left over walks it on from node 19";
    EXPECT_EQ(map.GetWalkers().size(), 1u);
}
