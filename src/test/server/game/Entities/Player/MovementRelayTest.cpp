/*
 * Project Ambrose by Imjustchico
 * Tests movement relay updates, idle standing, immediate link-dead standing, a newcomer's view of the last move and state, and the flush clock's live interval.
 */

#include "MoveFlushClock.h"
#include "MovementRelay.h"
#include "PlayerMovement.h"

#include <gtest/gtest.h>

#include <chrono>

namespace
{
    PlayerMovement Arrived()
    {
        PlayerMovement movement;
        movement.Reset({ 1.0f, 2.0f, 3.0f, 0.0f }, 0);
        return movement;
    }
}

TEST(MovementRelayTest, AMoveGoesOutOnceAsTheClientPackedItAndTheSameMoveAgainIsNoChange)
{
    PlayerMovement movement = Arrived();
    MovementRelay relay;
    relay.Reset(movement);
    EXPECT_TRUE(relay.Take(movement, 2).Empty()) << "a wizard that has not moved has nothing to show";

    movement.Apply(0xFF81, 0x0010, 0xFFF8, 40, 0);
    MovementUpdate const first = relay.Take(movement, 2);
    ASSERT_TRUE(first.Move);
    EXPECT_EQ(*first.Move, (PackedMove{ 0xFF81, 0x0010, 0xFFF8, 40 }));
    EXPECT_FALSE(first.State);
    EXPECT_TRUE(relay.Take(movement, 2).Empty());

    movement.Apply(0xFF81, 0x0010, 0xFFF8, 40, 0);
    EXPECT_FALSE(relay.Take(movement, 2).Move) << "a move that repeats the last one changes nothing";
    movement.Apply(0xFF81, 0x0011, 0xFFF8, 40, 0);
    EXPECT_TRUE(relay.Take(movement, 2).Move);
}

TEST(MovementRelayTest, AStateGoesOutWhenItChangesAndAQuietMoverIsToldAsStandingOnce)
{
    PlayerMovement movement = Arrived();
    MovementRelay relay;
    relay.Reset(movement);
    movement.SetMoveState(1);
    movement.Apply(10, 20, 30, 0, 0);
    MovementUpdate const start = relay.Take(movement, 2);
    EXPECT_TRUE(start.Move);
    EXPECT_EQ(start.State, std::optional<int8>(1));

    EXPECT_TRUE(relay.Take(movement, 2).Empty()) << "one quiet flush is not yet standing";
    MovementUpdate const stopped = relay.Take(movement, 2);
    EXPECT_FALSE(stopped.Move);
    EXPECT_EQ(stopped.State, std::optional<int8>(MovementRelay::Standing)) << "two quiet flushes are";
    EXPECT_EQ(movement.GetMoveState(), 1) << "what the client said is left as it said it";
    EXPECT_TRUE(relay.Take(movement, 2).Empty()) << "standing is said once";
    EXPECT_FALSE(relay.Current(movement).State) << "a newcomer sees the idle wizard standing too";

    movement.Apply(11, 20, 30, 0, 0);
    MovementUpdate const again = relay.Take(movement, 2);
    EXPECT_TRUE(again.Move);
    EXPECT_EQ(again.State, std::optional<int8>(1)) << "a move after the idle shows the client's own state again";

    relay.Take(movement, 2);
    relay.Take(movement, 2);
    movement.SetMoveState(2);
    EXPECT_EQ(relay.Take(movement, 2).State, std::optional<int8>(2)) << "so does a new state from the client";

    movement.SetMoveState(MovementRelay::Standing);
    EXPECT_EQ(relay.Take(movement, 2).State, std::optional<int8>(MovementRelay::Standing)) << "a client that says it stopped is told at once";
}

TEST(MovementRelayTest, TheIdleCountAppliesFromTheNextFlush)
{
    PlayerMovement movement = Arrived();
    MovementRelay relay;
    relay.Reset(movement);
    movement.SetMoveState(1);
    movement.Apply(10, 20, 30, 0, 0);
    relay.Take(movement, 5);
    EXPECT_TRUE(relay.Take(movement, 5).Empty());
    EXPECT_EQ(relay.Take(movement, 1).State, std::optional<int8>(MovementRelay::Standing)) << "a count lowered between flushes counts the quiet flushes already seen";
}

TEST(MovementRelayTest, ANewcomerIsShownTheLastMoveAndAStateOnlyWhileMoving)
{
    PlayerMovement movement = Arrived();
    MovementRelay relay;
    relay.Reset(movement);
    EXPECT_TRUE(relay.Current(movement).Empty()) << "a wizard that never moved stands where its object says";
    movement.Apply(7, 8, 9, 3, 0);
    MovementUpdate standing = relay.Current(movement);
    EXPECT_EQ(standing.Move, std::optional<PackedMove>(PackedMove{ 7, 8, 9, 3 }));
    EXPECT_FALSE(standing.State);
    movement.SetMoveState(1);
    EXPECT_EQ(relay.Current(movement).State, std::optional<int8>(1));
}

TEST(MovementRelayTest, ALinkDeadWizardIsShownStandingImmediately)
{
    PlayerMovement movement = Arrived();
    MovementRelay relay;
    relay.Reset(movement);
    movement.SetMoveState(1);
    movement.Apply(10, 20, 30, 0, 0);
    relay.Take(movement, 2);

    MovementUpdate const stopped = relay.Stop();
    EXPECT_EQ(stopped.State, std::optional<int8>(MovementRelay::Standing));
    EXPECT_EQ(movement.GetMoveState(), 1) << "a link-dead stop does not change what the client last reported";
    EXPECT_FALSE(relay.Current(movement).State);
    EXPECT_TRUE(relay.Take(movement, 2).Empty());
}

TEST(MovementRelayTest, TheFlushClockTakesAChangedIntervalFromTheNextFlush)
{
    using std::chrono::milliseconds;
    MoveFlushClock clock;
    EXPECT_FALSE(clock.Advance(milliseconds(100), milliseconds(250)));
    EXPECT_FALSE(clock.Advance(milliseconds(100), milliseconds(250)));
    EXPECT_TRUE(clock.Advance(milliseconds(100), milliseconds(250)));
    EXPECT_FALSE(clock.Advance(milliseconds(100), milliseconds(1000)));
    EXPECT_TRUE(clock.Advance(milliseconds(100), milliseconds(150))) << "an interval lowered between ticks is used at the next tick";
    EXPECT_TRUE(clock.Advance(milliseconds(50), milliseconds(50)));
    clock.Advance(milliseconds(40), milliseconds(50));
    clock.Reset();
    EXPECT_FALSE(clock.Advance(milliseconds(40), milliseconds(50))) << "a reset forgets the time already added up";
}
