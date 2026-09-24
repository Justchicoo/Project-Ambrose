/*
 * Project Ambrose by Imjustchico
 * Tests argument evaluation, allocation-free disabled paths, runtime categories, format errors and fatal flushing.
 */

#include "AllocationCounter.h"
#include "Log.h"
#include "LogTestHarness.h"

#include <gtest/gtest.h>

#include <thread>

namespace
{
    struct CountedFormat
    {
        int* Calls;
    };

    void LogDisabledAtWarmSite(Log& log)
    {
        AMBROSE_LOG(log, LogLevel::Debug, "server.warm", "{}", std::string(4096, 'x'));
    }

    void LogDisabledAtColdSite(Log& log)
    {
        AMBROSE_LOG(log, LogLevel::Debug, "server.cold", "{}", std::string(4096, 'x'));
    }

    class SLogMacroTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _store = std::make_shared<TestAppenderStore>();
            ASSERT_TRUE(sLog.RegisterAppenderType(TestAppender::GetTypeInfo(_store)).Succeeded());
            ASSERT_TRUE(sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n")).Succeeded());
        }

        void TearDown() override
        {
            sLog.Reset();
        }

        std::shared_ptr<TestAppenderStore> _store;
    };
}

template<>
struct fmt::formatter<CountedFormat> : fmt::formatter<std::string_view>
{
    auto format(CountedFormat const& value, fmt::format_context& context) const
    {
        ++*value.Calls;
        return fmt::formatter<std::string_view>::format("counted", context);
    }
};

TEST(LogMacroTest, DisabledLevelDoesNotEvaluateArguments)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 3,A\nLogger.other = 1,A\n");
    int evaluations = 0;
    AMBROSE_LOG(harness.GetLog(), LogLevel::Debug, "server", "{}", ++evaluations);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Trace, "server", "{}", ++evaluations);
    EXPECT_EQ(evaluations, 0);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", ++evaluations);
    EXPECT_EQ(evaluations, 1);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Trace, "other", "{}", ++evaluations);
    EXPECT_EQ(evaluations, 2);
}

TEST(LogMacroTest, DisabledPathAllocatesNothing)
{
    if (!AllocationScope::IsSupported())
        GTEST_SKIP() << "allocation counting is not available in this build";
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 3,A\nLogger.other = 1,A\n");
    Log& log = harness.GetLog();
    LogDisabledAtWarmSite(log);

    std::size_t warm = 0;
    {
        AllocationScope scope;
        for (int i = 0; i < 1000; ++i)
            LogDisabledAtWarmSite(log);
        warm = scope.GetCount();
    }
    EXPECT_EQ(warm, 0u);

    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 3,A\nLogger.other = 2,A\n");
    std::size_t cold = 0;
    {
        AllocationScope scope;
        for (int i = 0; i < 1000; ++i)
        {
            LogDisabledAtColdSite(log);
            LogDisabledAtWarmSite(log);
        }
        cold = scope.GetCount();
    }
    EXPECT_EQ(cold, 0u);

    std::size_t enabled = 0;
    {
        AllocationScope scope;
        AMBROSE_LOG(log, LogLevel::Info, "server.enabled", "{}", 1);
        enabled = scope.GetCount();
    }
    EXPECT_GE(enabled, 1u);
    EXPECT_EQ(harness.Store().Count("A"), 1u);
}

TEST(LogMacroTest, EnabledPathFormatsOnce)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nAppender.B = 200,1,0\nAppender.C = 200,1,0\nLogger.root = 1,A B C\n");
    int calls = 0;
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "{}", CountedFormat{ &calls });
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(harness.Store().TotalCount(), 3u);
    EXPECT_EQ(harness.Store().Texts("C"), std::vector<std::string>{ "counted" });
}

TEST(LogMacroTest, RuntimeFormatThroughFmtRuntime)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 1,A\n");
    std::string const format = "{} + {} = {}";
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", fmt::runtime(format), 2, 2, 4);
    EXPECT_EQ(harness.Store().Texts("A"), std::vector<std::string>{ "2 + 2 = 4" });
}

TEST(LogMacroTest, FormatExceptionIsLoggedNotThrown)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 1,A\n");
    std::string const format = "{:d}";
    EXPECT_NO_THROW(AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", fmt::runtime(format), "text"));
    std::vector<std::string> const texts = harness.Store().Texts("A");
    ASSERT_EQ(texts.size(), 1u);
    EXPECT_EQ(texts[0].rfind("<format error", 0), 0u) << texts[0];
    EXPECT_NE(texts[0].find("{:d}"), std::string::npos);
    EXPECT_EQ(harness.GetLog().GetStatistics().FormatErrors, 1u);
}

TEST(LogMacroTest, FatalFlushesBeforeReturning)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Log.Async.Enable = 1\nAppender.A = 200,1,0\nLogger.root = 1,A\n");
    TestAppenderStore& store = harness.Store();
    store.CloseGate();
    for (int i = 0; i < 100; ++i)
        AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "queued {}", i);
    std::thread opener([&store]
    {
        store.WaitForGateWaiters(1, std::chrono::seconds(30));
        store.OpenGate();
    });
    AMBROSE_LOG(harness.GetLog(), LogLevel::Fatal, "server", "fatal");
    std::vector<std::string> const texts = store.Texts("A");
    opener.join();
    ASSERT_EQ(texts.size(), 101u);
    EXPECT_EQ(texts.back(), "fatal");
    EXPECT_GE(store.FlushCount("A"), 1u);
}

TEST_F(SLogMacroTest, DynamicFilterEvaluatedOnceAndTemporaryKeptAlive)
{
    int calls = 0;
    auto category = [&calls]
    {
        ++calls;
        return std::string("server.dynamic.") + std::to_string(calls);
    };
    LOG_DYNAMIC(LogLevel::Info, category(), "value {}", 5);
    EXPECT_EQ(calls, 1);
    std::vector<LogMessage> const messages = _store->Messages("Capture");
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].Category, "server.dynamic.1");
    EXPECT_EQ(messages[0].Text, "value 5");
    LOG_DYNAMIC(LogLevel::Debug, category(), "hidden");
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(_store->Count("Capture"), 1u);
}

TEST_F(SLogMacroTest, EveryLevelMacroRoutesThroughTheSingleton)
{
    ASSERT_TRUE(sLog.SetLoggerLevel("root", LogLevel::Trace));
    LOG_TRACE("server.macro", "trace");
    LOG_DEBUG("server.macro", "debug");
    LOG_INFO("server.macro", "info");
    LOG_WARN("server.macro", "warn");
    LOG_ERROR("server.macro", "error");
    LOG_FATAL("server.macro", "fatal");
    std::vector<LogMessage> const messages = _store->Messages("Capture");
    ASSERT_EQ(messages.size(), 6u);
    for (std::size_t i = 0; i < messages.size(); ++i)
        EXPECT_EQ(messages[i].Level, static_cast<LogLevel>(i + 1));
}

TEST_F(SLogMacroTest, WriteTextSkipsFormatting)
{
    sLog.WriteText("server.text", LogLevel::Info, "{not a format}");
    sLog.WriteText("server.text", LogLevel::Debug, "hidden");
    EXPECT_EQ(_store->Texts("Capture"), std::vector<std::string>{ "{not a format}" });
}
