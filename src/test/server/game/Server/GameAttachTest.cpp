/*
 * Project Ambrose by Imjustchico
 * Drives the handoff a client makes over loopback: a game server listening on its own port offers a session, and a client that has just been told where to go connects, handshakes and sends MSG_ATTACH, which is dispatched while the session is only Connected, because a client that has not attached has nothing else it may say. Checks that the attach is answered rather than counted as a message with no rule, that a server with no login database behind it refuses the key and says so with MSG_ATTACHFAILED instead of believing the account and wizard the client named for itself, and that a game message with no rule is counted rather than acted on.
 */

#include "GameTestHarness.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

using namespace GameTesting;

TEST(GameAttachTest, AnAttachIsTakenWhileOnlyConnectedAndIsRefusedWhenNoKeyCanBeSpent)
{
    GameDefinitions definitions;
    GameListener server;
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
    EXPECT_TRUE(Is<GameMessages::AttachFailed>(*reply)) << "a client whose key no login database can vouch for is told so";

    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u) << "a message the table answers is not counted as one it could not";
    EXPECT_FALSE(session->IsAttached());
    EXPECT_EQ(session->GetAccountId(), 0u) << "the server does not take the client's word for whose account it is";
    EXPECT_EQ(session->GetCharacterId(), 0u) << "nor for which wizard it is";
}

TEST(GameAttachTest, AGameMessageWithNoRuleIsCountedRatherThanActedOn)
{
    GameDefinitions definitions;
    GameListener server;
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
