/*
 * Project Ambrose by Imjustchico
 * Verifies that Go Home cooldowns use each press's current duration, begin on accepted use, and do not move forward on a refused press.
 */

#include "GoHomeCooldown.h"

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST(GoHomeCooldownTest, ReadsTheDurationAtEachPressAndOnlyRecordsAcceptedUses)
{
    GoHomeCooldown cooldown;
    GoHomeCooldown::Clock::time_point const start{};

    EXPECT_TRUE(cooldown.TryUse(start, 30s));
    EXPECT_FALSE(cooldown.TryUse(start + 4s, 5s));
    EXPECT_TRUE(cooldown.TryUse(start + 6s, 5s));
    EXPECT_FALSE(cooldown.TryUse(start + 10s, 30s));
    EXPECT_TRUE(cooldown.TryUse(start + 36s, 30s));
}

TEST(GoHomeCooldownTest, CanCheckThenRecordAUseOnlyAfterTheTransferIsAccepted)
{
    GoHomeCooldown cooldown;
    GoHomeCooldown::Clock::time_point const start{};

    EXPECT_TRUE(cooldown.CanUse(start, 30s));
    cooldown.RecordUse(start);
    EXPECT_FALSE(cooldown.CanUse(start + 29s, 30s));
    EXPECT_TRUE(cooldown.CanUse(start + 30s, 30s));
}

TEST(GoHomeCooldownTest, ZeroDisablesTheWaitAndEarlierTimeIsRejected)
{
    GoHomeCooldown cooldown;
    GoHomeCooldown::Clock::time_point const start{};

    EXPECT_TRUE(cooldown.TryUse(start, 30s));
    EXPECT_FALSE(cooldown.TryUse(start - 1s, 0s));
    EXPECT_TRUE(cooldown.TryUse(start + 1s, 0s));
}
