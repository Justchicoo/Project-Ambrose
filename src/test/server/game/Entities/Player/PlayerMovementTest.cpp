/*
 * Project Ambrose by Imjustchico
 * Tests where a wizard stands: a move under the session's zone counter places it by the client's own unpacking and a move under another counter is ignored, a thousand moves leave one write pending and nothing written before it is taken, the write is taken once, a wizard that never moved has nothing to write, and starting over forgets what came before.
 */

#include "MovementPacking.h"
#include "PlayerMovement.h"

#include <gtest/gtest.h>

#include <optional>

namespace
{
    uint16 Packed(float value)
    {
        return static_cast<uint16>(*MovementPacking::TryPackLocation(value));
    }
}

TEST(PlayerMovementTest, AMoveUnderTheSessionsZoneCounterPlacesTheWizardAndAStaleOneIsIgnored)
{
    PlayerMovement movement;
    movement.Reset({ -5.6f, -1531.4f, -30.5f, 1.5f }, 0);
    EXPECT_FALSE(movement.HasMoved());
    EXPECT_EQ(movement.Apply(Packed(-100.0f), Packed(-1600.0f), Packed(-32.0f), 60, 0), MoveResult::Moved);
    EXPECT_EQ(movement.GetPosition(), (PlayerPosition{ -100.0f, -1600.0f, -32.0f, 1.5f }));
    EXPECT_EQ(movement.Apply(0xFF81, 0xFE81, 0xFFF8, 40, 0), MoveResult::Moved) << "the shorts a client sends read back signed";
    EXPECT_EQ(movement.GetPosition(), (PlayerPosition{ -508.0f, -1532.0f, -32.0f, 1.0f }));

    EXPECT_EQ(movement.Apply(Packed(400.0f), Packed(400.0f), Packed(0.0f), 0, 1), MoveResult::StaleZone);
    EXPECT_EQ(movement.GetPosition(), (PlayerPosition{ -508.0f, -1532.0f, -32.0f, 1.0f })) << "a move from another zone counter leaves the wizard where it was";
    EXPECT_EQ(movement.GetMoves(), 2u);
    movement.SetZoneCounter(1);
    EXPECT_EQ(movement.Apply(Packed(400.0f), Packed(400.0f), Packed(0.0f), 0, 1), MoveResult::Moved);
    EXPECT_EQ(movement.Apply(Packed(0.0f), Packed(0.0f), Packed(0.0f), 0, 0), MoveResult::StaleZone);
    movement.SetMoveState(3);
    EXPECT_EQ(movement.GetMoveState(), 3);
}

TEST(PlayerMovementTest, AThousandMovesLeaveOneWriteThatIsTakenOnce)
{
    PlayerMovement movement;
    movement.Reset({ 0.0f, 0.0f, 0.0f, 0.0f }, 0);
    EXPECT_FALSE(movement.TakeWrite()) << "a wizard that never moved has nothing to write";
    for (int step = 1; step <= 1000; ++step)
        ASSERT_EQ(movement.Apply(Packed(static_cast<float>(step * 4)), Packed(8.0f), Packed(0.0f), static_cast<uint8>(step % 250), 0), MoveResult::Moved);
    EXPECT_EQ(movement.GetMoves(), 1000u);
    EXPECT_TRUE(movement.HasMoved());
    std::optional<PlayerPosition> const written = movement.TakeWrite();
    ASSERT_TRUE(written);
    EXPECT_EQ(*written, (PlayerPosition{ 4000.0f, 8.0f, 0.0f, MovementPacking::UnpackYaw(1000 % 250) }));
    EXPECT_FALSE(movement.TakeWrite()) << "the thousand moves are one write, taken once";
    EXPECT_FALSE(movement.HasMoved());

    movement.Reset({ 1.0f, 2.0f, 3.0f, 0.5f }, 7);
    EXPECT_EQ(movement.GetMoves(), 0u);
    EXPECT_EQ(movement.GetZoneCounter(), 7);
    EXPECT_EQ(movement.GetMoveState(), 0);
    EXPECT_FALSE(movement.TakeWrite());
}
