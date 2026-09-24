/*
 * Project Ambrose by Imjustchico
 * Tests pending types with replay, unregistration, excluded categories, nested delivery and config warning delivery.
 */

#include "Log.h"
#include "LogTestHarness.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>

namespace
{
    constexpr AppenderType LateType = static_cast<AppenderType>(201);

    bool AnyIssueContains(std::vector<ConfigIssue> const& issues, std::string_view text)
    {
        for (ConfigIssue const& issue : issues)
            if (issue.ToString().find(text) != std::string::npos)
                return true;
        return false;
    }
}

TEST(LogRegistryTest, PendingTypeBuffersAndReplaysInOrder)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Late = 201,2,0\nLogger.root = 2,Late\n");
    Log& log = harness.GetLog();
    EXPECT_EQ(log.GetPendingAppenderNames(), std::vector<std::string>{ "Late" });
    for (int i = 1; i <= 5; ++i)
        AMBROSE_LOG(log, LogLevel::Info, "server", "p{}", i);
    AMBROSE_LOG(log, LogLevel::Trace, "server", "below level");
    EXPECT_EQ(harness.Store().Count("Late"), 0u);

    auto late = std::make_shared<TestAppenderStore>();
    LogConfigResult const registered = log.RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType));
    ASSERT_TRUE(registered.Succeeded()) << LogTestConfig::Describe(registered);
    EXPECT_TRUE(log.IsAppenderTypeRegistered(LateType));
    EXPECT_TRUE(log.GetPendingAppenderNames().empty());
    AMBROSE_LOG(log, LogLevel::Info, "server", "live");
    EXPECT_EQ(late->Texts("Late"), (std::vector<std::string>{ "p1", "p2", "p3", "p4", "p5", "live" }));
}

TEST(LogRegistryTest, PendingOverflowCountsDrops)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Log.PendingBuffer = 3\nAppender.Late = 201,2,0\nLogger.root = 2,Late\n");
    Log& log = harness.GetLog();
    for (int i = 1; i <= 5; ++i)
        AMBROSE_LOG(log, LogLevel::Info, "server", "p{}", i);
    EXPECT_EQ(log.GetStatistics().PendingDropped, 2u);
    auto late = std::make_shared<TestAppenderStore>();
    ASSERT_TRUE(log.RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType)).Succeeded());
    EXPECT_EQ(late->Texts("Late"), (std::vector<std::string>{ "p3", "p4", "p5" }));
}

TEST(LogRegistryTest, PendingBufferSurvivesUnrelatedReload)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Late = 201,2,0\nAppender.Capture = 200,1,0\nLogger.root = 2,Late Capture\n");
    Log& log = harness.GetLog();
    AMBROSE_LOG(log, LogLevel::Info, "server", "kept across reload");
    harness.ApplyOrFail("Appender.Late = 201,1,0\nAppender.Capture = 200,3,0\nLogger.root = 1,Late Capture\n");
    auto late = std::make_shared<TestAppenderStore>();
    ASSERT_TRUE(log.RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType)).Succeeded());
    EXPECT_EQ(late->Texts("Late"), std::vector<std::string>{ "kept across reload" });
}

TEST(LogRegistryTest, ReplayHonorsExcludedCategories)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Late = 201,1,0\nLogger.root = 1,Late\n");
    Log& log = harness.GetLog();
    AMBROSE_LOG(log, LogLevel::Error, "sql.sql", "sql failure");
    AMBROSE_LOG(log, LogLevel::Info, "server.x", "server line");
    auto late = std::make_shared<TestAppenderStore>();
    ASSERT_TRUE(log.RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType, { "sql" })).Succeeded());
    EXPECT_EQ(late->Texts("Late"), std::vector<std::string>{ "server line" });
}

TEST(LogRegistryTest, UnregisterWaitsForInFlightWrites)
{
    LogTestHarness harness;
    Log& log = harness.GetLog();
    auto late = std::make_shared<TestAppenderStore>();
    ASSERT_TRUE(log.RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType)).Succeeded());
    harness.ApplyOrFail("Appender.Late = 201,1,0\nLogger.root = 1,Late\n");
    late->CloseGate();
    std::thread writer([&log] { AMBROSE_LOG(log, LogLevel::Info, "server", "in flight"); });
    ASSERT_TRUE(late->WaitForGateWaiters(1, std::chrono::seconds(30)));
    std::future<LogConfigResult> unregister = std::async(std::launch::async, [&log] { return log.UnregisterAppenderType(LateType); });
    EXPECT_EQ(unregister.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);
    late->OpenGate();
    LogConfigResult const result = unregister.get();
    writer.join();
    EXPECT_TRUE(result.Succeeded()) << LogTestConfig::Describe(result);
    EXPECT_EQ(late->Texts("Late"), std::vector<std::string>{ "in flight" });
    EXPECT_EQ(log.GetPendingAppenderNames(), std::vector<std::string>{ "Late" });
    AMBROSE_LOG(log, LogLevel::Info, "server", "after unregister");
    EXPECT_EQ(late->Count("Late"), 1u);
}

