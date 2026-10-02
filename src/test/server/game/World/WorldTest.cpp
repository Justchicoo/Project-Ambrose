/*
 * Project Ambrose by Imjustchico
 * Tests the world thread, which clearing the world forgets, the order of session and script work, named tick component timing and budgets, and bounded on-demand Chrome trace capture.
 */

#include "GameSession.h"
#include "MetricRegistry.h"
#include "ScriptMgr.h"
#include "SessionContext.h"
#include "World.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <string>
#include <vector>

namespace
{
    class WorldTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sWorld.Clear();
            sScriptMgr.Unload();
            _context = std::make_shared<SessionContext>(SessionSettings{});
        }

        void TearDown() override
        {
            sWorld.Clear();
            sScriptMgr.Unload();
        }

        std::shared_ptr<GameSession> MakeSession()
        {
            return std::make_shared<GameSession>(asio::ip::tcp::socket(_io), FrameLimits{}, _context);
        }

        asio::io_context _io;
        std::shared_ptr<SessionContext> _context;
    };

    class RecordingScript : public WorldScript
    {
    public:
        RecordingScript(std::vector<std::string>& log) : WorldScript("recording"), _log(log) {}

        void OnUpdate(std::chrono::milliseconds) override { _log.push_back("script"); }

    private:
        std::vector<std::string>& _log;
    };

    std::optional<Ambrose::MetricEntry> FindMetric(std::string_view name, std::string_view component)
    {
        std::vector<Ambrose::MetricEntry> const entries = sMetrics.Collect();
        for (Ambrose::MetricEntry const& entry : entries)
            if (entry.Name == name && std::find(entry.Labels.begin(), entry.Labels.end(),
                std::pair<std::string, std::string>{ "component", std::string(component) }) != entry.Labels.end())
                return entry;
        return std::nullopt;
    }

    double HistogramSum(std::string_view name)
    {
        std::vector<Ambrose::MetricEntry> const entries = sMetrics.Collect();
        for (Ambrose::MetricEntry const& entry : entries)
            if (entry.Name == name && entry.AsHistogram)
                return entry.AsHistogram->Sum();
        return 0.0;
    }
}

TEST_F(WorldTest, QueuedWorkRunsOnTheThreadThatCallsUpdate)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    session->SetStatus(SessionStatus::Authenticated);
    sWorld.AddSession(session);
    ASSERT_EQ(sWorld.GetSessionCount(), 1u);

    std::atomic<bool> ran{ false };
    std::thread::id queued{};
    std::thread::id drained{};
    std::thread other([&]
    {
        queued = std::this_thread::get_id();
        ASSERT_TRUE(session->QueueInbound([&]
        {
            drained = std::this_thread::get_id();
            ran.store(true);
        }));
    });
    other.join();

    EXPECT_FALSE(ran.load()) << "the work ran before the world asked for it";
    EXPECT_EQ(session->GetQueuedMessageCount(), 1u);

    std::thread::id const worldThread = std::this_thread::get_id();
    sWorld.Update(std::chrono::milliseconds(50));

    EXPECT_TRUE(ran.load());
    EXPECT_EQ(drained, worldThread) << "the queued work did not run on the thread that called Update";
    EXPECT_NE(drained, queued) << "the queued work ran on the thread that queued it";
    EXPECT_EQ(sWorld.GetWorldThreadId(), worldThread);
    EXPECT_TRUE(sWorld.IsWorldThread());
    EXPECT_EQ(session->GetQueuedMessageCount(), 0u);
}

TEST_F(WorldTest, TheWorldThreadIsTheOneThatCalledUpdateAndNoOther)
{
    sWorld.Update(std::chrono::milliseconds(1));
    std::thread::id const worldThread = std::this_thread::get_id();
    EXPECT_TRUE(sWorld.IsWorldThread());

    std::atomic<bool> elsewhere{ true };
    std::thread other([&] { elsewhere.store(sWorld.IsWorldThread()); });
    other.join();
    EXPECT_FALSE(elsewhere.load()) << "another thread believed it was the world thread";
    EXPECT_EQ(sWorld.GetWorldThreadId(), worldThread);
}

TEST_F(WorldTest, ClearingTheWorldForgetsItsThread)
{
    sWorld.Update(std::chrono::milliseconds(1));
    ASSERT_TRUE(sWorld.IsWorldThread());
    sWorld.Clear();
    EXPECT_FALSE(sWorld.IsWorldThread()) << "a thread given the old world thread's id would run world work inline with no world thread running";
    EXPECT_EQ(sWorld.GetWorldThreadId(), std::thread::id());
}

TEST_F(WorldTest, EverySessionIsDrainedBeforeTheScriptsRun)
{
    std::vector<std::string> log;
    new RecordingScript(log);

    std::shared_ptr<GameSession> const first = MakeSession();
    std::shared_ptr<GameSession> const second = MakeSession();
    first->SetStatus(SessionStatus::Authenticated);
    second->SetStatus(SessionStatus::Authenticated);
    sWorld.AddSession(first);
    sWorld.AddSession(second);

    ASSERT_TRUE(first->QueueInbound([&] { log.push_back("first"); }));
    ASSERT_TRUE(second->QueueInbound([&] { log.push_back("second"); }));

    uint64 const before = sWorld.GetTickCount();
    sWorld.Update(std::chrono::milliseconds(50));
    EXPECT_EQ(sWorld.GetTickCount(), before + 1);
    EXPECT_EQ(log, (std::vector<std::string>{ "first", "second", "script" }));
}

