/*
 * Project Ambrose by Imjustchico
 * Tests the id generator: a million ids claimed by eight threads released together never repeat and fill the range without gaps, ids claimed while another thread keeps resuming the generator never repeat either, zero is never handed out, resuming only ever raises the next id, a second generator resumed from the first one's highest id continues above it as after a restart, and the largest 64-bit id is handed out once before the generator refuses.
 */

#include "GuidGenerator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <latch>
#include <limits>
#include <thread>
#include <vector>

TEST(GuidGeneratorTest, AMillionIdsFromEightThreadsNeverRepeat)
{
    constexpr std::size_t Threads = 8;
    constexpr std::size_t PerThread = 125000;
    GuidGenerator generator;
    std::vector<std::vector<uint64>> claimed(Threads);
    std::latch start(1);
    {
        std::vector<std::jthread> workers;
        for (std::size_t thread = 0; thread < Threads; ++thread)
        {
            workers.emplace_back([&generator, &start, &ids = claimed[thread]]
            {
                ids.reserve(PerThread);
                start.wait();
                for (std::size_t index = 0; index < PerThread; ++index)
                    ids.push_back(generator.Generate().value_or(0));
            });
        }
        start.count_down();
    }

    std::vector<uint64> all;
    all.reserve(Threads * PerThread);
    for (std::vector<uint64> const& ids : claimed)
        all.insert(all.end(), ids.begin(), ids.end());
    std::sort(all.begin(), all.end());
    ASSERT_EQ(all.size(), Threads * PerThread);
    EXPECT_EQ(all.front(), 1u);
    EXPECT_EQ(all.back(), Threads * PerThread);
    EXPECT_EQ(std::adjacent_find(all.begin(), all.end()), all.end());
    EXPECT_EQ(generator.PeekNext(), Threads * PerThread + 1);
}

TEST(GuidGeneratorTest, IdsClaimedWhileAnotherThreadResumesNeverRepeat)
{
    constexpr std::size_t Threads = 4;
    constexpr std::size_t PerThread = 50000;
    GuidGenerator generator;
    std::vector<std::vector<uint64>> claimed(Threads);
    std::atomic<bool> done{ false };
    std::latch start(1);
    {
        std::jthread resumer([&generator, &start, &done]
        {
            start.wait();
            uint64 mark = 0;
            while (!done.load(std::memory_order_relaxed))
            {
                mark += 997;
                generator.Resume(mark);
            }
        });
        {
            std::vector<std::jthread> workers;
            for (std::size_t thread = 0; thread < Threads; ++thread)
            {
                workers.emplace_back([&generator, &start, &ids = claimed[thread]]
                {
                    ids.reserve(PerThread);
                    start.wait();
                    for (std::size_t index = 0; index < PerThread; ++index)
                        ids.push_back(generator.Generate().value_or(0));
                });
            }
            start.count_down();
        }
        done.store(true, std::memory_order_relaxed);
    }

    std::vector<uint64> all;
    for (std::vector<uint64> const& ids : claimed)
        all.insert(all.end(), ids.begin(), ids.end());
    std::sort(all.begin(), all.end());
    ASSERT_EQ(all.size(), Threads * PerThread);
    EXPECT_NE(all.front(), 0u);
    EXPECT_EQ(std::adjacent_find(all.begin(), all.end()), all.end());
}

TEST(GuidGeneratorTest, ResumingContinuesAboveTheHighestStoredId)
{
    GuidGenerator first(0);
    EXPECT_EQ(first.Generate(), 1u);
    std::vector<uint64> issued;
    for (int index = 0; index < 1000; ++index)
        issued.push_back(first.Generate().value_or(0));

    GuidGenerator restarted;
    restarted.Resume(*std::max_element(issued.begin(), issued.end()));
    std::optional<uint64> const next = restarted.Generate();
    ASSERT_TRUE(next);
    EXPECT_EQ(*next, 1002u);
    EXPECT_TRUE(std::none_of(issued.begin(), issued.end(), [&next](uint64 id) { return id == *next; }));

    restarted.Resume(5);
    EXPECT_EQ(restarted.Generate(), 1003u);
    restarted.Resume(0);
    EXPECT_EQ(restarted.PeekNext(), 1004u);
}

TEST(GuidGeneratorTest, TheLargestIdIsHandedOutOnceBeforeTheGeneratorRefuses)
{
    uint64 const largest = std::numeric_limits<uint64>::max();
    GuidGenerator generator(largest - 1);
    EXPECT_EQ(generator.Generate(), largest - 1);
    EXPECT_EQ(generator.Generate(), largest);
    EXPECT_FALSE(generator.Generate());
    EXPECT_FALSE(generator.PeekNext());
    generator.Resume(10);
    EXPECT_FALSE(generator.Generate());

    GuidGenerator full;
    full.Resume(largest);
    EXPECT_FALSE(full.Generate());
}
