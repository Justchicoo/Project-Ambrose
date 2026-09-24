/*
 * Project Ambrose by Imjustchico
 * Tests the cost-weighted limit against a clock the test moves: an uncosted route is never held back, a costly one spends the burst and is refused with whole seconds to wait, waiting refills it, one caller's spending does not hold back another, an address holds back every caller behind it, a cost larger than the whole burst is refused at once, and a refusal is worth recording once a minute however many are refused in it.
 */

#include "PanelRateLimit.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace
{
    class Fake
    {
    public:
        PanelRateLimit::TimeSource Source() { return [this] { return _now; }; }
        void Advance(std::chrono::milliseconds by) { _now += by; }

    private:
        PanelRateLimit::Clock::time_point _now = PanelRateLimit::Clock::time_point() + std::chrono::hours(1);
    };
}

TEST(PanelRateLimitTest, AnUncostedRouteIsNeverHeldBack)
{
    Fake clock;
    PanelRateLimit limit(clock.Source());
    limit.SetLimits(10, 0.0);
    for (int index = 0; index < 1000; ++index)
        EXPECT_TRUE(limit.Take("session:one", "127.0.0.1", 0).Allowed);
    EXPECT_EQ(limit.Tracked(), 0u);
}

TEST(PanelRateLimitTest, SpendsTheBurstThenRefillsOverTime)
{
    Fake clock;
    PanelRateLimit limit(clock.Source());
    limit.SetLimits(10, 2.0);

    for (int index = 0; index < 5; ++index)
        EXPECT_TRUE(limit.Take("session:one", "127.0.0.1", 2).Allowed) << index;

    PanelRateVerdict const held = limit.Take("session:one", "127.0.0.1", 2);
    EXPECT_FALSE(held.Allowed);
    EXPECT_EQ(held.RetryAfterSeconds, 1u);
    EXPECT_EQ(held.Bucket, "user");
    EXPECT_TRUE(held.FirstThisMinute);

    clock.Advance(std::chrono::milliseconds(1000));
    EXPECT_TRUE(limit.Take("session:one", "127.0.0.1", 2).Allowed);
    EXPECT_FALSE(limit.Take("session:one", "127.0.0.1", 2).Allowed);

    clock.Advance(std::chrono::milliseconds(5000));
    EXPECT_TRUE(limit.Take("session:one", "127.0.0.1", 10).Allowed);
}

TEST(PanelRateLimitTest, OneCallerDoesNotHoldBackAnother)
{
    Fake clock;
    PanelRateLimit limit(clock.Source());
    limit.SetLimits(4, 0.0);

    EXPECT_TRUE(limit.Take("session:one", "10.0.0.1", 4).Allowed);
    EXPECT_FALSE(limit.Take("session:one", "10.0.0.1", 1).Allowed);
    EXPECT_TRUE(limit.Take("session:two", "10.0.0.2", 4).Allowed);
    EXPECT_FALSE(limit.Take("session:two", "10.0.0.2", 1).Allowed);
}

TEST(PanelRateLimitTest, AnAddressHoldsBackEveryCallerBehindIt)
{
    Fake clock;
    PanelRateLimit limit(clock.Source());
    limit.SetLimits(4, 0.0);

    EXPECT_TRUE(limit.Take("session:one", "10.0.0.1", 4).Allowed);
    PanelRateVerdict const held = limit.Take("session:two", "10.0.0.1", 1);
    EXPECT_FALSE(held.Allowed);
    EXPECT_EQ(held.Bucket, "address");
}

TEST(PanelRateLimitTest, ACostLargerThanTheBurstIsRefusedAtOnce)
{
    Fake clock;
    PanelRateLimit limit(clock.Source());
    limit.SetLimits(10, 100.0);

    PanelRateVerdict const held = limit.Take("session:one", "127.0.0.1", 11);
    EXPECT_FALSE(held.Allowed);
    EXPECT_EQ(held.Bucket, "cost");
    EXPECT_EQ(held.RetryAfterSeconds, 1u);
    EXPECT_TRUE(held.FirstThisMinute);

    clock.Advance(std::chrono::milliseconds(60000));
    EXPECT_FALSE(limit.Take("session:one", "127.0.0.1", 11).Allowed);
}

TEST(PanelRateLimitTest, ARefusalIsWorthRecordingOnceAMinute)
{
    Fake clock;
    PanelRateLimit limit(clock.Source());
    limit.SetLimits(2, 0.0);

    EXPECT_TRUE(limit.Take("session:one", "127.0.0.1", 2).Allowed);
    EXPECT_TRUE(limit.Take("session:one", "127.0.0.1", 1).FirstThisMinute);
    for (int index = 0; index < 20; ++index)
        EXPECT_FALSE(limit.Take("session:one", "127.0.0.1", 1).FirstThisMinute) << index;

    clock.Advance(std::chrono::milliseconds(59000));
    EXPECT_FALSE(limit.Take("session:one", "127.0.0.1", 1).FirstThisMinute);
    clock.Advance(std::chrono::milliseconds(2000));
    EXPECT_TRUE(limit.Take("session:one", "127.0.0.1", 1).FirstThisMinute);

    limit.Forget("session:one", "127.0.0.1");
    EXPECT_EQ(limit.Tracked(), 0u);
}
