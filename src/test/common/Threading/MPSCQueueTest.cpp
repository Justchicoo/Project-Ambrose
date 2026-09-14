/*
 * Project Ambrose by Imjustchico
 * Tests the lock-free queue for exactly-once, per-producer ordered delivery and for freeing undelivered values.
 */

#include "MPSCQueue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

TEST(MPSCQueueTest, EmptyQueueDequeuesNothing)
{
    MPSCQueue<int> queue;
    int value = 0;
    EXPECT_TRUE(queue.Empty());
    EXPECT_FALSE(queue.Dequeue(value));
    queue.Enqueue(9);
    EXPECT_FALSE(queue.Empty());
    ASSERT_TRUE(queue.Dequeue(value));
    EXPECT_EQ(value, 9);
    EXPECT_TRUE(queue.Empty());
}

TEST(MPSCQueueTest, ManyProducersOneConsumerExactlyOnceInProducerOrder)
{
    constexpr int Producers = 4;
    constexpr int PerProducer = 100000;
    MPSCQueue<std::unique_ptr<std::pair<int, int>>> queue;
    std::atomic<int> finished{ 0 };
    std::vector<std::thread> producers;
    for (int p = 0; p < Producers; ++p)
    {
        producers.emplace_back([&queue, &finished, p]
        {
            for (int i = 0; i < PerProducer; ++i)
                queue.Enqueue(std::make_unique<std::pair<int, int>>(p, i));
            ++finished;
        });
    }
    std::vector<int> next(Producers, 0);
    int received = 0;
    int outOfOrder = 0;
    std::unique_ptr<std::pair<int, int>> item;
    while (received < Producers * PerProducer)
    {
        if (!queue.Dequeue(item))
        {
            std::this_thread::yield();
            continue;
        }
        if (item->second != next[static_cast<std::size_t>(item->first)])
            ++outOfOrder;
        next[static_cast<std::size_t>(item->first)] = item->second + 1;
        ++received;
    }
    for (std::thread& producer : producers)
        producer.join();
    EXPECT_EQ(outOfOrder, 0);
    for (int count : next)
        EXPECT_EQ(count, PerProducer);
    EXPECT_FALSE(queue.Dequeue(item));
}

TEST(MPSCQueueTest, DestructorReleasesUndeliveredValues)
{
    auto const tracker = std::make_shared<int>(0);
    {
        MPSCQueue<std::shared_ptr<int>> queue;
        for (int i = 0; i < 10; ++i)
            queue.Enqueue(tracker);
        std::shared_ptr<int> one;
        ASSERT_TRUE(queue.Dequeue(one));
        EXPECT_EQ(tracker.use_count(), 11);
    }
    EXPECT_EQ(tracker.use_count(), 1);
}