TEST_F(WorldTest, ASessionTheWorldNoLongerHoldsIsNotDrained)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    session->SetStatus(SessionStatus::Authenticated);
    sWorld.AddSession(session);

    bool ran = false;
    ASSERT_TRUE(session->QueueInbound([&] { ran = true; }));

    sWorld.RemoveSession(session.get());
    EXPECT_EQ(sWorld.GetSessionCount(), 0u);

    sWorld.Update(std::chrono::milliseconds(50));
    EXPECT_FALSE(ran) << "a session the world let go was still drained";
    EXPECT_EQ(session->GetQueuedMessageCount(), 1u);
}

TEST_F(WorldTest, TickBreakdownNamesTheSlowSubsystemAndAddsToTheTick)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    session->SetStatus(SessionStatus::Authenticated);
    sWorld.AddSession(session);
    ASSERT_TRUE(session->QueueInbound([] { std::this_thread::sleep_for(std::chrono::milliseconds(15)); }));

    double const tickBefore = HistogramSum("ambrose_world_tick_seconds");
    sWorld.Update(std::chrono::milliseconds(50));
    double const tickSeconds = HistogramSum("ambrose_world_tick_seconds") - tickBefore;

    std::optional<Ambrose::MetricEntry> const drain = FindMetric("ambrose_world_tick_subsystem_nanoseconds", "network_drain");
    ASSERT_TRUE(drain);
    ASSERT_NE(drain->AsGauge, nullptr);
    EXPECT_GT(drain->AsGauge->Value(), 10000000);

    double componentSeconds = 0.0;
    for (Ambrose::MetricEntry const& entry : sMetrics.Collect())
        if (entry.Name == "ambrose_world_tick_subsystem_nanoseconds" && entry.AsGauge)
            componentSeconds += static_cast<double>(entry.AsGauge->Value()) / 1000000000.0;
    EXPECT_NEAR(componentSeconds, tickSeconds, 0.0005);

    std::optional<Ambrose::MetricEntry> const overBudget = FindMetric("ambrose_world_tick_subsystem_over_budget", "network_drain");
    ASSERT_TRUE(overBudget);
    ASSERT_NE(overBudget->AsGauge, nullptr);
    EXPECT_EQ(overBudget->AsGauge->Value(), 1);

    for (std::string_view const name : { "movement", "chat" })
    {
        std::optional<Ambrose::MetricEntry> const available = FindMetric("ambrose_world_tick_subsystem_available", name);
        ASSERT_TRUE(available) << name;
        ASSERT_NE(available->AsGauge, nullptr);
        EXPECT_EQ(available->AsGauge->Value(), 1) << name;
        ASSERT_TRUE(FindMetric("ambrose_world_tick_subsystem_nanoseconds", name)) << name;
    }

    for (std::string_view const name : { "database_waits", "combat" })
    {
        std::optional<Ambrose::MetricEntry> const unavailable = FindMetric("ambrose_world_tick_subsystem_available", name);
        ASSERT_TRUE(unavailable) << name;
        ASSERT_NE(unavailable->AsGauge, nullptr);
        EXPECT_EQ(unavailable->AsGauge->Value(), 0) << name;
    }
}

TEST_F(WorldTest, TickProfileIsBoundedTimedAndReadableAsAChromeTrace)
{
    EXPECT_FALSE(sWorld.StartTickProfile(0));
    EXPECT_FALSE(sWorld.StartTickProfile(31));
    ASSERT_TRUE(sWorld.StartTickProfile(1));
    EXPECT_FALSE(sWorld.StartTickProfile(1));

    sWorld.Update(std::chrono::milliseconds(50));
    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    sWorld.Update(std::chrono::milliseconds(50));

    WorldTickProfileSnapshot const profile = sWorld.GetTickProfile();
    EXPECT_FALSE(profile.Active);
    EXPECT_TRUE(profile.Complete);
    EXPECT_FALSE(profile.Truncated);
    EXPECT_EQ(profile.RequestedSeconds, 1u);
    ASSERT_FALSE(profile.Events.empty());

    nlohmann::json const trace = nlohmann::json::parse(World::TickProfileTraceJson(profile));
    ASSERT_TRUE(trace["traceEvents"].is_array());
    EXPECT_FALSE(trace["traceEvents"].empty());
    EXPECT_EQ(trace["metadata"]["requested_seconds"], 1);
    EXPECT_FALSE(trace["metadata"]["truncated"].get<bool>());
    EXPECT_EQ(trace["traceEvents"].front()["ph"], "X");
    EXPECT_EQ(trace["traceEvents"].front()["cat"], "world_tick");
}

TEST_F(WorldTest, TickProfileStopsAtItsEventBound)
{
    ASSERT_TRUE(sWorld.StartTickProfile(30));
    for (std::size_t tick = 0; tick < 12000; ++tick)
        sWorld.Update(std::chrono::milliseconds(50));

    WorldTickProfileSnapshot const profile = sWorld.GetTickProfile();
    EXPECT_FALSE(profile.Active);
    EXPECT_TRUE(profile.Complete);
    EXPECT_TRUE(profile.Truncated);
    EXPECT_LE(profile.Events.size(), 50000u);
    EXPECT_EQ(profile.Events.size(), 50000u);
}
