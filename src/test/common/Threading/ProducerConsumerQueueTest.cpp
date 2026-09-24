/*
 * Project Ambrose by Imjustchico
 * Tests exactly-once delivery across producers and consumers, cancel waking waiters, close draining, and timed waits.
 */

#include "ProducerConsumerQueue.h"
#include "ScopeExit.h"
#include "Types.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

TEST(ProducerConsumerQueueTest, FourProducersFourConsumersDeliverExactlyOnce)
{
    constexpr int Producers = 4;
    constexpr int Consumers = 4;
    constexpr int PerProducer = 100000;
    ProducerConsumerQueue<int> queue;
    std::vector<std::atomic<uint8>> seen(Producers * PerProducer);
    std::atomic<int> duplicates{ 0 };
    std::atomic<int> consumed{ 0 };

    std::vector<std::thread> consumers;
    for (int c = 0; c < Consumers; ++c)
    {
        consumers.emplace_back([&]
        {
            int value = 0;
            while (queue.WaitAndPop(value))
            {
                if (seen[static_cast<std::size_t>(value)].fetch_add(1) != 0)
                    ++duplicates;
                ++consumed;
            }
        });
    }
    std::vector<std::thread> producers;
    for (int p = 0; p < Producers; ++p)
    {
        producers.emplace_back([&queue, p]
        {
            for (int i = 0; i < PerProducer; ++i)
                queue.Push(p * PerProducer + i);
        });
    }
    for (std::thread& producer : producers)
        producer.join();
    queue.Close();
    for (std::thread& consumer : consumers)
        consumer.join();

    EXPECT_EQ(consumed.load(), Producers * PerProducer);
    EXPECT_EQ(duplicates.load(), 0);
    int missing = 0;
    for (std::atomic<uint8> const& flag : seen)
        if (flag.load() != 1)
            ++missing;
    EXPECT_EQ(missing, 0);
    EXPECT_TRUE(queue.Empty());
}

TEST(ProducerConsumerQueueTest, CancelWakesEveryBlockedConsumer)
{
    ProducerConsumerQueue<int> queue;
    std::atomic<int> waiting{ 0 };
    std::vector<std::future<bool>> results;
    for (int i = 0; i < 4; ++i)
    {
        results.push_back(std::async(std::launch::async, [&queue, &waiting]
        {
            int value = 0;
            ++waiting;
            return queue.WaitAndPop(value);
        }));
    }
    ScopeExit const cancelOnExit([&queue] { queue.Cancel(); });
    while (waiting.load() < 4)
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    queue.Cancel();
    for (std::future<bool>& result : results)
    {
        ASSERT_EQ(result.wait_for(std::chrono::seconds(30)), std::future_status::ready);
        EXPECT_FALSE(result.get());
    }
    EXPECT_TRUE(queue.IsCancelled());
}

TEST(ProducerConsumerQueueTest, CancelStopsImmediatelyEvenWithItemsLeft)
{
    ProducerConsumerQueue<int> queue;
    ASSERT_TRUE(queue.Push(1));
    ASSERT_TRUE(queue.Push(2));
    queue.Cancel();
    int value = 0;
    EXPECT_FALSE(queue.WaitAndPop(value));
    EXPECT_FALSE(queue.Pop(value));
    EXPECT_FALSE(queue.Push(3));
    EXPECT_EQ(queue.TakeAll().size(), 2u);
    EXPECT_TRUE(queue.Empty());
}

TEST(ProducerConsumerQueueTest, CloseRejectsPushesButDrainsRemainingItems)
{
    ProducerConsumerQueue<int> queue;
    queue.Push(7);
    queue.Push(8);
    queue.Close();
    EXPECT_FALSE(queue.Push(9));
    int value = 0;
    ASSERT_TRUE(queue.WaitAndPop(value));
    EXPECT_EQ(value, 7);
    ASSERT_TRUE(queue.WaitAndPop(value));
    EXPECT_EQ(value, 8);
    EXPECT_FALSE(queue.WaitAndPop(value));
    EXPECT_TRUE(queue.IsClosed());
}

TEST(ProducerConsumerQueueTest, WaitAndPopForTimesOutWhenEmpty)
{
    ProducerConsumerQueue<int> queue;
    int value = 0;
    auto const start = std::chrono::steady_clock::now();
    EXPECT_FALSE(queue.WaitAndPopFor(value, std::chrono::milliseconds(30)));
    EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(25));
    queue.Push(5);
    EXPECT_TRUE(queue.WaitAndPopFor(value, std::chrono::seconds(10)));
    EXPECT_EQ(value, 5);
}

TEST(ProducerConsumerQueueTest, PopDoesNotBlockAndKeepsOrder)
{
    ProducerConsumerQueue<int> queue;
    int value = 0;
    EXPECT_FALSE(queue.Pop(value));
    for (int i = 0; i < 5; ++i)
        queue.Push(i);
    EXPECT_EQ(queue.Size(), 5u);
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_TRUE(queue.Pop(value));
        EXPECT_EQ(value, i);
    }
}

TEST(ProducerConsumerQueueTest, MoveOnlyValuesAreSupported)
{
    ProducerConsumerQueue<std::unique_ptr<int>> queue;
    ASSERT_TRUE(queue.Push(std::make_unique<int>(42)));
    ASSERT_TRUE(queue.Emplace(new int(43)));
    std::unique_ptr<int> value;
    ASSERT_TRUE(queue.WaitAndPop(value));
    EXPECT_EQ(*value, 42);
    ASSERT_TRUE(queue.Pop(value));
    EXPECT_EQ(*value, 43);
}
