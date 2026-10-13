/*
 * Project Ambrose by Imjustchico
 * Tests path validation, speed-scaled travel, loop and ping-pong endpoints, leftover tick time, node waits and a long tick folding whole route cycles, and stopping on each node reached with the time left over, holding, turning back and jumping to another node.
 */

#include "PathMovementGenerator.h"

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    std::vector<PathMovementNode> ThreeNodes(std::chrono::milliseconds wait = 0ms)
    {
        return {
            { 10, { 0.0f, 0.0f, 0.0f }, 0ms },
            { 20, { 10.0f, 0.0f, 0.0f }, wait },
            { 30, { 20.0f, 0.0f, 0.0f }, 0ms }
        };
    }

    PathMovementGenerator MakeGenerator(std::vector<PathMovementNode> nodes, float speed, float scale, PathTraversalMode traversal,
        PathInitialDirection direction = PathInitialDirection::Forward, std::size_t start = 0)
    {
        std::string error;
        std::optional<PathMovementGenerator> generator = PathMovementGenerator::Create(std::move(nodes), start, speed, scale, traversal, direction, error);
        EXPECT_TRUE(generator) << error;
        return std::move(generator).value();
    }
}

TEST(PathMovementGeneratorTest, TwoNodePathCoversDistanceAtConfiguredSpeedWithinOneTick)
{
    std::string error;
    std::vector<PathMovementNode> const nodes{
        { 1, { 0.0f, 0.0f, 0.0f }, 0ms },
        { 2, { 50.0f, 0.0f, 0.0f }, 0ms }
    };
    std::optional<PathMovementGenerator> generator = PathMovementGenerator::Create(nodes, 0, 5.0f, 2.0f, PathTraversalMode::Loop,
        PathInitialDirection::Forward, error);
    ASSERT_TRUE(generator) << error;

    constexpr uint32 expectedTicks = 250;
    uint32 ticks = 0;
    while (generator->GetCurrentNodeId() != 2u && ticks < expectedTicks + 2)
    {
        ASSERT_TRUE(generator->Advance(20ms, error)) << error;
        ++ticks;
    }
    EXPECT_NEAR(ticks, expectedTicks, 1u);
    EXPECT_DOUBLE_EQ(generator->GetPosition().X, 50.0);
    EXPECT_EQ(generator->GetCurrentNodeId(), 2u);
}

TEST(PathMovementGeneratorTest, LoopWrapsFromLastNodeToFirst)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(), 10.0f, 1.0f, PathTraversalMode::Loop);
    std::string error;

    ASSERT_TRUE(generator.Advance(2s, error));
    PathMovementStep const step = *generator.Advance(500ms, error);
    EXPECT_TRUE(error.empty());
    EXPECT_DOUBLE_EQ(step.Position.X, 15.0);
    EXPECT_EQ(step.CurrentNodeId, 30u);
    EXPECT_EQ(step.NextNodeId, 10u);
}

TEST(PathMovementGeneratorTest, PingPongReversesAtTheLastNode)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(), 10.0f, 1.0f, PathTraversalMode::PingPong);
    std::string error;

    ASSERT_TRUE(generator.Advance(2s, error));
    PathMovementStep const step = *generator.Advance(500ms, error);
    EXPECT_TRUE(error.empty());
    EXPECT_DOUBLE_EQ(step.Position.X, 15.0);
    EXPECT_EQ(step.CurrentNodeId, 30u);
    EXPECT_EQ(step.NextNodeId, 20u);
}

TEST(PathMovementGeneratorTest, WaitAtANodeUsesElapsedTimeBeforeMovingOn)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(1s), 10.0f, 1.0f, PathTraversalMode::Loop);
    std::string error;

    PathMovementStep const arrived = *generator.Advance(1s, error);
    ASSERT_TRUE(error.empty());
    EXPECT_DOUBLE_EQ(arrived.Position.X, 10.0);
    EXPECT_FALSE(arrived.Moving);

    PathMovementStep const waiting = *generator.Advance(500ms, error);
    ASSERT_TRUE(error.empty());
    EXPECT_DOUBLE_EQ(waiting.Position.X, 10.0);
    EXPECT_FALSE(waiting.Moving);

    PathMovementStep const left = *generator.Advance(750ms, error);
    EXPECT_TRUE(error.empty());
    EXPECT_NEAR(left.Position.X, 12.5, 0.0001);
    EXPECT_TRUE(left.Moving);
}

TEST(PathMovementGeneratorTest, ReverseStartTravelsTowardThePreviousNode)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(), 10.0f, 1.0f, PathTraversalMode::Loop, PathInitialDirection::Reverse, 2);
    std::string error;

    PathMovementStep const step = *generator.Advance(500ms, error);
    EXPECT_TRUE(error.empty());
    EXPECT_DOUBLE_EQ(step.Position.X, 15.0);
    EXPECT_EQ(step.CurrentNodeId, 30u);
    EXPECT_EQ(step.NextNodeId, 20u);
}

