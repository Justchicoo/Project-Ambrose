/*
 * Project Ambrose by Imjustchico
 * Tests path validation, speed-scaled travel, loop and ping-pong endpoints, leftover tick time and node waits.
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
        return std::move(*generator);
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
    EXPECT_FLOAT_EQ(generator->GetPosition().X, 50.0f);
}

TEST(PathMovementGeneratorTest, LoopWrapsFromLastNodeToFirst)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(), 10.0f, 1.0f, PathTraversalMode::Loop);
    std::string error;

    ASSERT_TRUE(generator.Advance(2s, error));
    PathMovementStep const step = *generator.Advance(500ms, error);
    EXPECT_TRUE(error.empty());
    EXPECT_FLOAT_EQ(step.Position.X, 15.0f);
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
    EXPECT_FLOAT_EQ(step.Position.X, 15.0f);
    EXPECT_EQ(step.CurrentNodeId, 30u);
    EXPECT_EQ(step.NextNodeId, 20u);
}

TEST(PathMovementGeneratorTest, WaitAtANodeUsesElapsedTimeBeforeMovingOn)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(1s), 10.0f, 1.0f, PathTraversalMode::Loop);
    std::string error;

    PathMovementStep const arrived = *generator.Advance(1s, error);
    ASSERT_TRUE(error.empty());
    EXPECT_FLOAT_EQ(arrived.Position.X, 10.0f);
    EXPECT_FALSE(arrived.Moving);

    PathMovementStep const waiting = *generator.Advance(500ms, error);
    ASSERT_TRUE(error.empty());
    EXPECT_FLOAT_EQ(waiting.Position.X, 10.0f);
    EXPECT_FALSE(waiting.Moving);

    PathMovementStep const left = *generator.Advance(750ms, error);
    EXPECT_TRUE(error.empty());
    EXPECT_NEAR(left.Position.X, 12.5f, 0.0001f);
    EXPECT_TRUE(left.Moving);
}

TEST(PathMovementGeneratorTest, ReverseStartTravelsTowardThePreviousNode)
{
    PathMovementGenerator generator = MakeGenerator(ThreeNodes(), 10.0f, 1.0f, PathTraversalMode::Loop, PathInitialDirection::Reverse, 2);
    std::string error;

    PathMovementStep const step = *generator.Advance(500ms, error);
    EXPECT_TRUE(error.empty());
    EXPECT_FLOAT_EQ(step.Position.X, 15.0f);
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
