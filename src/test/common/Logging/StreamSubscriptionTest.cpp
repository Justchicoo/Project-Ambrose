/*
 * Project Ambrose by Imjustchico
 * Tests the three overflow policies at the layer, with no feed on top: by default a full ring drops its oldest records and counts them; a ring that keeps everything refuses a record rather than evict one it holds, and every record after it even once there is room again, says it overflowed, wakes its reader on the first overflow and reports it only once the reader has taken everything kept; and a ring that keeps the newest record per key replaces a queued record of the same key without counting it as dropped, keeps the rest in sequence order, and still drops the oldest when every key is new and the ring is full.
 */

#include "StreamSubscription.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    struct PlainRecord
    {
        uint64 Sequence = 0;
    };

    struct KeyedRecord
    {
        uint64 Sequence = 0;
        std::string Key = {};
    };

    struct EveryRecord
    {
    };

    template<class Record>
    std::vector<uint64> Take(StreamSubscription<Record, EveryRecord>& subscription, StreamPopResult& result, std::size_t max = 1000)
    {
        std::vector<std::shared_ptr<Record const>> records;
        result = subscription.Pop(records, max);
        std::vector<uint64> sequences;
        for (std::shared_ptr<Record const> const& record : records)
            sequences.push_back(record->Sequence);
        return sequences;
    }

    std::shared_ptr<PlainRecord const> Plain(uint64 sequence)
    {
        return std::make_shared<PlainRecord const>(PlainRecord{ sequence });
    }

    std::shared_ptr<KeyedRecord const> Keyed(uint64 sequence, std::string key)
    {
        return std::make_shared<KeyedRecord const>(KeyedRecord{ sequence, std::move(key) });
    }
}

TEST(StreamSubscriptionTest, TheDefaultDropsTheOldestAndCountsWhatItDropped)
{
    StreamSubscription<PlainRecord, EveryRecord> subscription(EveryRecord{}, 3, {});
    EXPECT_EQ(subscription.GetOverflow(), StreamOverflow::DropOldest);
    for (uint64 sequence = 1; sequence <= 5; ++sequence)
    {
        subscription.Push(Plain(sequence));
    }

    StreamPopResult result;
    EXPECT_EQ(Take(subscription, result), (std::vector<uint64>{ 3, 4, 5 }));
    EXPECT_EQ(result.Dropped, 2u);
    EXPECT_FALSE(result.Overflowed);
    EXPECT_FALSE(subscription.IsOverflowed());
    EXPECT_EQ(subscription.GetTotalDropped(), 2u);
}

TEST(StreamSubscriptionTest, KeepAllNeverEvictsAndSaysItOverflowedOnceItsReaderHasTakenEverything)
{
    StreamSubscription<PlainRecord, EveryRecord> subscription(EveryRecord{}, 3, [] {}, StreamOverflow::KeepAll);
    std::vector<bool> woke;
    for (uint64 sequence = 1; sequence <= 5; ++sequence)
    {
        woke.push_back(subscription.Push(Plain(sequence)));
    }
    EXPECT_EQ(woke, (std::vector<bool>{ true, false, false, true, false })) << "the edge from empty wakes the reader, and so does the first overflow";
    EXPECT_TRUE(subscription.IsOverflowed());

    StreamPopResult first;
    EXPECT_EQ(Take(subscription, first, 2), (std::vector<uint64>{ 1, 2 }));
    EXPECT_FALSE(first.Overflowed) << "a record it kept is still waiting";
    EXPECT_EQ(first.Dropped, 0u);
    EXPECT_FALSE(subscription.Push(Plain(6))) << "once it has overflowed it takes nothing more, even with room again";

    StreamPopResult last;
    EXPECT_EQ(Take(subscription, last), (std::vector<uint64>{ 3 }));
    EXPECT_TRUE(last.Overflowed);
    EXPECT_EQ(last.Dropped, 0u);
    EXPECT_EQ(subscription.GetTotalDropped(), 0u);
}

TEST(StreamSubscriptionTest, NewestPerKeyKeepsOneRecordPerKeyInSequenceOrderAndCountsNothingAsDropped)
{
    StreamSubscription<KeyedRecord, EveryRecord> subscription(EveryRecord{}, 10, {}, StreamOverflow::NewestPerKey);
    std::vector<std::string> const keys{ "gameserver-1", "loginserver", "gameserver-1", "gameserver-1", "patchserver", "loginserver" };
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        subscription.Push(Keyed(index + 1, keys[index]));
    }

    StreamPopResult result;
    EXPECT_EQ(Take(subscription, result), (std::vector<uint64>{ 4, 5, 6 }));
    EXPECT_EQ(result.Dropped, 0u);
    EXPECT_FALSE(result.Overflowed);
    EXPECT_EQ(subscription.GetTotalDropped(), 0u);
}

TEST(StreamSubscriptionTest, NewestPerKeyStillDropsTheOldestWhenEveryKeyIsNewAndTheRingIsFull)
{
    StreamSubscription<KeyedRecord, EveryRecord> subscription(EveryRecord{}, 2, {}, StreamOverflow::NewestPerKey);
    subscription.Push(Keyed(1, "a"));
    subscription.Push(Keyed(2, "b"));
    subscription.Push(Keyed(3, "c"));
    subscription.Push(Keyed(4, "b"));

    StreamPopResult result;
    EXPECT_EQ(Take(subscription, result), (std::vector<uint64>{ 3, 4 }));
    EXPECT_EQ(result.Dropped, 1u) << "only the record pushed out by a new key counts";
    EXPECT_EQ(subscription.GetTotalDropped(), 1u);
}
