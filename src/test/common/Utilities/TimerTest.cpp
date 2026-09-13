/*
 * Project Ambrose by Imjustchico
 * Tests wrap-safe millisecond differences and the interval and countdown timers.
 */

#include "Timer.h"

#include <gtest/gtest.h>

#include <thread>

TEST(TimerTest, MSTimeDiffHandlesUint32Wrap)
{
    EXPECT_EQ(GetMSTimeDiff(0xFFFFFFF0u, 0x00000010u), 0x20u);
    EXPECT_EQ(GetMSTimeDiff(0xFFFFFFFFu, 0x00000000u), 1u);
    EXPECT_EQ(GetMSTimeDiff(100u, 250u), 150u);
}

TEST(TimerTest, MSTimeAdvances)
{
    uint32 const start = GetMSTime();
    std::this_thread::sleep_for(Milliseconds(20));
    EXPECT_GE(GetMSTimeDiffToNow(start), 15u);
}

TEST(TimerTest, IntervalTimerPassesAndKeepsRemainder)
{
    IntervalTimer timer(100);
    timer.Update(60);
    EXPECT_FALSE(timer.Passed());
    timer.Update(70);
    EXPECT_TRUE(timer.Passed());
    timer.Reset();
    EXPECT_EQ(timer.GetCurrent(), 30);
    EXPECT_FALSE(timer.Passed());
}

TEST(TimerTest, IntervalTimerNeverGoesNegative)
{
    IntervalTimer timer(100);
    timer.Update(-500);
    EXPECT_EQ(timer.GetCurrent(), 0);
}

TEST(TimerTest, TimeTrackerCountsDown)
{
    TimeTracker tracker(Milliseconds(50));
    tracker.Update(Milliseconds(30));
    EXPECT_FALSE(tracker.Passed());
    tracker.Update(Milliseconds(20));
    EXPECT_TRUE(tracker.Passed());
    tracker.Reset(Seconds(1));
    EXPECT_EQ(tracker.GetExpiry(), Milliseconds(1000));
}
