/*
 * Project Ambrose by Imjustchico
 * Tests that the world owns its thread: work queued on a session from any other thread runs on the one that calls Update and on no other, which is the whole reason the queue exists; the tick drains every session it holds before it runs the scripts, so a script sees what the messages of that tick left behind, a session removed while it holds queued work is not drained again, and the world says which thread it belongs to.
 */

#include "GameSession.h"
#include "ScriptMgr.h"
#include "SessionContext.h"
#include "World.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
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