TEST(LogRegistryTest, BuiltInTypesCannotBeUnregistered)
{
    LogTestHarness harness;
    EXPECT_FALSE(harness.GetLog().UnregisterAppenderType(AppenderType::File).Succeeded());
    EXPECT_TRUE(harness.GetLog().IsAppenderTypeRegistered(AppenderType::File));
    EXPECT_FALSE(harness.GetLog().UnregisterAppenderType(static_cast<AppenderType>(250)).Succeeded());
}

TEST(LogRegistryTest, ExplicitExcludedRouteIsRegistrationError)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.DB = 201,2,0\nAppender.Capture = 200,1,0\nLogger.root = 2,Capture\nLogger.sql = 2,DB\n");
    auto late = std::make_shared<TestAppenderStore>();
    LogConfigResult const result = harness.GetLog().RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType, { "sql" }));
    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(AnyIssueContains(result.Errors, "Logger.sql: routes to appender 'DB', whose type never accepts category 'sql'")) << LogTestConfig::Describe(result);
    EXPECT_EQ(harness.GetLog().GetPendingAppenderNames(), std::vector<std::string>{ "DB" });
    EXPECT_FALSE(harness.GetLog().IsAppenderTypeRegistered(LateType));
    EXPECT_TRUE(harness.GetLog().RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType)).Succeeded());
}

TEST(LogRegistryTest, ReplayedAppenderMayLogWithoutDeadlock)
{
    LogTestHarness harness;
    Log& log = harness.GetLog();
    harness.ApplyOrFail("Appender.Late = 201,2,0\nAppender.Capture = 200,1,0\nLogger.root = 2,Late Capture\n");
    for (int i = 1; i <= 3; ++i)
        AMBROSE_LOG(log, LogLevel::Info, "server", "p{}", i);
    auto late = std::make_shared<TestAppenderStore>();
    late->SetOnWrite([&log](std::string const& appender, LogMessage const& message)
    {
        if (appender == "Late" && !message.Nested)
            AMBROSE_LOG(log, LogLevel::Warn, "server.replay", "replayed {}", message.Text);
    });
    std::future<LogConfigResult> registered = std::async(std::launch::async, [&log, &late] { return log.RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType)); });
    if (registered.wait_for(std::chrono::seconds(60)) != std::future_status::ready)
    {
        std::fprintf(stderr, "ReplayedAppenderMayLogWithoutDeadlock appears deadlocked\n");
        std::abort();
    }
    ASSERT_TRUE(registered.get().Succeeded());
    AMBROSE_LOG(log, LogLevel::Info, "server", "live");
    EXPECT_EQ(late->Texts("Late"), (std::vector<std::string>{ "p1", "p2", "p3", "live" }));
    std::vector<std::string> const captured = harness.Store().Texts("Capture");
    EXPECT_EQ(captured, (std::vector<std::string>{ "p1", "p2", "p3", "replayed p1", "replayed p2", "replayed p3", "live", "replayed live" }));
}

TEST(LogRegistryTest, RejectedReloadCreatesNothing)
{
    LogTestDirectory directory;
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    LogSettings settings = LogTestConfig::Settings(fmt::format("LogsDir = {}\nAppender.New = 2,2,0,New.log,w\nAppender.Capture = 200,1,0\nLogger.root = 1,Capture New\n", ConfigMgr::PathToUtf8(directory.Path() / "logs")));
    settings.Loggers.front().Appenders.push_back("Missing");
    EXPECT_FALSE(harness.GetLog().Apply(std::move(settings)).Succeeded());
    EXPECT_FALSE(std::filesystem::exists(directory.Path() / "logs" / "New.log"));
}

TEST(LogRegistryTest, InheritedRouteSkipsExcludedCategory)
{
    LogTestHarness harness;
    Log& log = harness.GetLog();
    auto excluded = std::make_shared<TestAppenderStore>();
    ASSERT_TRUE(log.RegisterAppenderType(TestAppender::GetTypeInfo(excluded, LateType, { "sql" })).Succeeded());
    harness.ApplyOrFail("Appender.Ex = 201,1,0\nAppender.Capture = 200,1,0\nLogger.root = 1,Ex Capture\n");
    AMBROSE_LOG(log, LogLevel::Error, "sql.sql", "query failed");
    AMBROSE_LOG(log, LogLevel::Error, "sqlite", "not sql");
    EXPECT_EQ(excluded->Texts("Ex"), std::vector<std::string>{ "not sql" });
    EXPECT_EQ(harness.Store().Texts("Capture"), (std::vector<std::string>{ "query failed", "not sql" }));
}

