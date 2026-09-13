/*
 * Project Ambrose by Imjustchico
 * Tests backlog, subscriber filters, drop-oldest counts, wake edges and survival across reloads.
 */

#include "Log.h"
#include "LogStreamHub.h"
#include "LogTestHarness.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

namespace
{
    LogMessage MakeRecord(uint64 sequence, LogLevel level = LogLevel::Info, std::string category = "server")
    {
        LogMessage message;
        message.Level = level;
        message.Category = std::move(category);
        message.Text = "record " + std::to_string(sequence);
        message.Sequence = sequence;
        return message;
    }

    std::vector<std::shared_ptr<LogMessage const>> PopAll(LogSubscription& subscription, LogPopResult* result = nullptr)
    {
        std::vector<std::shared_ptr<LogMessage const>> records;
        LogPopResult const popped = subscription.Pop(records, 1000000);
        if (result)
            *result = popped;
        return records;
    }
}

TEST(LogStreamTest, NewSubscriberReceivesFilteredBacklog)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Stream = 3,1,0,1000\nLogger.root = 1,Stream\n");
    for (int i = 0; i < 1500; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", i);
    std::shared_ptr<LogSubscription> const subscription = harness.GetLog().GetStreamHub().Subscribe({});
    std::vector<std::shared_ptr<LogMessage const>> const records = PopAll(*subscription);
    ASSERT_EQ(records.size(), 1000u);
    EXPECT_EQ(records.front()->Sequence, 501u);
    EXPECT_EQ(records.back()->Sequence, 1500u);
    EXPECT_EQ(records.back()->Text, "1499");
}

TEST(LogStreamTest, WarnFilterReceivesOnlyWarnErrorFatal)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Stream = 3,1,0\nLogger.root = 1,Stream\n");
    LogStreamFilter filter;
    filter.MinLevel = LogLevel::Warn;
    std::shared_ptr<LogSubscription> const subscription = harness.GetLog().GetStreamHub().Subscribe(filter);
    for (uint8 level = 1; level <= 6; ++level)
        AMBROSE_LOG(harness.GetLog(), static_cast<LogLevel>(level), "server", "level {}", level);
    std::vector<std::shared_ptr<LogMessage const>> const records = PopAll(*subscription);
    ASSERT_EQ(records.size(), 3u);
    EXPECT_EQ(records[0]->Level, LogLevel::Warn);
    EXPECT_EQ(records[2]->Level, LogLevel::Fatal);
}

TEST(LogStreamTest, CategoryFilterMatchesAtDots)
{
    LogStreamHub hub;
    LogStreamFilter filter;
    filter.Categories = { "sql" };
    std::shared_ptr<LogSubscription> const subscription = hub.Subscribe(filter);
    hub.Publish(MakeRecord(1, LogLevel::Info, "sql.sql"));
    hub.Publish(MakeRecord(2, LogLevel::Info, "sqlx"));
    hub.Publish(MakeRecord(3, LogLevel::Info, "sql"));
    hub.Publish(MakeRecord(4, LogLevel::Info, "server.sql"));
    std::vector<std::shared_ptr<LogMessage const>> const records = PopAll(*subscription);
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0]->Sequence, 1u);
    EXPECT_EQ(records[1]->Sequence, 3u);
}

TEST(LogStreamTest, SlowSubscriberDropsOldestAndReportsCount)
{
    LogStreamHub hub;
    std::shared_ptr<LogSubscription> const subscription = hub.Subscribe({}, 10);
    for (uint64 i = 1; i <= 100; ++i)
        hub.Publish(MakeRecord(i));
    LogPopResult result;
    std::vector<std::shared_ptr<LogMessage const>> const records = PopAll(*subscription, &result);
    ASSERT_EQ(records.size(), 10u);
    EXPECT_EQ(result.Dropped, 90u);
    EXPECT_EQ(records.front()->Sequence, 91u);
    EXPECT_EQ(records.back()->Sequence, 100u);
    PopAll(*subscription, &result);
    EXPECT_EQ(result.Dropped, 0u);
    EXPECT_EQ(subscription->GetTotalDropped(), 90u);
}

