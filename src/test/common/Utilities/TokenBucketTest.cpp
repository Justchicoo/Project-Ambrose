/*
 * Project Ambrose by Imjustchico
 * Tests token bucket capacity, refill rate, and the capacity cap on a fake clock.
 */

#include "TokenBucket.h"

#include <gtest/gtest.h>

#include <chrono>

namespace
{
    struct FakeClock
    {
        TokenBucket::Clock::time_point now{};

        TokenBucket::TimeSource Source()
        {
            return [this] { return now; };
        }
    };
}

TEST(TokenBucketTest, AllowsCapacityThenRefusesUntilRefilled)
{
    FakeClock clock;
    TokenBucket bucket(5, 2.0, clock.Source());
    for (int i = 0; i < 5; ++i)
        EXPECT_TRUE(bucket.TryConsume());
    EXPECT_FALSE(bucket.TryConsume());

    clock.now += std::chrono::milliseconds(499);
    EXPECT_FALSE(bucket.TryConsume());

    clock.now += std::chrono::milliseconds(1);
    EXPECT_TRUE(bucket.TryConsume());
    EXPECT_FALSE(bucket.TryConsume());
}

TEST(TokenBucketTest, RefillIsCappedAtCapacity)
{
    FakeClock clock;
    TokenBucket bucket(3, 10.0, clock.Source());
    EXPECT_TRUE(bucket.TryConsume(3));
    clock.now += std::chrono::hours(1);
    EXPECT_DOUBLE_EQ(bucket.GetAvailableTokens(), 3.0);
    EXPECT_FALSE(bucket.TryConsume(4));
    EXPECT_TRUE(bucket.TryConsume(3));
}

TEST(TokenBucketTest, ClockGoingBackwardsDoesNotRefill)
{
    FakeClock clock;
    clock.now += std::chrono::seconds(10);
    TokenBucket bucket(1, 1.0, clock.Source());
    EXPECT_TRUE(bucket.TryConsume());
    clock.now -= std::chrono::seconds(5);
    EXPECT_FALSE(bucket.TryConsume());
}
