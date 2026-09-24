/*
 * Project Ambrose by Imjustchico
 * Drives real LoginSessions over loopback with a frozen clock the test moves forward: an idle session is kept one second before Login.AfkTimeout and dropped at it with the configured Warning byte, keepalives do not count as activity, MSG_LOGIN_NOT_AFK every 30 seconds keeps a session, a lowered or disabled timeout applies live, the check is suspended while a character is selected, and a shutdown stops accepting clients and waits only until every notice is written.
 */

#include "ControlMessages.h"
#include "LoginMgr.h"
#include "LoginShutdown.h"
#include "LoginTestHarness.h"

#include <gtest/gtest.h>

#include <future>
#include <system_error>

namespace
{
    using namespace LoginTesting;
    using namespace std::chrono_literals;

    class LoginAfkTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sLoginMgr.Reset();
            sLoginMgr.FreezeClock();
            _server = std::make_unique<LoginServerHarness>();
        }

        void TearDown() override
        {
            _server.reset();
            sLoginMgr.Reset();
        }

        static void Configure(std::chrono::seconds timeout, int8 warning = 1)
        {
            LoginSettings settings = *sLoginMgr.GetSettings();
            settings.AfkTimeout = timeout;
            settings.AfkWarning = warning;
            sLoginMgr.SetSettings(settings);
        }

        static void WaitForChecks(std::shared_ptr<LoginSession> const& session)
        {
            uint64 const before = session->GetAfkCheckCount();
            ASSERT_TRUE(WaitForCondition([&] { return session->GetAfkCheckCount() >= before + 2; }));
        }

        static void ExpectStillConnected(LoginClient& client, std::shared_ptr<LoginSession> const& session)
        {
            ASSERT_NO_FATAL_FAILURE(WaitForChecks(session));
            EXPECT_FALSE(ReadDml(*client.Socket, 50ms));
            EXPECT_FALSE(client.Socket->IsClosed());
            EXPECT_TRUE(session->IsOpen());
        }

        std::unique_ptr<LoginServerHarness> _server;
    };
}

TEST_F(LoginAfkTest, AnIdleSessionIsDroppedExactlyAtTheTimeoutAndKeepalivesDoNotCount)
{
    Configure(60s, -1);
    LoginClient client = _server->Connect();
    std::shared_ptr<LoginSession> const session = WaitForSession(*_server, client);
    ASSERT_TRUE(session);
    sLoginMgr.AdvanceClock(59s);
    ExpectStillConnected(client, session);

    ByteBuffer keepAlive;
    ControlMessages::WriteFrame(keepAlive, ClientKeepAlive{ client.Salt.SessionId, 0, 1 });
    client.Socket->Send(keepAlive);
    ASSERT_TRUE(client.Socket->ReadControl(ControlOpcode::KeepAliveRsp, 10s));

    sLoginMgr.AdvanceClock(1s);
    std::optional<LoginMessages::DisconnectLoginAfk> const message = ReadMessage<LoginMessages::DisconnectLoginAfk>(client);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->Warning, -1);
    EXPECT_TRUE(client.Socket->WaitForClose());

    LoginClient standard = _server->Connect();
    ASSERT_TRUE(WaitForSession(*_server, standard));
    Configure(LoginSettings{}.AfkTimeout);
    sLoginMgr.AdvanceClock(LoginSettings{}.AfkTimeout);
    std::optional<LoginMessages::DisconnectLoginAfk> const standardMessage = ReadMessage<LoginMessages::DisconnectLoginAfk>(standard);
    ASSERT_TRUE(standardMessage);
    EXPECT_EQ(standardMessage->Warning, 1);
}

