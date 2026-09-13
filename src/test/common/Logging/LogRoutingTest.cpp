/*
 * Project Ambrose by Imjustchico
 * Tests hierarchy, effective levels, site caching across reloads and instances, and the Warn-versus-Info acceptance.
 */

#include "Log.h"
#include "LogRouting.h"
#include "LogTestHarness.h"

#include <gtest/gtest.h>

namespace
{
    class SLogRoutingTest : public testing::TestWithParam<bool>
    {
    protected:
        void SetUp() override
        {
            _store = std::make_shared<TestAppenderStore>();
            LogConfigResult const registered = sLog.RegisterAppenderType(TestAppender::GetTypeInfo(_store));
            ASSERT_TRUE(registered.Succeeded()) << LogTestConfig::Describe(registered);
        }

        void TearDown() override
        {
            sLog.Reset();
        }

        std::shared_ptr<TestAppenderStore> _store;
    };

    std::vector<std::pair<std::string, std::string>> CategoriesAndTexts(std::vector<LogMessage> const& messages)
    {
        std::vector<std::pair<std::string, std::string>> result;
        for (LogMessage const& message : messages)
            result.emplace_back(message.Category, message.Text);
        return result;
    }

    void LogDebugAtFixedSite(Log& log, int& evaluations)
    {
        AMBROSE_LOG(log, LogLevel::Debug, "server.cached", "debug {}", ++evaluations);
    }

    void LogInfoAtSharedSite(Log& log)
    {
        AMBROSE_LOG(log, LogLevel::Info, "server.shared", "shared");
    }
}

TEST_P(SLogRoutingTest, SqlAtWarnDropsInfoWhileRootInfoPrintsServer)
{
    LogConfigResult const applied = sLog.Apply(LogTestConfig::Settings(fmt::format("Log.Async.Enable = {}\nAppender.Capture = 200,1,0\nLogger.root = 3,Capture\nLogger.sql.sql = 4,Capture\n", GetParam() ? 1 : 0)));
    ASSERT_TRUE(applied.Succeeded()) << LogTestConfig::Describe(applied);
    EXPECT_EQ(sLog.GetStatistics().Async, GetParam());

    LOG_INFO("sql.sql", "select {}", 1);
    LOG_WARN("sql.sql", "slow query {}", 2);
    LOG_INFO("server.gameserver", "realm {} online", "Ravenwood");
    LOG_INFO("server", "plain server");
    LOG_DEBUG("server.x", "hidden");
    ASSERT_TRUE(sLog.Flush());

    std::vector<std::pair<std::string, std::string>> const expected{
        { "sql.sql", "slow query 2" },
        { "server.gameserver", "realm Ravenwood online" },
        { "server", "plain server" }
    };
    EXPECT_EQ(CategoriesAndTexts(_store->Messages("Capture")), expected);
    EXPECT_EQ(_store->TotalCount(), 3u);
}

INSTANTIATE_TEST_SUITE_P(SyncAndAsync, SLogRoutingTest, testing::Bool());

TEST(LogRoutingTest, NearestParentLoggerWins)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nAppender.B = 200,1,0\nAppender.C = 200,1,0\nLogger.root = 3,A\nLogger.sql = 4,B\nLogger.sql.sql = 2,C\n");
    Log& log = harness.GetLog();
    AMBROSE_LOG(log, LogLevel::Debug, "sql.sql.deep", "to C");
    AMBROSE_LOG(log, LogLevel::Warn, "sql.other", "to B");
    AMBROSE_LOG(log, LogLevel::Info, "sql.other", "dropped by sql");
    AMBROSE_LOG(log, LogLevel::Info, "sqlx", "to A");
    AMBROSE_LOG(log, LogLevel::Info, "", "empty to A");
    EXPECT_EQ(harness.Store().Texts("C"), std::vector<std::string>{ "to C" });
    EXPECT_EQ(harness.Store().Texts("B"), std::vector<std::string>{ "to B" });
    EXPECT_EQ(harness.Store().Texts("A"), (std::vector<std::string>{ "to A", "empty to A" }));
}

TEST(LogRoutingTest, DisabledLoggerSilencesUnconfiguredChildren)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 1,A\nLogger.network = 0,A\n");
    Log& log = harness.GetLog();
    AMBROSE_LOG(log, LogLevel::Fatal, "network.opcode", "silenced");
    AMBROSE_LOG(log, LogLevel::Trace, "networking", "root");
    EXPECT_EQ(harness.Store().Texts("A"), std::vector<std::string>{ "root" });
    EXPECT_EQ(log.GetEffectiveLevel("network.opcode"), LogLevel::Disabled);
}

TEST(LogRoutingTest, AppenderLevelFiltersAfterLogger)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nAppender.B = 200,4,0\nLogger.root = 1,A B\n");
    Log& log = harness.GetLog();
    AMBROSE_LOG(log, LogLevel::Info, "server", "info");
    AMBROSE_LOG(log, LogLevel::Error, "server", "error");
    EXPECT_EQ(harness.Store().Texts("A"), (std::vector<std::string>{ "info", "error" }));
    EXPECT_EQ(harness.Store().Texts("B"), std::vector<std::string>{ "error" });
}

