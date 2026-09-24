/*
 * Project Ambrose by Imjustchico
 * Tests locked queue ordering, conditional pops, peeking, bulk adds, and concurrent exactly-once use.
 */

#include "LockedQueue.h"
#include "Types.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

TEST(LockedQueueTest, AddNextAndPeekKeepOrder)
{
    LockedQueue<int> queue;
    int value = 0;
    EXPECT_FALSE(queue.Next(value));
    EXPECT_FALSE(queue.Peek(value));
    queue.Add(1);
    queue.Add(2);
    ASSERT_TRUE(queue.Peek(value));
    EXPECT_EQ(value, 1);
    EXPECT_EQ(queue.Size(), 2u);
    ASSERT_TRUE(queue.Next(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.Next(value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(queue.Empty());
}

TEST(LockedQueueTest, CheckerDecidesWhetherFrontIsTaken)
{
    LockedQueue<int> queue;
    std::vector<int> const items{ 10, 3, 20 };
    queue.AddRange(items.begin(), items.end());
    int value = 0;
    auto const isLarge = [](int candidate) { return candidate >= 10; };
    ASSERT_TRUE(queue.Next(value, isLarge));
    EXPECT_EQ(value, 10);
    EXPECT_FALSE(queue.Next(value, isLarge));
    EXPECT_EQ(queue.Size(), 2u);
    ASSERT_TRUE(queue.Next(value));
    EXPECT_EQ(value, 3);
}

TEST(LockedQueueTest, CancelAndClear)
{
    LockedQueue<int> queue;
    queue.Add(1);
    EXPECT_FALSE(queue.IsCancelled());
    queue.Cancel();
    EXPECT_TRUE(queue.IsCancelled());
    queue.Clear();
    EXPECT_TRUE(queue.Empty());
}

TEST(LockedQueueTest, ConcurrentAddAndNextDeliverExactlyOnce)
{
    constexpr int Producers = 4;
    constexpr int PerProducer = 50000;
    LockedQueue<int> queue;
    std::vector<std::atomic<uint8>> seen(Producers * PerProducer);
    std::atomic<int> produced{ 0 };
    std::atomic<int> consumed{ 0 };
    std::vector<std::thread> threads;
    for (int p = 0; p < Producers; ++p)
    {
        threads.emplace_back([&, p]
        {
            for (int i = 0; i < PerProducer; ++i)
                queue.Add(p * PerProducer + i);
            produced += PerProducer;
        });
    }
    for (int c = 0; c < 2; ++c)
    {
        threads.emplace_back([&]
        {
            int value = 0;
            while (consumed.load() < Producers * PerProducer)
            {
                if (queue.Next(value))
                {
                    seen[static_cast<std::size_t>(value)].fetch_add(1);
                    ++consumed;
                }
                else
                    std::this_thread::yield();
            }
        });
    }
    for (std::thread& thread : threads)
        thread.join();
    int wrong = 0;
    for (std::atomic<uint8> const& flag : seen)
        if (flag.load() != 1)
            ++wrong;
    EXPECT_EQ(wrong, 0);
}
