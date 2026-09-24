/*
 * Project Ambrose by Imjustchico
 * Tests reloads and async toggles under concurrent logging with no lost or reordered lines.
 */

#include "Log.h"
#include "LogTestHarness.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <map>
#include <set>
#include <thread>

namespace
{
    std::string Utf8(std::filesystem::path const& path)
    {
        return ConfigMgr::PathToUtf8(path);
    }

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

    void ExpectExactlyOnceInOrder(std::vector<std::string> const& lines, int threads, int perThread, std::string const& source)
    {
        std::map<int, int> next;
        for (std::string const& line : lines)
        {
            int thread = 0;
            int number = 0;
            if (!ParseNumbered(line, thread, number))
                continue;
            ASSERT_EQ(number, next[thread]) << source << " thread " << thread;
            next[thread] = number + 1;
        }
        ASSERT_EQ(static_cast<int>(next.size()), threads) << source;
        for (auto const& [thread, count] : next)
            EXPECT_EQ(count, perThread) << source << " thread " << thread;
    }

    std::string ReloadBody(std::filesystem::path const& logsDir, int round)
    {
        return fmt::format(
            "LogsDir = {}\n"
            "Log.Async.Enable = {}\n"
            "Log.Async.QueueSize = {}\n"
            "Appender.Server = 2,1,0,Server.log,w,{}\n"
            "Appender.Capture = 200,1,0\n"
            "Appender.Other = 200,{},0\n"
            "Logger.root = 1,Server Capture\n"
            "Logger.other = {},Other\n",
            Utf8(logsDir), round % 3 == 1 ? 1 : 0, round % 5 < 2 ? 1024 : 65536, round % 2 == 0 ? "0" : "64M", round % 2 + 1, round % 2 == 0 ? 4 : 1);
    }

    class LogReloadTest : public testing::TestWithParam<bool>
    {
    };
}

TEST_P(LogReloadTest, ReloadUnderConcurrentLoggingLosesNothing)
{
    constexpr int Threads = 8;
    constexpr int PerThread = 10000;
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    LogTestHarness harness;
    harness.ApplyOrFail(ReloadBody(logsDir, GetParam() ? 1 : 0));
    ASSERT_EQ(harness.GetLog().GetStatistics().Async, GetParam());

    std::atomic<int> finished{ 0 };
    std::vector<std::thread> producers;
    for (int thread = 0; thread < Threads; ++thread)
    {
        producers.emplace_back([&harness, &finished, thread]
        {
            for (int n = 0; n < PerThread; ++n)
                AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "test.a", "t{} n{}", thread, n);
            ++finished;
        });
    }
    int reloads = 0;
    while (finished.load() < Threads || reloads < 100)
    {
        harness.ApplyOrFail(ReloadBody(logsDir, reloads + (GetParam() ? 1 : 0)));
        ++reloads;
    }
    for (std::thread& producer : producers)
        producer.join();
    harness.GetLog().Shutdown();

    ExpectExactlyOnceInOrder(harness.Store().Texts("Capture"), Threads, PerThread, "capture");
    ExpectExactlyOnceInOrder(directory.ReadLines(logsDir / "Server.log"), Threads, PerThread, "Server.log");
    EXPECT_GE(reloads, 100);
}

INSTANTIATE_TEST_SUITE_P(SyncAndAsyncStart, LogReloadTest, testing::Bool());

TEST(LogReloadTestSingle, ReloadReplacingAppenderSplitsCleanly)
{
    constexpr int Threads = 4;
    constexpr int PerThread = 5000;
    LogTestDirectory directory;
    std::filesystem::path const logsDir = directory.Path() / "logs";
    LogTestHarness harness;
    auto body = [&logsDir](char const* file)
    {
        return fmt::format("LogsDir = {}\nLog.Async.Enable = 1\nAppender.A = 2,1,0,{},w\nLogger.root = 1,A\n", Utf8(logsDir), file);
    };
    harness.ApplyOrFail(body("a.log"));
    std::atomic<int> halfway{ 0 };
    std::atomic<bool> reloaded{ false };
    std::vector<std::thread> producers;
    for (int thread = 0; thread < Threads; ++thread)
    {
        producers.emplace_back([&harness, &halfway, &reloaded, thread]
        {
            for (int n = 0; n < PerThread; ++n)
            {
                if (n == PerThread / 2)
                {
                    ++halfway;
                    while (!reloaded.load())
                        std::this_thread::yield();
                }
                AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "test.split", "t{} n{}", thread, n);
            }
        });
    }
    while (halfway.load() < Threads)
        std::this_thread::yield();
    harness.ApplyOrFail(body("a2.log"));
    reloaded = true;
    for (std::thread& producer : producers)
        producer.join();
    harness.GetLog().Shutdown();

    std::vector<std::string> const first = directory.ReadLines(logsDir / "a.log");
    std::vector<std::string> const second = directory.ReadLines(logsDir / "a2.log");
    ASSERT_EQ(first.size(), static_cast<std::size_t>(Threads * PerThread / 2));
    ASSERT_EQ(second.size(), static_cast<std::size_t>(Threads * PerThread / 2));
    std::vector<std::string> ordered;
    for (int thread = 0; thread < Threads; ++thread)
    {
        for (std::vector<std::string> const* lines : { &first, &second })
        {
            for (std::string const& line : *lines)
            {
                int parsedThread = 0;
                int number = 0;
                if (!ParseNumbered(line, parsedThread, number) || parsedThread != thread)
                    continue;
                EXPECT_EQ(number < PerThread / 2, lines == &first) << line;
                ordered.push_back(line);
            }
        }
    }
    ExpectExactlyOnceInOrder(ordered, Threads, PerThread, "a.log then a2.log");
}

