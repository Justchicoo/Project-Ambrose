/*
 * Project Ambrose by Imjustchico
 * Exercises live AFK and link-dead deadlines, reconnect takeover of one character's existing placement, and shutdown-safe session state.
 */

#include "ConfigMgr.h"
#include "GameSession.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "Settings.h"
#include "StringHash.h"
#include "World.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct GameSessionLifecycleTestAccess
{
    static void PrepareAttachedInWorld(GameSession& session, uint64 characterId)
    {
        session.SetAccountId(123);
        session.SetCharacterId(characterId);
        session._attached.store(true, std::memory_order_relaxed);
        session._inWorld.store(true, std::memory_order_relaxed);
        session.SetStatus(SessionStatus::InWorld);
    }

    static void Close(GameSession& session)
    {
        session.OnSessionClosed();
    }

    static std::chrono::steady_clock::time_point LostAt(GameSession const& session)
    {
        return std::chrono::steady_clock::time_point(std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::nanoseconds(session._socketLostAtNanoseconds.load(std::memory_order_relaxed))));
    }

    static void StartAfkTimer(GameSession& session, std::chrono::steady_clock::time_point started)
    {
        PrepareAttachedInWorld(session, 456);
        session._afkStarted = started;
        session._afkTimerStarted = true;
        session._afkWarned = false;
    }

    static bool WasAfkWarningSent(GameSession const& session)
    {
        return session._afkWarned;
    }

    static std::chrono::steady_clock::time_point AfkStarted(GameSession const& session)
    {
        return session._afkStarted;
    }

    static void SetZonePath(GameSession& session, std::string zonePath)
    {
        session._zonePath = std::move(zonePath);
    }

    static void TransferWorldState(GameSession& current, GameSession& replacement)
    {
        current.TransferWorldStateTo(replacement);
    }

    static void SetPlacement(GameSession& session, uint32 mapId, uint64 worldGuid, uint16 mobileId)
    {
        session._mapId = mapId;
        session._worldGuid = worldGuid;
        session._zonePath = "WizardCity/Commons";
        session._mobileId = mobileId;
        session._arrived = false;
        session._movement.Reset({ 1.0f, 2.0f, 3.0f, 4.0f }, 0);
        session._movement.Apply(100, 200, 300, 40, 0);
    }
};

namespace
{
    class GameSessionLifecycleTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sWorld.Clear();
            sSettings.Clear();
            _configFile = _directory.Write("gameserver.conf",
                "Player.LinkDeadTime = 1\nPlayer.AfkWarnTime = 2\nPlayer.AfkTime = 4\n");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_configFile).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
            _context = std::make_shared<SessionContext>(SessionSettings{});
        }

        void TearDown() override
        {
            sWorld.Clear();
            sSettings.Clear();
        }

        std::shared_ptr<GameSession> MakeSession()
        {
            asio::ip::tcp::acceptor acceptor(_io, { asio::ip::address_v4::loopback(), 0 });
            asio::ip::tcp::socket client(_io);
            client.connect(acceptor.local_endpoint());
            asio::ip::tcp::socket server(_io);
            acceptor.accept(server);
            return std::make_shared<GameSession>(std::move(server), FrameLimits{}, _context);
        }

        asio::io_context _io;
        LogTestDirectory _directory;
        std::filesystem::path _configFile;
        std::unique_ptr<ConfigMgr> _config;
        std::shared_ptr<SessionContext> _context;
    };
}

