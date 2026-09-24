/*
 * Project Ambrose by Imjustchico
 * Tests async ordering, flush barriers, drop policy, shutdown drain and sync fallback after shutdown.
 */

#include "Log.h"
#include "LogTestHarness.h"
#include "StringUtil.h"

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <map>
#include <thread>

namespace
{
    bool ParseNumbered(std::string const& text, int& thread, int& number)
    {
        std::size_t const space = text.find(" n");
        if (text.size() < 4 || text[0] != 't' || space == std::string::npos)
            return false;
        std::optional<int> const parsedThread = Ambrose::StringTo<int>(std::string_view(text).substr(1, space - 1));
        std::optional<int> const parsedNumber = Ambrose::StringTo<int>(std::string_view(text).substr(space + 2));
        if (!parsedThread || !parsedNumber)
            return false;
        thread = *parsedThread;
        number = *parsedNumber;
        return true;
    }

    std::string const AsyncBody = "Log.Async.Enable = 1\nAppender.A = 200,1,0\nLogger.root = 1,A\n";

    void ExpectEveryThreadInOrder(std::vector<LogMessage> const& messages, int threads, int perThread)
    {
        std::map<int, int> next;
        for (LogMessage const& message : messages)
        {
            int thread = 0;
            int number = 0;
            if (!ParseNumbered(message.Text, thread, number))
                continue;
            ASSERT_EQ(number, next[thread]) << "thread " << thread;
            next[thread] = number + 1;
        }
        ASSERT_EQ(static_cast<int>(next.size()), threads);
        for (auto const& [thread, count] : next)
            EXPECT_EQ(count, perThread) << "thread " << thread;
    }

    void Produce(Log& log, int thread, int count)
    {
        for (int n = 0; n < count; ++n)
            AMBROSE_LOG(log, LogLevel::Info, "server.producer", "t{} n{}", thread, n);
    }
}

TEST(LogAsyncTest, AsyncPreservesPerThreadOrder)
{
    LogTestHarness harness;
    harness.ApplyOrFail(AsyncBody);
    ASSERT_TRUE(harness.GetLog().GetStatistics().Async);
    std::vector<std::thread> producers;
    for (int thread = 0; thread < 4; ++thread)
        producers.emplace_back([&harness, thread] { Produce(harness.GetLog(), thread, 25000); });
    for (std::thread& producer : producers)
        producer.join();
    ASSERT_TRUE(harness.GetLog().Flush());
    std::vector<LogMessage> const messages = harness.Store().Messages("A");
    EXPECT_EQ(messages.size(), 100000u);
    ExpectEveryThreadInOrder(messages, 4, 25000);
}

TEST(LogAsyncTest, FlushIsABarrierUnderConstantLoad)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Log.Async.Enable = 1\nLog.Async.QueueSize = 1024\nAppender.A = 200,1,0\nAppender.Marker = 200,1,0\nLogger.root = 1,A\nLogger.marker = 1,Marker\n");
    harness.Store().SetCountOnly("A");
    std::atomic<bool> stop{ false };
    std::vector<std::thread> producers;
    for (int thread = 0; thread < 4; ++thread)
    {
        producers.emplace_back([&harness, &stop, thread]
        {
            for (int n = 0; !stop.load(); ++n)
                AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server.producer", "t{} n{}", thread, n);
        });
    }
    int completed = 0;
    for (int i = 0; i < 50; ++i)
    {
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "marker", "marker {}", i);
        if (harness.GetLog().Flush())
            ++completed;
        std::vector<std::string> const markers = harness.Store().Texts("Marker");
        ASSERT_EQ(markers.size(), static_cast<std::size_t>(i + 1));
        EXPECT_EQ(markers.back(), "marker " + std::to_string(i));
    }
    stop = true;
    for (std::thread& producer : producers)
        producer.join();
    EXPECT_EQ(completed, 50);
    EXPECT_EQ(harness.GetLog().GetStatistics().FlushTimeouts, 0u);
}

