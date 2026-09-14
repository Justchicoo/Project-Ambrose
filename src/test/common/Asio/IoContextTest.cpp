/*
 * Project Ambrose by Imjustchico
 * Tests posting and dispatching onto an io_context, work guards keeping it alive, and strands serializing handlers.
 */

#include "IoContext.h"
#include "ScopeExit.h"
#include "Strand.h"

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <thread>
#include <vector>

TEST(IoContextTest, PostedHandlersRunInsideRun)
{
    Ambrose::Asio::IoContext context;
    int calls = 0;
    Ambrose::Asio::Post(context, [&calls] { ++calls; });
    Ambrose::Asio::Post(context, [&calls] { ++calls; });
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(context.Run(), 2u);
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(context.IsStopped());
    context.Restart();
    Ambrose::Asio::Post(context, [&calls] { ++calls; });
    EXPECT_EQ(context.Poll(), 1u);
    EXPECT_EQ(calls, 3);
}

TEST(IoContextTest, DispatchRunsInlineWhenAlreadyInsideTheContext)
{
    Ambrose::Asio::IoContext context;
    std::vector<int> order;
    Ambrose::Asio::Post(context, [&]
    {
        order.push_back(1);
        Ambrose::Asio::Dispatch(context, [&] { order.push_back(2); });
        Ambrose::Asio::Post(context, [&] { order.push_back(4); });
        order.push_back(3);
    });
    context.Run();
    EXPECT_EQ(order, (std::vector<int>{ 1, 2, 3, 4 }));
}

TEST(IoContextTest, WorkGuardKeepsRunAliveUntilReset)
{
    Ambrose::Asio::IoContext context;
    std::optional<Ambrose::Asio::IoContext::WorkGuard> guard(context.MakeWorkGuard());
    std::promise<void> running;
    std::future<std::size_t> run = std::async(std::launch::async, [&context, &running]
    {
        Ambrose::Asio::Post(context, [&running] { running.set_value(); });
        return context.Run();
    });
    ScopeExit const stopOnExit([&context] { context.Stop(); });
    running.get_future().wait();
    EXPECT_EQ(run.wait_for(std::chrono::milliseconds(30)), std::future_status::timeout);
    guard.reset();
    ASSERT_EQ(run.wait_for(std::chrono::seconds(30)), std::future_status::ready);
    EXPECT_EQ(run.get(), 1u);
}

TEST(IoContextTest, StopEndsRunFromAnotherThread)
{
    Ambrose::Asio::IoContext context;
    auto guard = context.MakeWorkGuard();
    std::future<std::size_t> run = std::async(std::launch::async, [&context] { return context.Run(); });
    context.Stop();
    ASSERT_EQ(run.wait_for(std::chrono::seconds(30)), std::future_status::ready);
    EXPECT_TRUE(context.IsStopped());
}

TEST(IoContextTest, StrandSerializesHandlersAcrossThreads)
{
    Ambrose::Asio::IoContext context(4);
    Ambrose::Asio::Strand const strand = Ambrose::Asio::MakeStrand(context);
    int counter = 0;
    std::atomic<int> concurrent{ 0 };
    std::atomic<int> overlaps{ 0 };
    for (int i = 0; i < 20000; ++i)
    {
        Ambrose::Asio::Post(strand, [&]
        {
            if (concurrent.fetch_add(1) != 0)
                ++overlaps;
            ++counter;
            concurrent.fetch_sub(1);
        });
    }
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&context] { context.Run(); });
    for (std::thread& thread : threads)
        thread.join();
    EXPECT_EQ(counter, 20000);
    EXPECT_EQ(overlaps.load(), 0);
}