TEST(LogReloadTestSingle, ReloadFailureKeepsLogging)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    LogConfigResult const failed = harness.Apply("Appender.Capture = 200,1,0\nLogger.server = 1,Capture\n");
    EXPECT_FALSE(failed.Succeeded());
    AMBROSE_LOG(harness.GetLog(), LogLevel::Debug, "anything", "still routed");
    EXPECT_EQ(harness.Store().Texts("Capture"), std::vector<std::string>{ "still routed" });
}

TEST(LogReloadTestSingle, LoadFromConfigAppliesAndReportsIssues)
{
    LogTestDirectory directory;
    LogTestHarness harness;
    std::unique_ptr<ConfigMgr> const config = LogTestConfig::Load(directory, "Log.Bogus = 1\nAppender.Capture = 200,2,0\nLogger.root = 2,Capture\n");
    LogConfigResult const result = harness.GetLog().LoadFromConfig(*config);
    ASSERT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
    ASSERT_EQ(result.Warnings.size(), 1u);
    EXPECT_NE(result.Warnings[0].Message.find("Log.Bogus"), std::string::npos);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Debug, "server", "loaded");
    EXPECT_EQ(harness.Store().Count("Capture"), 1u);

    std::unique_ptr<ConfigMgr> const broken = LogTestConfig::Load(directory, "Appender.Capture = 200,2,0\nLogger.root = 9,Capture\n");
    LogConfigResult const failed = harness.GetLog().LoadFromConfig(*broken);
    EXPECT_FALSE(failed.Succeeded());
    AMBROSE_LOG(harness.GetLog(), LogLevel::Debug, "server", "still loaded");
    EXPECT_EQ(harness.Store().Count("Capture"), 2u);
}

TEST(LogReloadTestSingle, ConfigWarningSinkAndReloadDoNotDeadlock)
{
    LogTestDirectory directory;
    LogTestHarness harness;
    std::unique_ptr<ConfigMgr> const config = LogTestConfig::Load(directory, "Log.Async.Enable = 1\nAppender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    ASSERT_TRUE(harness.GetLog().LoadFromConfig(*config).Succeeded());
    harness.GetLog().AttachConfigWarnings(*config);

    std::future<void> work = std::async(std::launch::async, [&harness, &config]
    {
        std::thread reloader([&harness, &config]
        {
            for (int i = 0; i < 300; ++i)
                harness.GetLog().LoadFromConfig(*config);
        });
        std::thread warner([&config]
        {
            for (int i = 0; i < 2000; ++i)
                config->GetOption<uint32>("Missing.Option" + std::to_string(i), 0);
        });
        std::thread logger([&harness]
        {
            for (int i = 0; i < 2000; ++i)
                AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "line {}", i);
        });
        reloader.join();
        warner.join();
        logger.join();
    });
    if (work.wait_for(std::chrono::seconds(120)) != std::future_status::ready)
    {
        std::fprintf(stderr, "ConfigWarningSinkAndReloadDoNotDeadlock appears deadlocked\n");
        std::abort();
    }
    work.get();
    harness.GetLog().DetachConfigWarnings();
    ASSERT_TRUE(harness.GetLog().Flush());
    std::size_t warnings = 0;
    for (LogMessage const& message : harness.Store().Messages("Capture"))
        if (message.Category == "server.config")
            ++warnings;
    EXPECT_EQ(warnings, 2000u);
}
