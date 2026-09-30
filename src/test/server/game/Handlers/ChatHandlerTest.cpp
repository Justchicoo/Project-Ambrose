/*
 * Project Ambrose by Imjustchico
 * Drives the chat a client sends through a real game session over loopback: a typed line, a quick chat phrase, an extended phrase and an emote from a wizard that has its object are each taken and queued for the world thread rather than counted as messages the server does not handle, and shown to nobody while the wizard does not yet stand shown in an instance; a line starting with the command prefix from a game master's account runs the command, whose reply comes back as one MSG_SERVERMESSAGE, a line too long for one split across several, before the wizard stands anywhere; and a client that has not attached is not listened to at all; and a server script refusing one message holds back exactly that one, reaching the session as though never sent, with no edit to the core.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "ChatText.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "GameTestHarness.h"
#include "ScriptMgr.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using namespace GameTesting;

    constexpr uint64 WizardId = 7002;

    class HoldBackQuickChat : public ServerScript
    {
    public:
        HoldBackQuickChat() : ServerScript("hold_back_quick_chat") {}

        bool CanPacketReceive(uint16, uint8 serviceId, uint8 order) override
        {
            MessageInfo const* const quickChat = sMessageRegistry.GetCatalog()->Find(GameMessages::GameService, "MSG_REQUESTRADIALQUICKCHAT");
            return !quickChat || serviceId != GameMessages::GameService || order != quickChat->Definition->Order;
        }
    };

    class ChatHandlerTest : public testing::Test
    {
    protected:
        std::shared_ptr<GameSession> Connect(std::unique_ptr<FakeSessionClient>& client, bool entered)
        {
            uint16 sessionId = 0;
            client = _server.Connect(sessionId);
            std::shared_ptr<GameSession> session;
            EXPECT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
            if (session && entered)
            {
                session->SetCharacterId(WizardId);
                session->SetStatus(SessionStatus::LoggedIn);
            }
            return session;
        }

        void SendEach(FakeSessionClient& client)
        {
            GameMessages::RequestRadialChat line;
            line.Message = ChatText::Write(u"hello");
            Send(client, line);
            GameMessages::RequestRadialQuickChat phrase;
            phrase.MessageId = 267;
            Send(client, phrase);
            GameMessages::RequestRadialQuickChatExt extended;
            extended.Message = "Stats Health 0";
            Send(client, extended);
            GameMessages::CoreEmote emote;
            emote.Name = "Wave";
            emote.ExcludeOriginator = 1;
            Send(client, emote);
        }

        GameDefinitions _definitions;
        GameListener _server;
    };

    template<DeclaredMessage T>
    std::optional<T> ReadReply(FakeSessionClient& client)
    {
        std::optional<DmlMessageData> const reply = ReadNextDml(client);
        if (!reply || !Is<T>(*reply))
            return std::nullopt;
        T message;
        if (sMessageRegistry.GetCatalog()->Decode(reply->Body, message) != MessageDecodeStatus::Ok)
            return std::nullopt;
        return message;
    }
}

TEST_F(ChatHandlerTest, ChatIsQueuedForTheWorldAndShownToNobodyBeforeTheWizardStandsInAnInstance)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);

    SendEach(*client);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 4; })) << "each waits for the world thread, where the wizard's place is kept";
    EXPECT_EQ(session->DrainQueue(), 4u);
    EXPECT_TRUE(session->TakeSpeech().empty()) << "a wizard shown in no instance has nobody to be heard by";
    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u);
    EXPECT_EQ(session->GetStrikes(), 0u);
}

TEST_F(ChatHandlerTest, AClientThatHasNotAttachedIsNotListenedTo)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, false);
    ASSERT_TRUE(session);

    SendEach(*client);
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(500)));
    EXPECT_EQ(session->GetQueuedMessageCount(), 0u) << "nothing a client says before it attaches reaches the world";
}

TEST_F(ChatHandlerTest, AGameMastersCommandRunsAndItsRepliesComeBackAsServerMessages)
{
    sCommandMgr.Clear();
    sCommandMgr.Load({ { .Name = "ping", .SecurityLevel = SEC_GAMEMASTER, .Help = "answer", .Run = [](CommandCaller& caller, std::vector<std::string> const&)
    {
        caller.Reply("pong");
        caller.Reply("again");
        return true;
    } } });
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);
    session->SetSecurityLevel(SEC_GAMEMASTER);

    GameMessages::RequestRadialChat line;
    line.Message = ChatText::Write(u".ping");
    Send(*client, line);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    std::optional<SystemMessages::ServerMessage> const reply = ReadReply<SystemMessages::ServerMessage>(*client);
    ASSERT_TRUE(reply) << "the command's reply goes to the chat window of the one who typed it";
    EXPECT_EQ(reply->Message, u"pong\nagain") << "one message for the whole reply, so its client shows one notice";
    EXPECT_EQ(reply->Modal, 0);
    EXPECT_TRUE(session->TakeSpeech().empty()) << "a command is never said";

    sCommandMgr.Clear();
    sCommandMgr.Load({ { .Name = "long", .SecurityLevel = SEC_GAMEMASTER, .Help = "answer at length", .Run = [](CommandCaller& caller, std::vector<std::string> const&)
    {
        caller.Reply(std::string(4000, 'a'));
        return true;
    } } });
    line.Message = ChatText::Write(u".long");
    Send(*client, line);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    std::size_t received = 0;
    for (int part = 0; part < 3; ++part)
    {
        std::optional<SystemMessages::ServerMessage> const piece = ReadReply<SystemMessages::ServerMessage>(*client);
        ASSERT_TRUE(piece) << "part " << part;
        EXPECT_LE(piece->Message.size(), 1500u) << "a line too long for one message is split";
        received += piece->Message.size();
    }
    EXPECT_EQ(received, 4000u);
    sCommandMgr.Clear();
}

TEST_F(ChatHandlerTest, AServerScriptHoldsBackTheOneMessageItRefusesWithNoEditToTheCore)
{
    sScriptMgr.Unload();
    new HoldBackQuickChat();
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);

    SendEach(*client);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 3; })) << "the quick chat phrase never reaches the session";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(session->GetQueuedMessageCount(), 3u);
    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u) << "a message a hook holds back is not one the server failed to handle";
    EXPECT_EQ(session->GetStrikes(), 0u);
    EXPECT_EQ(session->DrainQueue(), 3u);

    sScriptMgr.Unload();
    SendEach(*client);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 4; })) << "with the script gone, every message reaches the session again";
    EXPECT_EQ(session->DrainQueue(), 4u);
}