TEST(LogRoutingTest, EffectiveLevelIncludesAppenderLevels)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,3,0\nAppender.Off = 200,0,0\nLogger.root = 2,A Off\nLogger.muted = 1,Off\nLogger.trace = 1,A Off\n");
    Log& log = harness.GetLog();
    EXPECT_EQ(log.GetEffectiveLevel("server"), LogLevel::Info);
    EXPECT_EQ(log.GetEffectiveLevel("muted"), LogLevel::Disabled);
    EXPECT_EQ(log.GetEffectiveLevel("trace.x"), LogLevel::Info);
    int evaluations = 0;
    AMBROSE_LOG(log, LogLevel::Debug, "server", "{}", ++evaluations);
    AMBROSE_LOG(log, LogLevel::Fatal, "muted", "{}", ++evaluations);
    EXPECT_EQ(evaluations, 0);
    EXPECT_EQ(harness.Store().TotalCount(), 0u);
}

TEST(LogRoutingTest, SiteCacheRefreshesAfterReload)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 3,A\n");
    int evaluations = 0;
    LogDebugAtFixedSite(harness.GetLog(), evaluations);
    EXPECT_EQ(evaluations, 0);
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 2,A\n");
    LogDebugAtFixedSite(harness.GetLog(), evaluations);
    EXPECT_EQ(evaluations, 1);
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 3,A\nLogger.server.cached = 1,A\nLogger.other = 4,A\n");
    LogDebugAtFixedSite(harness.GetLog(), evaluations);
    EXPECT_EQ(evaluations, 2);
    EXPECT_EQ(harness.Store().Texts("A"), (std::vector<std::string>{ "debug 1", "debug 2" }));
}

TEST(LogRoutingTest, SiteCacheIsSafeAcrossInstances)
{
    LogTestHarness first;
    LogTestHarness second;
    first.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 3,A\n");
    second.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 4,A\n");
    LogInfoAtSharedSite(first.GetLog());
    LogInfoAtSharedSite(second.GetLog());
    LogInfoAtSharedSite(first.GetLog());
    LogInfoAtSharedSite(second.GetLog());
    EXPECT_EQ(first.Store().Count("A"), 2u);
    EXPECT_EQ(second.Store().Count("A"), 0u);
}

TEST(LogRoutingTest, SetLoggerLevelPublishesGenerationAndIsLostOnReload)
{
    LogTestHarness harness;
    std::string const body = "Appender.A = 200,1,0\nLogger.root = 3,A\nLogger.low = 1,A\n";
    harness.ApplyOrFail(body);
    Log& log = harness.GetLog();
    static constinit LogSite const site{ "server.generation" };
    uint64 const before = log.GetGeneration();
    EXPECT_FALSE(log.ShouldLog(site, LogLevel::Debug));
    EXPECT_EQ(LogSite::GenerationOf(site.LoadCache()), before);

    ASSERT_TRUE(log.SetLoggerLevel("root", LogLevel::Debug));
    uint64 const after = log.GetGeneration();
    EXPECT_GT(after, before);
    EXPECT_TRUE(log.ShouldLog(site, LogLevel::Debug));
    EXPECT_EQ(LogSite::GenerationOf(site.LoadCache()), after);
    EXPECT_EQ(LogSite::LoggerIndexOf(site.LoadCache()), LogRouting::RootIndex);
    EXPECT_EQ(LogSite::LevelOf(site.LoadCache()), static_cast<uint8>(LogLevel::Debug));
    EXPECT_FALSE(log.SetLoggerLevel("missing", LogLevel::Debug));
    EXPECT_EQ(log.GetGeneration(), after);

    harness.ApplyOrFail(body);
    EXPECT_FALSE(log.ShouldLog(site, LogLevel::Debug));
    EXPECT_EQ(log.GetSettings().FindLogger("root")->Level, LogLevel::Info);
}

TEST(LogRoutingTest, SetAppenderLevelKeepsTheSameAppender)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 1,A\n");
    Log& log = harness.GetLog();
    std::shared_ptr<Appender> const before = log.GetAppender("A");
    ASSERT_TRUE(log.SetAppenderLevel("A", LogLevel::Error));
    EXPECT_EQ(log.GetAppender("A"), before);
    EXPECT_EQ(before->GetLevel(), LogLevel::Error);
    AMBROSE_LOG(log, LogLevel::Warn, "server", "filtered");
    AMBROSE_LOG(log, LogLevel::Error, "server", "kept");
    EXPECT_EQ(harness.Store().Texts("A"), std::vector<std::string>{ "kept" });
}

TEST(LogRoutingTest, DynamicCategoryRoutesLikeConstant)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nAppender.B = 200,1,0\nLogger.root = 3,A\nLogger.network = 1,B\n");
    Log& log = harness.GetLog();
    std::string const category = std::string("network.") + "opcode";
    EXPECT_TRUE(log.ShouldLog(std::string_view(category), LogLevel::Trace));
    EXPECT_FALSE(log.ShouldLog(std::string_view("server"), LogLevel::Debug));
    log.Write(std::string_view(category), LogLevel::Trace, "dynamic {}", 7);
    AMBROSE_LOG(log, LogLevel::Trace, "network.opcode", "constant {}", 7);
    std::vector<LogMessage> const messages = harness.Store().Messages("B");
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0].Category, "network.opcode");
    EXPECT_EQ(messages[0].Text, "dynamic 7");
    EXPECT_EQ(messages[1].Text, "constant 7");
    EXPECT_LT(messages[0].Sequence, messages[1].Sequence);
}
