/*
 * Project Ambrose by Imjustchico
 * Drives the handoff a client makes over loopback: a game server listening on its own port offers a session, and a client that has just been told where to go connects, handshakes and sends MSG_ATTACH, which is dispatched while the session is only Connected, because a client that has not attached has nothing else it may say. Checks that the attach is answered rather than counted as a message with no rule, that a server with no login database behind it refuses the key and says so with MSG_ATTACHFAILED instead of believing the account and wizard the client named for itself, and that a game message with no rule is counted rather than acted on.
 */

#include "FakeSessionClient.h"
#include "GameMessageTable.h"
#include "GameSession.h"
#include "LoginMessageFixtures.h"
#include "MessageRegistry.h"
#include "SessionContext.h"
#include "FrameWriter.h"
#include "SocketMgr.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <mutex>
#include <string>
#include <vector>

namespace
{
    class GameServerHarness
    {
    public:
        GameServerHarness()
        {
            sMessageRegistry.Clear();
            std::vector<std::string> errors;
            EXPECT_TRUE(GameMessageTable::Get().Declare(sMessageRegistry, errors)) << (errors.empty() ? std::string() : errors.front());
            MessageDefinitionSet definitions;
            EXPECT_TRUE(LoginMessageFixtures::AddTo(definitions, true));
            EXPECT_TRUE(sMessageRegistry.Load(std::move(definitions)));

            _context = std::make_shared<SessionContext>(SessionSettings{});
            _manager = std::make_unique<SocketMgr<GameSession>>([this](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
            {
                auto session = std::make_shared<GameSession>(std::move(socket), limits, _context);
                std::lock_guard const lock(_mutex);
                _sessions.push_back(session);
                return session;
            });
            NetworkSettings network;
            network.BindIp = "127.0.0.1";
            network.Port = 0;
            network.Threads = 1;
            std::string error;
            EXPECT_TRUE(_manager->StartNetwork(network, error)) << error;
        }

        ~GameServerHarness()
        {
            _manager.reset();
            sMessageRegistry.Clear();
        }

        std::unique_ptr<FakeSessionClient> Connect(uint16& sessionId)
        {
            auto client = std::make_unique<FakeSessionClient>(_manager->GetPort());
            sessionId = client->Handshake();
            EXPECT_NE(sessionId, 0);
            return client;
        }

        std::shared_ptr<GameSession> Find(uint16 sessionId)
        {
            std::lock_guard const lock(_mutex);
            for (std::weak_ptr<GameSession> const& weak : _sessions)
                if (std::shared_ptr<GameSession> session = weak.lock(); session && session->GetSessionId() == sessionId)
                    return session;
            return nullptr;
        }

    private:
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<GameSession>> _manager;
        std::mutex _mutex;
        std::vector<std::weak_ptr<GameSession>> _sessions;
    };

    template<DeclaredMessage T>
    void Send(FakeSessionClient& client, T const& message)
    {
        ByteBuffer body;
        sMessageRegistry.Encode(message, body);
        MessageInfo const& info = sMessageRegistry.GetCatalog()->GetInfo<T>();
        ByteBuffer frame;
        FrameWriter::WriteDml(frame, info.Protocol->ServiceId, static_cast<uint8>(info.Definition->Order), body.GetData());
        client.Send(frame);
    }
}

TEST(GameAttachTest, AnAttachIsTakenWhileOnlyConnectedAndIsRefusedWhenNoKeyCanBeSpent)
{
    GameServerHarness server;
    uint16 sessionId = 0;
    std::unique_ptr<FakeSessionClient> const client = server.Connect(sessionId);
    std::shared_ptr<GameSession> session;
    ASSERT_TRUE(WaitForCondition([&] { session = server.Find(sessionId); return session != nullptr; }));
    EXPECT_EQ(session->GetStatus(), SessionStatus::Connected) << "the attach is answered before anything has raised the session's status";

    GameMessages::Attach attach;
    attach.LoginKey = "3Yl0dGhpcyBpcyBub3QgYSByZWFsIGtleSBidXQgaXQgaXMgbG9uZw==";
    attach.UserId = 4242;
    attach.CharId = 101;
    attach.ZoneName = "WizardCity/WC_Ravenwood";
    attach.Location = "-32,-552,-28,6.350083";
    Send(*client, attach);

    std::optional<DmlMessageData> const reply = ReadNextDml(*client);
    ASSERT_TRUE(reply) << "MSG_ATTACH was not dispatched while the session was Connected";
    MessageInfo const& refusal = sMessageRegistry.GetCatalog()->GetInfo<GameMessages::AttachFailed>();
    EXPECT_EQ(reply->ServiceId, refusal.Protocol->ServiceId);
    EXPECT_EQ(reply->Order, refusal.Definition->Order) << "a client whose key no login database can vouch for is told so";

    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u) << "a message the table answers is not counted as one it could not";
    EXPECT_FALSE(session->IsAttached());
    EXPECT_EQ(session->GetAccountId(), 0u) << "the server does not take the client's word for whose account it is";
    EXPECT_EQ(session->GetCharacterId(), 0u) << "nor for which wizard it is";
}

TEST(GameAttachTest, AGameMessageWithNoRuleIsCountedRatherThanActedOn)
{
    GameServerHarness server;
    uint16 sessionId = 0;
    std::unique_ptr<FakeSessionClient> const client = server.Connect(sessionId);
    std::shared_ptr<GameSession> session;
    ASSERT_TRUE(WaitForCondition([&] { session = server.Find(sessionId); return session != nullptr; }));

    ByteBuffer frame;
    FrameWriter::WriteDml(frame, GameMessages::GameService, 200, std::vector<uint8>{});
    client->Send(frame);

    ASSERT_TRUE(WaitForCondition([&] { return session->GetUnhandledMessageCount() > 0; })) << "a game message with no rule must be noticed";
    EXPECT_EQ(session->GetAccountId(), 0u) << "and must not reach a handler";
}