TEST(LogAsyncTest, FlushWaitsForLinesQueuedBeforeIt)
{
    LogTestHarness harness;
    harness.ApplyOrFail(AsyncBody);
    TestAppenderStore& store = harness.Store();
    store.CloseGate();
    for (int i = 0; i < 500; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", i);
    std::thread opener([&store]
    {
        store.WaitForGateWaiters(1, std::chrono::seconds(30));
        store.OpenGate();
    });
    ASSERT_TRUE(harness.GetLog().Flush());
    EXPECT_EQ(store.Count("A"), 500u);
    opener.join();
}

TEST(LogAsyncTest, ShutdownDrainsEveryQueuedLine)
{
    LogTestHarness harness;
    harness.ApplyOrFail(AsyncBody);
    TestAppenderStore& store = harness.Store();
    store.CloseGate();
    for (int i = 0; i < 10000; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", i);
    std::future<void> shutdown = std::async(std::launch::async, [&harness] { harness.GetLog().Shutdown(); });
    ASSERT_TRUE(store.WaitForGateWaiters(1, std::chrono::seconds(30)));
    EXPECT_EQ(shutdown.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    store.OpenGate();
    shutdown.get();
    std::vector<std::string> const texts = store.Texts("A");
    ASSERT_EQ(texts.size(), 10000u);
    for (int i = 0; i < 10000; ++i)
        ASSERT_EQ(texts[static_cast<std::size_t>(i)], std::to_string(i));
    EXPECT_TRUE(harness.GetLog().IsShutdown());
    EXPECT_FALSE(harness.GetLog().GetStatistics().Async);
}

TEST(LogAsyncTest, LinesLoggedDuringShutdownAreKeptInOrder)
{
    LogTestHarness harness;
    harness.ApplyOrFail(AsyncBody);
    std::atomic<int> started{ 0 };
    std::vector<std::thread> producers;
    for (int thread = 0; thread < 4; ++thread)
    {
        producers.emplace_back([&harness, &started, thread]
        {
            for (int n = 0; n < 5000; ++n)
            {
                if (n == 100)
                    ++started;
                AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server.producer", "t{} n{}", thread, n);
            }
        });
    }
    while (started.load() < 4)
        std::this_thread::yield();
    harness.GetLog().Shutdown();
    for (std::thread& producer : producers)
        producer.join();
    std::vector<LogMessage> const messages = harness.Store().Messages("A");
    EXPECT_EQ(messages.size(), 20000u);
    ExpectEveryThreadInOrder(messages, 4, 5000);
}

TEST(LogAsyncTest, LogAfterShutdownWritesSynchronouslyAndFlushes)
{
    LogTestHarness harness;
    harness.ApplyOrFail(AsyncBody);
    harness.GetLog().Shutdown();
    uint32 const flushes = harness.Store().FlushCount("A");
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "late line");
    EXPECT_EQ(harness.Store().Texts("A"), std::vector<std::string>{ "late line" });
    EXPECT_EQ(harness.Store().FlushCount("A"), flushes + 1);
    harness.ApplyOrFail(AsyncBody);
    EXPECT_FALSE(harness.GetLog().GetStatistics().Async);
}

TEST(LogAsyncTest, FullQueueWaitBlocksProducerNotDrops)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Log.Async.Enable = 1\nLog.Async.QueueSize = 1024\nAppender.A = 200,1,0\nLogger.root = 1,A\n");
    TestAppenderStore& store = harness.Store();
    store.CloseGate();
    std::thread producer([&harness] { Produce(harness.GetLog(), 0, 5000); });
    ASSERT_TRUE(store.WaitForGateWaiters(1, std::chrono::seconds(30)));
    store.OpenGate();
    producer.join();
    ASSERT_TRUE(harness.GetLog().Flush());
    EXPECT_EQ(store.Count("A"), 5000u);
    LogStatistics const statistics = harness.GetLog().GetStatistics();
    EXPECT_EQ(statistics.AsyncDropped, 0u);
    EXPECT_LE(statistics.AsyncHighWater, 1024u);
    ExpectEveryThreadInOrder(store.Messages("A"), 1, 5000);
}

TEST(LogAsyncTest, FullQueueDropReportsCount)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Log.Async.Enable = 1\nLog.Async.QueueSize = 1024\nLog.Async.QueueFull = 1\nAppender.A = 200,1,0\nLogger.root = 1,A\n");
    TestAppenderStore& store = harness.Store();
    store.CloseGate();
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "first");
    ASSERT_TRUE(store.WaitForGateWaiters(1, std::chrono::seconds(30)));
    for (int i = 0; i < 5000; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", i);
    uint64 const dropped = harness.GetLog().GetStatistics().AsyncDropped;
    EXPECT_EQ(dropped, 5000u - 1024u);
    store.OpenGate();
    ASSERT_TRUE(harness.GetLog().Flush());
    uint64 reported = 0;
    for (LogMessage const& message : store.Messages("A"))
    {
        std::string_view const text = message.Text;
        std::size_t const end = text.find(" log lines");
        std::optional<uint64> const count = text.rfind("dropped ", 0) == 0 && end != std::string_view::npos ? Ambrose::StringTo<uint64>(text.substr(8, end - 8)) : std::nullopt;
        if (message.Category == "server.logging" && count)
        {
            EXPECT_EQ(message.Level, LogLevel::Warn);
            reported += *count;
        }
    }
    EXPECT_EQ(reported, dropped);
    EXPECT_EQ(store.Count("A"), 1u + 1024u + 1u);
}