TEST(LogRegistryTest, NestedLogFromAppenderReachesOthersNotOrigin)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nAppender.B = 200,1,0\nLogger.root = 1,A B\n");
    Log& log = harness.GetLog();
    harness.Store().SetOnWrite([&log](std::string const& appender, LogMessage const& message)
    {
        if (appender == "A" && message.Text == "outer")
            AMBROSE_LOG(log, LogLevel::Warn, "server.nested", "nested from {}", appender);
    });
    AMBROSE_LOG(log, LogLevel::Info, "server", "outer");
    EXPECT_EQ(harness.Store().Texts("A"), std::vector<std::string>{ "outer" });
    std::vector<LogMessage> const b = harness.Store().Messages("B");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0].Text, "outer");
    EXPECT_FALSE(b[0].Nested);
    EXPECT_EQ(b[1].Text, "nested from A");
    EXPECT_TRUE(b[1].Nested);
    EXPECT_EQ(log.GetStatistics().NestedDeferred, 1u);
}

TEST(LogRegistryTest, NestedFromNonNestedAppenderSkipped)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nAppender.B = 200,1,0,nonested\nAppender.C = 200,1,0\nLogger.root = 1,A B C\n");
    Log& log = harness.GetLog();
    harness.Store().SetOnWrite([&log](std::string const& appender, LogMessage const& message)
    {
        if (appender == "A" && message.Text == "outer")
            AMBROSE_LOG(log, LogLevel::Warn, "server.nested", "nested");
    });
    AMBROSE_LOG(log, LogLevel::Info, "server", "outer");
    EXPECT_EQ(harness.Store().Texts("B"), std::vector<std::string>{ "outer" });
    EXPECT_EQ(harness.Store().Texts("C"), (std::vector<std::string>{ "outer", "nested" }));
}

TEST(LogRegistryTest, SecondGenerationNestedIsDropped)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nAppender.B = 200,1,0\nLogger.root = 1,A B\n");
    Log& log = harness.GetLog();
    harness.Store().SetOnWrite([&log](std::string const& appender, LogMessage const& message)
    {
        if (appender == "A" && message.Text == "outer")
            AMBROSE_LOG(log, LogLevel::Warn, "server.nested", "first generation");
        if (appender == "B" && message.Nested)
            AMBROSE_LOG(log, LogLevel::Warn, "server.nested", "second generation");
    });
    AMBROSE_LOG(log, LogLevel::Info, "server", "outer");
    EXPECT_EQ(harness.Store().Texts("B"), (std::vector<std::string>{ "outer", "first generation" }));
    EXPECT_EQ(harness.Store().Texts("A"), std::vector<std::string>{ "outer" });
    EXPECT_EQ(log.GetStatistics().NestedDropped, 1u);
    AMBROSE_LOG(log, LogLevel::Info, "server", "normal again");
    EXPECT_EQ(harness.Store().Count("A"), 2u);
}

TEST(LogRegistryTest, AppenderExceptionIsCountedNotPropagated)
{
    LogTestHarness harness;
    harness.ApplyOrFail("Appender.A = 200,1,0\nLogger.root = 1,A\n");
    harness.Store().SetThrowOnWrite(true);
    EXPECT_NO_THROW(AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "boom"));
    harness.Store().SetThrowOnWrite(false);
    AMBROSE_LOG(harness.GetLog(), LogLevel::Info, "server", "fine");
    EXPECT_EQ(harness.GetLog().GetStatistics().AppenderFailures, 1u);
    EXPECT_EQ(harness.Store().Texts("A"), std::vector<std::string>{ "fine" });
}

TEST(LogRegistryTest, DuplicateRegistrationFails)
{
    LogTestHarness harness;
    auto late = std::make_shared<TestAppenderStore>();
    EXPECT_TRUE(harness.GetLog().RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType)).Succeeded());
    LogConfigResult const second = harness.GetLog().RegisterAppenderType(TestAppender::GetTypeInfo(late, LateType));
    EXPECT_TRUE(AnyIssueContains(second.Errors, "already registered"));
    EXPECT_FALSE(harness.GetLog().RegisterAppenderType(TestAppender::GetTypeInfo(late, AppenderType::Console)).Succeeded());
}

TEST(LogRegistryTest, ConfigWarningsDrainIntoLog)
{
    LogTestDirectory directory;
    LogTestHarness harness;
    std::unique_ptr<ConfigMgr> const config = LogTestConfig::Load(directory, "Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
    ASSERT_TRUE(harness.GetLog().LoadFromConfig(*config).Succeeded());
    config->GetOption<uint32>("BeforeAttach", 0);
    harness.GetLog().AttachConfigWarnings(*config);
    config->GetOption<uint32>("AfterAttach", 0);
    std::vector<LogMessage> const messages = harness.Store().Messages("Capture");
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_NE(messages[0].Text.find("BeforeAttach"), std::string::npos);
    EXPECT_NE(messages[1].Text.find("AfterAttach"), std::string::npos);
    for (LogMessage const& message : messages)
    {
        EXPECT_EQ(message.Level, LogLevel::Warn);
        EXPECT_EQ(message.Category, "server.config");
    }
    harness.GetLog().DetachConfigWarnings();
    config->GetOption<uint32>("AfterDetach", 0);
    EXPECT_EQ(harness.Store().Count("Capture"), 2u);
    EXPECT_EQ(config->TakeWarnings().size(), 1u);
}
