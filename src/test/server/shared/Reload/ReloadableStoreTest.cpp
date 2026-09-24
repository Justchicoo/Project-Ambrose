/*
 * Project Ambrose by Imjustchico
 * Checks what a store promises a reader: a snapshot taken before a replacement goes on describing the generation it was taken from for as long as it is held, a replacement is seen whole or not at all rather than half way through, the generation number counts replacements rather than readers, and readers running against a writer that never stops replacing never see a generation whose parts disagree, which is the property a thread sanitizer run is there to hold.
 */

#include "ReloadableStore.h"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace
{
    struct Contents
    {
        uint32 Number = 0;
        std::string Spelled;
        std::vector<uint32> Repeated;

        static Contents Of(uint32 number)
        {
            Contents contents;
            contents.Number = number;
            contents.Spelled = std::to_string(number);
            contents.Repeated.assign(16, number);
            return contents;
        }

        bool Agrees() const
        {
            if (Spelled != std::to_string(Number))
                return false;
            for (uint32 const value : Repeated)
                if (value != Number)
                    return false;
            return Repeated.size() == 16;
        }
    };
}

TEST(ReloadableStoreTest, AnUnfilledStoreStillAnswers)
{
    ReloadableStore<Contents> store;
    ReloadableStore<Contents>::Snapshot const snapshot = store.Get();
    ASSERT_TRUE(snapshot) << "a store that has never been filled must not hand back nothing";
    EXPECT_EQ(snapshot->Number, 0u);
    EXPECT_EQ(store.GetGeneration(), 0u);
}

TEST(ReloadableStoreTest, AHeldSnapshotKeepsTheGenerationItWasTakenFrom)
{
    ReloadableStore<Contents> store(Contents::Of(1));
    ReloadableStore<Contents>::Snapshot const first = store.Get();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->Number, 1u);

    store.Replace(Contents::Of(2));

    EXPECT_EQ(first->Number, 1u) << "a reader holding a snapshot must not have it changed underneath";
    EXPECT_EQ(store.Get()->Number, 2u) << "a reader taking a new snapshot must get the new one";
}

TEST(ReloadableStoreTest, TheGenerationCountsReplacements)
{
    ReloadableStore<Contents> store;
    EXPECT_EQ(store.GetGeneration(), 0u);
    store.Replace(Contents::Of(1));
    EXPECT_EQ(store.GetGeneration(), 1u);
    store.Replace(Contents::Of(2));
    EXPECT_EQ(store.GetGeneration(), 2u);
    (void)store.Get();
    (void)store.Get();
    EXPECT_EQ(store.GetGeneration(), 2u) << "reading is not replacing";
}

TEST(ReloadableStoreTest, AnEmptySnapshotIsRefusedRatherThanServed)
{
    ReloadableStore<Contents> store(Contents::Of(7));
    store.Replace(ReloadableStore<Contents>::Snapshot{});
    EXPECT_EQ(store.Get()->Number, 7u) << "nothing must never replace something";
    EXPECT_EQ(store.GetGeneration(), 0u);
}

TEST(ReloadableStoreTest, ReadersRunningAgainstAWriterNeverSeeAGenerationWhosePartsDisagree)
{
    ReloadableStore<Contents> store(Contents::Of(0));
    std::atomic<bool> stop{ false };
    std::atomic<uint64> torn{ 0 };
    std::atomic<uint64> reads{ 0 };

    std::vector<std::thread> readers;
    for (int reader = 0; reader < 4; ++reader)
        readers.emplace_back([&store, &stop, &torn, &reads]
        {
            while (!stop.load(std::memory_order_relaxed))
            {
                ReloadableStore<Contents>::Snapshot const snapshot = store.Get();
                if (!snapshot || !snapshot->Agrees())
                    torn.fetch_add(1, std::memory_order_relaxed);
                uint32 const seen = snapshot ? snapshot->Number : 0;
                if (snapshot && (snapshot->Number != seen || !snapshot->Agrees()))
                    torn.fetch_add(1, std::memory_order_relaxed);
                reads.fetch_add(1, std::memory_order_relaxed);
            }
        });

    for (uint32 generation = 1; generation <= 2000; ++generation)
        store.Replace(Contents::Of(generation));

    stop.store(true, std::memory_order_relaxed);
    for (std::thread& reader : readers)
        reader.join();

    EXPECT_EQ(torn.load(), 0u) << "a reader saw a generation whose parts did not agree with each other";
    EXPECT_GT(reads.load(), 0u) << "the readers must actually have read something";
    EXPECT_EQ(store.GetGeneration(), 2000u);
    EXPECT_EQ(store.Get()->Number, 2000u);
}