TEST(LogAsyncTest, WorkerThreadNestedLogDoesNotDeadlock)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Log.Async.Enable = 1\nAppender.A = 200,1,0\nAppender.B = 200,1,0\nLogger.root = 1,A B\n");
    Log& log = harness.GetLog();
    harness.Store().SetOnWrite([&log](std::string const& appender, LogMessage const& message)
    {
        if (appender == "A" && message.Text == "outer")
        {
            AMBROSE_LOG(log, LogLevel::Warn, "server.nested", "inner");
            log.Flush();
        }
    });
    AMBROSE_LOG(log, LogLevel::Info, "server", "outer");
    ASSERT_TRUE(log.Flush());
    EXPECT_EQ(harness.Store().Texts("A"), std::vector<std::string>{ "outer" });
    EXPECT_EQ(harness.Store().Texts("B"), (std::vector<std::string>{ "outer", "inner" }));
}

TEST(LogAsyncTest, WorkerThreadCannotSwitchAsyncMode)
{
    LogTestHarness harness;
    harness.ApplyOrFail(AsyncBody);
    Log& log = harness.GetLog();
    LogSettings const syncSettings = LogTestConfig::Settings("Appender.A = 200,1,0\nLogger.root = 1,A\n");
    std::atomic<bool> rejected{ false };
    harness.Store().SetOnWrite([&log, &syncSettings, &rejected](std::string const&, LogMessage const& message)
    {
        if (message.Text == "switch")
            rejected = !log.Apply(syncSettings).Succeeded();
    });
    AMBROSE_LOG(log, LogLevel::Info, "server", "switch");
    ASSERT_TRUE(log.Flush());
    EXPECT_TRUE(rejected.load());
    EXPECT_TRUE(log.GetStatistics().Async);
    harness.Store().SetOnWrite({});
}

TEST(LogAsyncTest, ToggleAsyncUnderLoadExactlyOnce)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 1,A\n");
    std::atomic<int> finished{ 0 };
    std::vector<std::thread> producers;
    for (int thread = 0; thread < 4; ++thread)
    {
        producers.emplace_back([&harness, &finished, thread]
        {
            Produce(harness.GetLog(), thread, 20000);
            ++finished;
        });
    }
    int toggles = 0;
    while (finished.load() < 4 || toggles < 20)
    {
        bool const async = toggles % 2 == 0;
        std::string const size = toggles % 4 < 2 ? "1024" : "4096";
        harness.ApplyOrFail("Log.Async.Enable = " + std::string(async ? "1" : "0") + "\nLog.Async.QueueSize = " + size + "\nAppender.A = 200,1,0\nLogger.root = 1,A\n");
        ++toggles;
    }
    for (std::thread& producer : producers)
        producer.join();
    harness.GetLog().Shutdown();
    std::vector<LogMessage> const messages = harness.Store().Messages("A");
    EXPECT_EQ(messages.size(), 80000u);
    ExpectEveryThreadInOrder(messages, 4, 20000);
}