TEST(LogStreamTest, StalledSubscriberDoesNotBlockLogging)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Stream = 3,1,0,100\nLogger.root = 1,Stream\n");
    std::shared_ptr<LogSubscription> const subscription = harness.GetLog().GetStreamHub().Subscribe({}, 10);
    for (int i = 0; i < 100000; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", i);
    EXPECT_EQ(subscription->GetQueuedCount(), 10u);
    EXPECT_EQ(subscription->GetTotalDropped(), 99990u);
    EXPECT_EQ(harness.GetLog().GetStreamHub().GetBacklog().size(), 100u);
}

TEST(LogStreamTest, WakeFiresOnEmptyToNonEmptyEdgeOnly)
{
    LogStreamHub hub;
    std::atomic<int> wakes{ 0 };
    std::shared_ptr<LogSubscription> const subscription = hub.Subscribe({}, 100, [&wakes] { ++wakes; });
    hub.Publish(MakeRecord(1));
    hub.Publish(MakeRecord(2));
    hub.Publish(MakeRecord(3));
    EXPECT_EQ(wakes.load(), 1);
    PopAll(*subscription);
    hub.Publish(MakeRecord(4));
    EXPECT_EQ(wakes.load(), 2);
}

TEST(LogStreamTest, SubscribeWithBacklogWakesOnce)
{
    LogStreamHub hub;
    hub.Publish(MakeRecord(1));
    hub.Publish(MakeRecord(2));
    std::atomic<int> wakes{ 0 };
    std::shared_ptr<LogSubscription> const subscription = hub.Subscribe({}, 100, [&wakes] { ++wakes; });
    EXPECT_EQ(wakes.load(), 1);
    EXPECT_EQ(subscription->GetQueuedCount(), 2u);
}

TEST(LogStreamTest, SubscribeDuringPublishHasNoGapOrDuplicate)
{
    LogStreamHub hub;
    hub.SetBacklogCapacity(1000);
    std::atomic<uint64> published{ 0 };
    std::thread publisher([&hub, &published]
    {
        for (uint64 i = 1; i <= 20000; ++i)
        {
            hub.Publish(MakeRecord(i));
            published = i;
        }
    });
    while (published.load() < 5000)
        std::this_thread::yield();
    std::shared_ptr<LogSubscription> const subscription = hub.Subscribe({}, 100000);
    publisher.join();
    std::vector<std::shared_ptr<LogMessage const>> const records = PopAll(*subscription);
    ASSERT_FALSE(records.empty());
    EXPECT_EQ(records.back()->Sequence, 20000u);
    for (std::size_t i = 1; i < records.size(); ++i)
        ASSERT_EQ(records[i]->Sequence, records[i - 1]->Sequence + 1) << "at " << i;
    EXPECT_LE(records.front()->Sequence, 20000u - records.size() + 1);
}

TEST(LogStreamTest, SubscribersSurviveReload)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Stream = 3,1,0,1000\nLogger.root = 1,Stream\n");
    std::shared_ptr<LogSubscription> const subscription = harness.GetLog().GetStreamHub().Subscribe({});
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "before");
    harness.ApplyOrFail("Appender.Stream = 3,2,0,500\nAppender.Capture = 200,1,0\nLogger.root = 1,Stream Capture\n");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "after");
    std::vector<std::shared_ptr<LogMessage const>> const records = PopAll(*subscription);
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[1]->Text, "after");
    EXPECT_EQ(harness.GetLog().GetStreamHub().GetBacklogCapacity(), 500u);
    EXPECT_EQ(harness.GetLog().GetStreamHub().GetSubscriberCount(), 1u);
}

TEST(LogStreamTest, ExpiredSubscriptionsArePruned)
{
    LogStreamHub hub;
    std::shared_ptr<LogSubscription> kept = hub.Subscribe({});
    std::shared_ptr<LogSubscription> closed = hub.Subscribe({});
    {
        std::shared_ptr<LogSubscription> const dropped = hub.Subscribe({});
    }
    closed->Close();
    EXPECT_EQ(hub.GetSubscriberCount(), 1u);
    hub.Publish(MakeRecord(1));
    EXPECT_EQ(kept->GetQueuedCount(), 1u);
    EXPECT_EQ(closed->GetQueuedCount(), 0u);
    kept.reset();
    hub.Publish(MakeRecord(2));
    EXPECT_EQ(hub.GetSubscriberCount(), 0u);
}