TEST_F(LoginAfkTest, NotAfkEveryThirtySecondsKeepsTheSession)
{
    LoginClient client = _server->Connect();
    std::shared_ptr<LoginSession> const session = WaitForSession(*_server, client);
    ASSERT_TRUE(session);
    for (int i = 0; i < 24; ++i)
    {
        sLoginMgr.AdvanceClock(30s);
        auto const sent = sLoginMgr.Now();
        Send(client, LoginMessages::LoginNotAfk{ 7 });
        ASSERT_TRUE(WaitForCondition([&] { return session->GetLastActivity() == sent; })) << i;
    }
    ExpectStillConnected(client, session);
    EXPECT_EQ(session->GetStrikes(), 0u);

    sLoginMgr.AdvanceClock(LoginSettings{}.AfkTimeout);
    ASSERT_TRUE(ReadMessage<LoginMessages::DisconnectLoginAfk>(client));
}

TEST_F(LoginAfkTest, LoweringOrDisablingTheTimeoutAppliesLive)
{
    LoginClient client = _server->Connect();
    std::shared_ptr<LoginSession> const session = WaitForSession(*_server, client);
    ASSERT_TRUE(session);
    sLoginMgr.AdvanceClock(100s);
    ExpectStillConnected(client, session);

    Configure(101s);
    ExpectStillConnected(client, session);
    Configure(100s);
    ASSERT_TRUE(ReadMessage<LoginMessages::DisconnectLoginAfk>(client));
    EXPECT_TRUE(client.Socket->WaitForClose());

    LoginClient disabled = _server->Connect();
    std::shared_ptr<LoginSession> const kept = WaitForSession(*_server, disabled);
    ASSERT_TRUE(kept);
    Configure(0s);
    sLoginMgr.AdvanceClock(24h);
    ExpectStillConnected(disabled, kept);
}

TEST_F(LoginAfkTest, TheCheckIsSuspendedWhileACharacterIsSelected)
{
    LoginClient client = _server->Connect();
    std::shared_ptr<LoginSession> const session = WaitForSession(*_server, client);
    ASSERT_TRUE(session);
    session->SetStatus(SessionStatus::CharacterSelected);
    sLoginMgr.AdvanceClock(1h);
    ExpectStillConnected(client, session);

    session->SetStatus(SessionStatus::Authenticated);
    ASSERT_TRUE(ReadMessage<LoginMessages::DisconnectLoginAfk>(client));
    EXPECT_TRUE(client.Socket->WaitForClose());
}

TEST_F(LoginAfkTest, AShutdownStopsAcceptingAndWaitsOnlyForTheNoticesToBeWritten)
{
    LoginClient first = _server->Connect();
    LoginClient second = _server->Connect();
    ASSERT_TRUE(WaitForSession(*_server, first));
    ASSERT_TRUE(WaitForSession(*_server, second));
    uint16 const port = _server->GetSockets().GetPort();

    EXPECT_EQ(LoginShutdown::NotifyAndDrain(_server->GetSockets(), 0s), 0u);
    EXPECT_FALSE(_server->GetSockets().IsListening());
    EXPECT_THROW(FakeSessionClient late(port), std::system_error);
    EXPECT_FALSE(ReadDml(*first.Socket, 200ms));

    auto const start = std::chrono::steady_clock::now();
    std::future<std::size_t> notified = std::async(std::launch::async, [this] { return LoginShutdown::NotifyAndDrain(_server->GetSockets(), 30s, 3); });
    ASSERT_EQ(notified.wait_for(20s), std::future_status::ready);
    EXPECT_EQ(notified.get(), 2u);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 20s);

    for (LoginClient* client : { &first, &second })
    {
        std::optional<LoginMessages::LoginServerShutdown> const notice = ReadMessage<LoginMessages::LoginServerShutdown>(*client);
        ASSERT_TRUE(notice);
        EXPECT_EQ(notice->Message, 3u);
        EXPECT_TRUE(client->Socket->WaitForClose());
        client->Socket.reset();
    }
    ASSERT_TRUE(WaitForCondition([this] { return _server->GetSockets().GetConnectionCount() == 0; }));
    EXPECT_EQ(LoginShutdown::NotifyAndDrain(_server->GetSockets(), 5s), 0u);
}
