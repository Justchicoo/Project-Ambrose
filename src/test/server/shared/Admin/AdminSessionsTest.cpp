/*
 * Project Ambrose by Imjustchico
 * Tests the admin API's browser sessions against a clock the test moves: a session opens with a secret and CSRF token that differ every time and finds its CSRF token again, an unknown or oversized secret finds nothing, idle and absolute lifetimes end a session, the least recently used session ends when the cap is reached, and closing one or all ends them.
 */

#include "AdminSessions.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

namespace
{
    struct MovingClock
    {
        AdminSessions::Clock::time_point Now = AdminSessions::Clock::time_point{} + std::chrono::hours(1);

        AdminSessions::TimeSource Source()
        {
            return [this] { return Now; };
        }
    };
}

TEST(AdminSessionsTest, OpensADistinctSessionAndFindsItsCsrfToken)
{
    MovingClock clock;
    AdminSessions sessions(clock.Source());
    AdminSession const first = sessions.Open();
    AdminSession const second = sessions.Open();

    EXPECT_EQ(first.Secret.size(), 43u);
    EXPECT_EQ(first.Csrf.size(), 43u);
    EXPECT_NE(first.Secret, second.Secret);
    EXPECT_NE(first.Csrf, second.Csrf);
    EXPECT_NE(first.Secret, first.Csrf);
    ASSERT_TRUE(sessions.Find(first.Secret).has_value());
    EXPECT_EQ(*sessions.Find(first.Secret), first.Csrf);
    EXPECT_EQ(*sessions.Find(second.Secret), second.Csrf);
    EXPECT_EQ(sessions.Count(), 2u);
}

TEST(AdminSessionsTest, AnUnknownOrOversizedSecretFindsNothing)
{
    AdminSessions sessions;
    AdminSession const opened = sessions.Open();

    EXPECT_FALSE(sessions.Find("").has_value());
    EXPECT_FALSE(sessions.Find(opened.Csrf).has_value());
    EXPECT_FALSE(sessions.Find(opened.Secret + "x").has_value());
    EXPECT_FALSE(sessions.Find(std::string(AdminSessions::MaxSecretLength + 1, 'a')).has_value());
    EXPECT_TRUE(sessions.Find(opened.Secret).has_value());
}

TEST(AdminSessionsTest, AnIdleSessionEnds)
{
    MovingClock clock;
    AdminSessions sessions(clock.Source());
    sessions.SetLifetimes(std::chrono::minutes(10), std::chrono::hours(1));
    AdminSession const opened = sessions.Open();

    clock.Now += std::chrono::minutes(9);
    EXPECT_TRUE(sessions.Find(opened.Secret).has_value());
    clock.Now += std::chrono::minutes(9);
    EXPECT_TRUE(sessions.Find(opened.Secret).has_value());
    clock.Now += std::chrono::minutes(10);
    EXPECT_FALSE(sessions.Find(opened.Secret).has_value());
    EXPECT_EQ(sessions.Count(), 0u);
}

TEST(AdminSessionsTest, ASessionEndsAtItsAbsoluteLifetimeEvenWhileInUse)
{
    MovingClock clock;
    AdminSessions sessions(clock.Source());
    sessions.SetLifetimes(std::chrono::minutes(10), std::chrono::minutes(30));
    AdminSession const opened = sessions.Open();

    for (int step = 0; step < 5; ++step)
    {
        clock.Now += std::chrono::minutes(5);
        EXPECT_TRUE(sessions.Find(opened.Secret).has_value()) << step;
    }
    clock.Now += std::chrono::minutes(5);
    EXPECT_FALSE(sessions.Find(opened.Secret).has_value());
}

TEST(AdminSessionsTest, TheLeastRecentlyUsedSessionEndsAtTheCap)
{
    MovingClock clock;
    AdminSessions sessions(clock.Source());
    std::vector<AdminSession> opened;
    for (std::size_t index = 0; index < AdminSessions::MaxSessions; ++index)
    {
        opened.push_back(sessions.Open());
        clock.Now += std::chrono::seconds(1);
    }
    ASSERT_TRUE(sessions.Find(opened.front().Secret).has_value());
    clock.Now += std::chrono::seconds(1);

    AdminSession const extra = sessions.Open();
    EXPECT_EQ(sessions.Count(), AdminSessions::MaxSessions);
    EXPECT_TRUE(sessions.Find(opened.front().Secret).has_value());
    EXPECT_FALSE(sessions.Find(opened[1].Secret).has_value());
    EXPECT_TRUE(sessions.Find(extra.Secret).has_value());
}

TEST(AdminSessionsTest, ClosingEndsOneOrAll)
{
    AdminSessions sessions;
    AdminSession const first = sessions.Open();
    AdminSession const second = sessions.Open();

    EXPECT_TRUE(sessions.Close(first.Secret));
    EXPECT_FALSE(sessions.Close(first.Secret));
    EXPECT_FALSE(sessions.Find(first.Secret).has_value());
    EXPECT_TRUE(sessions.Find(second.Secret).has_value());

    sessions.CloseAll();
    EXPECT_FALSE(sessions.Find(second.Secret).has_value());
    EXPECT_EQ(sessions.Count(), 0u);
}
