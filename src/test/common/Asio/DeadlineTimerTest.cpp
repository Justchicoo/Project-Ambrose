/*
 * Project Ambrose by Imjustchico
 * Tests that deadline timers fire on the io_context no earlier than asked, and that cancel prevents the handler.
 */

#include "DeadlineTimer.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>

TEST(DeadlineTimerTest, FiresOnTheIoContextWithinTolerance)
{
    Ambrose::Asio::IoContext context;
    Ambrose::Asio::DeadlineTimer timer(context);
    auto const start = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point fired;
    bool succeeded = false;
    timer.ExpiresAfter(std::chrono::milliseconds(100));
    timer.AsyncWait([&](std::error_code const& error)
    {
        succeeded = !error;
        fired = std::chrono::steady_clock::now();
    });
    context.Run();
    ASSERT_TRUE(succeeded);
    auto const elapsed = fired - start;
    EXPECT_GE(elapsed, std::chrono::milliseconds(100));
    EXPECT_LT(elapsed, std::chrono::seconds(10));
}

TEST(DeadlineTimerTest, CancelPreventsTheExpiryHandler)
{
    Ambrose::Asio::IoContext context;
    Ambrose::Asio::DeadlineTimer timer(context, std::chrono::milliseconds(50));
    std::atomic<int> fired{ 0 };
    std::error_code received;
    timer.OnExpiry([&fired] { ++fired; });
    timer.AsyncWait([&received](std::error_code const& error) { received = error; });
    EXPECT_EQ(timer.Cancel(), 2u);
    context.Run();
    EXPECT_EQ(fired.load(), 0);
    EXPECT_EQ(received, asio::error::operation_aborted);
}

TEST(DeadlineTimerTest, CancelAfterExpiryStillPreventsOnExpiry)
{
    Ambrose::Asio::IoContext context;
    auto const now = std::chrono::steady_clock::now();
    Ambrose::Asio::DeadlineTimer first(context);
    Ambrose::Asio::DeadlineTimer second(context);
    first.ExpiresAt(now - std::chrono::seconds(2));
    second.ExpiresAt(now - std::chrono::seconds(1));
    std::size_t cancelledWaits = 99;
    int fired = 0;
    first.OnExpiry([&] { cancelledWaits = second.Cancel(); });
    second.OnExpiry([&fired] { ++fired; });
    context.Run();
    EXPECT_EQ(cancelledWaits, 0u);
    EXPECT_EQ(fired, 0);
}

TEST(DeadlineTimerTest, MovingTheExpiryCancelsTheOldWait)
{
    Ambrose::Asio::IoContext context;
    Ambrose::Asio::DeadlineTimer timer(context, std::chrono::hours(1));
    int aborted = 0;
    int fired = 0;
    timer.AsyncWait([&](std::error_code const& error) { error ? ++aborted : ++fired; });
    EXPECT_EQ(timer.ExpiresAfter(std::chrono::milliseconds(1)), 1u);
    timer.OnExpiry([&fired] { ++fired; });
    context.Run();
    EXPECT_EQ(aborted, 1);
    EXPECT_EQ(fired, 1);
    EXPECT_LE(timer.GetExpiry(), std::chrono::steady_clock::now());
}

TEST(DeadlineTimerTest, StrandBoundHandlerRunsThroughTheStrand)
{
    Ambrose::Asio::IoContext context;
    Ambrose::Asio::Strand const strand = Ambrose::Asio::MakeStrand(context);
    Ambrose::Asio::DeadlineTimer timer(context, std::chrono::milliseconds(1));
    bool inStrand = false;
    timer.AsyncWait(strand, [&](std::error_code const&) { inStrand = strand.running_in_this_thread(); });
    context.Run();
    EXPECT_TRUE(inStrand);
}