TEST_F(GameSessionLifecycleTest, LinkDeadDeadlineUsesTheLiveSettingOnTheNextWorldTimer)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*session, 42);
    GameSessionLifecycleTestAccess::Close(*session);
    ASSERT_TRUE(session->IsLinkDead());

    auto const lostAt = GameSessionLifecycleTestAccess::LostAt(*session);
    ASSERT_TRUE(sSettings.Set("Player.LinkDeadTime", "3", { "test", 1, "unit_test" }, "extend test window").Ok());
    session->WorldUpdate(lostAt + std::chrono::seconds(2));
    EXPECT_TRUE(session->CanResume(lostAt + std::chrono::seconds(2)));

    ASSERT_TRUE(sSettings.Set("Player.LinkDeadTime", "2", { "test", 1, "unit_test" }, "shorten test window").Ok());
    session->WorldUpdate(lostAt + std::chrono::seconds(2));
    EXPECT_FALSE(session->IsLinkDead());
    EXPECT_FALSE(session->IsAttached());
}

TEST_F(GameSessionLifecycleTest, NotAfkResetsTheWarningAndLiveAfkTimeControlsDisconnect)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    auto const started = std::chrono::steady_clock::now();
    GameSessionLifecycleTestAccess::StartAfkTimer(*session, started);

    session->WorldUpdate(started + std::chrono::seconds(2));
    EXPECT_TRUE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));
    EXPECT_FALSE(session->IsKicked());

    GameMessages::NotAfk notAfk;
    session->HandleNotAfk(notAfk);
    auto const resumedAt = GameSessionLifecycleTestAccess::AfkStarted(*session);
    session->WorldUpdate(resumedAt + std::chrono::seconds(1));
    EXPECT_FALSE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));
    session->WorldUpdate(resumedAt + std::chrono::seconds(2));
    EXPECT_TRUE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));

    ASSERT_TRUE(sSettings.Set("Player.AfkTime", "3", { "test", 1, "unit_test" }, "shorten test timeout").Ok());
    session->WorldUpdate(resumedAt + std::chrono::seconds(3));
    EXPECT_TRUE(session->IsKicked());
}

TEST_F(GameSessionLifecycleTest, RepeatedClientZonedDoesNotResetAfkTimer)
{
    std::shared_ptr<GameSession> const session = MakeSession();
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*session, 42);
    GameSessionLifecycleTestAccess::SetZonePath(*session, "WizardCity/Commons");
    session->SetStatus(SessionStatus::LoggedIn);

    GameMessages::ClientZoned zoned;
    zoned.ZoneNameId = StringHash::KiStringHash("WizardCity/Commons");
    session->HandleClientZoned(zoned);
    auto const started = GameSessionLifecycleTestAccess::AfkStarted(*session);

    session->WorldUpdate(started + std::chrono::seconds(2));
    ASSERT_TRUE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));
    session->HandleClientZoned(zoned);

    EXPECT_EQ(GameSessionLifecycleTestAccess::AfkStarted(*session), started);
    EXPECT_TRUE(GameSessionLifecycleTestAccess::WasAfkWarningSent(*session));
}

TEST_F(GameSessionLifecycleTest, ReplacementAttachTakesOverTheExistingCharacterPlacement)
{
    std::shared_ptr<GameSession> const current = MakeSession();
    std::shared_ptr<GameSession> const replacement = MakeSession();
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*current, 42);
    GameSessionLifecycleTestAccess::PrepareAttachedInWorld(*replacement, 42);
    GameSessionLifecycleTestAccess::SetPlacement(*current, 17, 42, 9);
    sWorld.AddSession(current);
    sWorld.AddSession(replacement);

    ASSERT_EQ(sWorld.FindSessionByCharacterId(42), current);
    PlayerPosition const position = current->GetMovement().GetPosition();
    GameSessionLifecycleTestAccess::TransferWorldState(*current, *replacement);

    EXPECT_TRUE(current->IsKicked());
    EXPECT_FALSE(current->IsAttached());
    EXPECT_FALSE(current->GetMapId());
    EXPECT_EQ(replacement->GetMapId(), 17u);
    EXPECT_EQ(replacement->GetWorldGuid(), 42u);
    EXPECT_EQ(replacement->GetMovement().GetPosition(), position);
    EXPECT_EQ(sWorld.FindSessionByCharacterId(42), replacement);
}