TEST(PathMovementGeneratorTest, RefusesInvalidRoutesAndNonFiniteValues)
{
    std::string error;
    EXPECT_FALSE(PathMovementGenerator::Create({}, 0, 1.0f, 1.0f, PathTraversalMode::Loop, PathInitialDirection::Forward, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(PathMovementGenerator::Create(ThreeNodes(), 3, 1.0f, 1.0f, PathTraversalMode::Loop, PathInitialDirection::Forward, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_FALSE(PathMovementGenerator::Create(ThreeNodes(), 0, std::numeric_limits<float>::infinity(), 1.0f,
        PathTraversalMode::Loop, PathInitialDirection::Forward, error));
    EXPECT_FALSE(error.empty());

    error.clear();
    std::vector<PathMovementNode> nonFiniteNodes = ThreeNodes();
    nonFiniteNodes[1].Position.X = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(PathMovementGenerator::Create(std::move(nonFiniteNodes), 0, 1.0f, 1.0f, PathTraversalMode::Loop,
        PathInitialDirection::Forward, error));
    EXPECT_FALSE(error.empty());
}

TEST(PathMovementGeneratorTest, LongTickFoldsWholeCyclesInsteadOfFailing)
{
    std::vector<PathMovementNode> const nodes{
        { 1, { 0.0f, 0.0f, 0.0f }, 0ms },
        { 2, { 0.0009765625f, 0.0f, 0.0f }, 0ms }
    };
    PathMovementGenerator generator = MakeGenerator(nodes, 1.0f, 1.0f, PathTraversalMode::PingPong);
    std::string error;

    std::optional<PathMovementStep> const step = generator.Advance(std::chrono::duration<double>(3600.00048828125), error);
    ASSERT_TRUE(step) << error;
    EXPECT_NEAR(step->Position.X, 0.00048828125, 0.000001);

    PathMovementGenerator coarse = MakeGenerator(ThreeNodes(250ms), 10.0f, 1.0f, PathTraversalMode::PingPong);
    PathMovementGenerator fine = MakeGenerator(ThreeNodes(250ms), 10.0f, 1.0f, PathTraversalMode::PingPong);
    ASSERT_TRUE(coarse.Advance(std::chrono::duration<double>(4.5 * 7 + 1.25), error)) << error;
    ASSERT_TRUE(fine.Advance(std::chrono::duration<double>(1.25), error)) << error;
    EXPECT_NEAR(coarse.GetPosition().X, fine.GetPosition().X, 0.0001);
    EXPECT_EQ(coarse.GetCurrentNodeId(), fine.GetCurrentNodeId());
    EXPECT_EQ(coarse.GetNextNodeId(), fine.GetNextNodeId());
}

TEST(PathMovementGeneratorTest, RefusesAMovingPathThatNeverLeavesOnePoint)
{
    std::string error;
    std::vector<PathMovementNode> const nodes{
        { 1, { 3.0f, 4.0f, 5.0f }, 0ms },
        { 2, { 3.0f, 4.0f, 5.0f }, 0ms }
    };
    EXPECT_FALSE(PathMovementGenerator::Create(nodes, 0, 1.0f, 1.0f, PathTraversalMode::Loop, PathInitialDirection::Forward, error));
    EXPECT_FALSE(error.empty());

    std::vector<PathMovementNode> waiting = nodes;
    waiting[1].Wait = 1s;
    EXPECT_TRUE(PathMovementGenerator::Create(waiting, 0, 1.0f, 1.0f, PathTraversalMode::Loop, PathInitialDirection::Forward, error)) << error;
}

TEST(PathMovementGeneratorTest, StopsOnEachNodeItReachesAndHandsBackTheTimeLeftOver)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(), 10.0f, 1.0f, PathTraversalMode::Loop);
    std::string error;

    std::optional<PathMovementStep> const first = generator.Advance(1500ms, error, true);
    ASSERT_TRUE(first) << error;
    EXPECT_EQ(first->Arrived, std::optional<uint32>(20u));
    EXPECT_NEAR(first->Leftover.count(), 0.5, 1e-9);
    EXPECT_DOUBLE_EQ(first->Position.X, 10.0);

    std::optional<PathMovementStep> const rest = generator.Advance(first->Leftover, error, true);
    ASSERT_TRUE(rest) << error;
    EXPECT_FALSE(rest->Arrived);
    EXPECT_DOUBLE_EQ(rest->Position.X, 15.0);
}

TEST(PathMovementGeneratorTest, HoldsTurnsBackAndJumpsToAnotherNode)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(), 10.0f, 1.0f, PathTraversalMode::Loop);
    std::string error;
    ASSERT_TRUE(generator.Advance(1s, error));
    EXPECT_EQ(generator.GetCurrentNodeId(), 20u);

    generator.Hold(2s);
    std::optional<PathMovementStep> const held = generator.Advance(1500ms, error);
    ASSERT_TRUE(held) << error;
    EXPECT_DOUBLE_EQ(held->Position.X, 10.0);
    EXPECT_FALSE(held->Moving);

    generator.Reverse();
    EXPECT_EQ(generator.GetDirection(), PathInitialDirection::Reverse);
    std::optional<PathMovementStep> const back = generator.Advance(1s, error);
    ASSERT_TRUE(back) << error;
    EXPECT_DOUBLE_EQ(back->Position.X, 5.0) << "half a second of hold is left, then half a second back toward node 10";
    EXPECT_EQ(back->NextNodeId, 10u);

    ASSERT_TRUE(generator.JumpTo(30));
    EXPECT_DOUBLE_EQ(generator.GetPosition().X, 20.0);
    EXPECT_EQ(generator.GetCurrentNodeId(), 30u);
    EXPECT_FALSE(generator.JumpTo(99)) << "a node the path does not hold is refused";
}
