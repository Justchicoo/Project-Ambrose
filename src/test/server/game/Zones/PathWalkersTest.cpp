/*
 * Project Ambrose by Imjustchico
 * Tests the path walkers of a zone instance: a walker in an instance with no wizard is held still and nothing is sent, one with a wizard walks its path at its speed and is sent at most once a send interval with its walking state first, a reload that moves its path's nodes carries it on from its next node on the new path, and one whose path is gone stops where it stands.
 */

#include "Map.h"
#include "PathWalkers.h"
#include "ZonePathMgr.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

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
    EXPECT_NEAR(later.Moved.front().Yaw, 0.0f, 0.001f) << "it faces the way it travels";
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
