/*
 * Project Ambrose by Imjustchico
 * Tests chat handling through game sessions, including queued speech, custom emotes, command replies, mute and closed-chat rejection, and paired talking-emote suppression.
 */

#include "AccountMgr.h"
#include "ChatCommand.h"
#include "ChatText.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "GameTestHarness.h"
#include "ScriptMgr.h"
#include "SystemMessages.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

struct ChatHandlerTestAccess
{
    static void StandInInstance(GameSession& session)
    {
        session._mapId = 1;
        session._publicObject = { 0 };
    }

    static bool HidesNextChatEmote(GameSession const& session)
    {
        return session._hideNextChatEmote;
    }
};

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

TEST_F(ChatHandlerTest, TheRadialMenuCustomEmoteMessagesAreHandled)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);

    GameMessages::CorePiiRadialMenuEmote emote;
    emote.EmoteAnimationName = "Wave";
    emote.ExcludeOriginator = 1;
    Send(*client, emote);
    GameMessages::RequestPiiRadialMenuPlayEmote request;
    request.EmoteAnimationName = emote.EmoteAnimationName;
    request.EmoteText = u"hello";
    Send(*client, request);

    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 2; }));
    EXPECT_EQ(session->DrainQueue(), 2u);
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

TEST_F(ChatHandlerTest, AMutedChatRequestIsDroppedAndTheClientIsNotified)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);
    ChatHandlerTestAccess::StandInInstance(*session);

    GameMessages::RequestRadialChat line;
    line.Message = ChatText::Write(u"hello");
    Send(*client, line);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    ASSERT_EQ(session->TakeSpeech().size(), 1u) << "an unmuted line is queued to be heard";

    session->ApplyMute(AccountMgr::Now() + 60);
    std::optional<GameMessages::Mute> const initial = ReadReply<GameMessages::Mute>(*client);
    ASSERT_TRUE(initial);
    EXPECT_GT(std::stoll(initial->MuteTime), 0);
    EXPECT_LE(std::stoll(initial->MuteTime), 60);
    EXPECT_EQ(initial->ForceMessage, 1);

    Send(*client, line);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    std::optional<GameMessages::Mute> const notice = ReadReply<GameMessages::Mute>(*client);
    ASSERT_TRUE(notice) << "the client gets an explicit mute notice for a rejected request";
    EXPECT_GT(std::stoll(notice->MuteTime), 0);
    EXPECT_TRUE(session->TakeSpeech().empty());
    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u);
    EXPECT_EQ(session->GetStrikes(), 0u);
}

TEST_F(ChatHandlerTest, AMutedGameMastersCommandRunsBeforeSpeechIsRejected)
{
    sCommandMgr.Clear();
    sCommandMgr.Load({ { .Name = "ping", .SecurityLevel = SEC_GAMEMASTER, .Help = "answer", .Run = [](CommandCaller& caller, std::vector<std::string> const&)
    {
        caller.Reply("pong");
        return true;
    } } });

    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);
    session->SetSecurityLevel(SEC_GAMEMASTER);
    session->ApplyMute(AccountMgr::Now() + 60);
    ASSERT_TRUE(ReadReply<GameMessages::Mute>(*client));

    GameMessages::RequestRadialChat line;
    line.Message = ChatText::Write(u".ping");
    Send(*client, line);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);

    std::optional<SystemMessages::ServerMessage> const reply = ReadReply<SystemMessages::ServerMessage>(*client);
    ASSERT_TRUE(reply) << "the next message is the command reply, not a mute notice";
    EXPECT_EQ(reply->Message, u"pong");
    EXPECT_TRUE(session->TakeSpeech().empty());

    sCommandMgr.Clear();
}

TEST_F(ChatHandlerTest, ARejectedMutedLineHidesItsTalkingEmoteAndSendsOneNotice)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);
    ChatHandlerTestAccess::StandInInstance(*session);
    session->ApplyMute(AccountMgr::Now() + 60);
    ASSERT_TRUE(ReadReply<GameMessages::Mute>(*client));

    GameMessages::RequestRadialChat line;
    line.Message = ChatText::Write(u"hello");
    Send(*client, line);
    GameMessages::CoreEmote emote;
    emote.Name = "Chat";
    emote.ExcludeOriginator = 1;
    Send(*client, emote);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 2; }));
    EXPECT_EQ(session->DrainQueue(), 2u);

    ASSERT_TRUE(ReadReply<GameMessages::Mute>(*client));
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(100))) << "the paired emote sends no second notice";
    EXPECT_TRUE(session->TakeSpeech().empty()) << "neither speech nor its talking emote is queued";
}

TEST_F(ChatHandlerTest, ClosedTypedChatHidesItsTalkingEmote)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);
    ChatHandlerTestAccess::StandInInstance(*session);
    session->SetChatMode(2);

    GameMessages::RequestRadialChat line;
    line.Message = ChatText::Write(u"hello");
    Send(*client, line);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    std::optional<SystemMessages::ServerMessage> const notice = ReadReply<SystemMessages::ServerMessage>(*client);
    ASSERT_TRUE(notice);
    EXPECT_EQ(notice->Message, u"Chat is disabled for this account.");
    EXPECT_TRUE(ChatHandlerTestAccess::HidesNextChatEmote(*session)) << "the refused line hides the talking emote that follows it";

    GameMessages::CoreEmote emote;
    emote.Name = "Chat";
    emote.ExcludeOriginator = 1;
    Send(*client, emote);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    EXPECT_FALSE(ChatHandlerTestAccess::HidesNextChatEmote(*session));
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(100))) << "the paired emote sends no second notice";
    EXPECT_TRUE(session->TakeSpeech().empty());
}

TEST_F(ChatHandlerTest, ClearingMuteSendsOnlyAClientSafeNotice)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);

    session->ApplyMute(AccountMgr::Now() + 60);
    ASSERT_TRUE(ReadReply<GameMessages::Mute>(*client));

    session->ClearMute();
    std::optional<SystemMessages::ServerMessage> const notice = ReadReply<SystemMessages::ServerMessage>(*client);
    ASSERT_TRUE(notice);
    EXPECT_EQ(notice->Message, u"You have been unmuted.");
}

TEST_F(ChatHandlerTest, AnExpiredMuteIsClearedWhenTheClientSendsChat)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);

    session->ApplyMute(AccountMgr::Now() - 1);

    GameMessages::RequestRadialChat line;
    line.Message = ChatText::Write(u"hello");
    Send(*client, line);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    std::optional<SystemMessages::ServerMessage> const notice = ReadReply<SystemMessages::ServerMessage>(*client);
    ASSERT_TRUE(notice);
    EXPECT_EQ(notice->Message, u"You have been unmuted.");
    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u);
    EXPECT_EQ(session->GetStrikes(), 0u);
}

TEST_F(ChatHandlerTest, ClosedChatModeRejectsTypedAndQuickChat)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);
    ChatHandlerTestAccess::StandInInstance(*session);
    session->SetChatMode(2);

    GameMessages::RequestRadialChat line;
    line.Message = ChatText::Write(u"hello");
    Send(*client, line);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    std::optional<SystemMessages::ServerMessage> const typedNotice = ReadReply<SystemMessages::ServerMessage>(*client);
    ASSERT_TRUE(typedNotice);
    EXPECT_EQ(typedNotice->Message, u"Chat is disabled for this account.");
    EXPECT_TRUE(session->TakeSpeech().empty());

    GameMessages::RequestRadialQuickChat phrase;
    phrase.MessageId = 267;
    Send(*client, phrase);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);
    std::optional<SystemMessages::ServerMessage> const quickChatNotice = ReadReply<SystemMessages::ServerMessage>(*client);
    ASSERT_TRUE(quickChatNotice);
    EXPECT_EQ(quickChatNotice->Message, u"Chat is disabled for this account.");
    EXPECT_TRUE(session->TakeSpeech().empty());
    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u);
    EXPECT_EQ(session->GetStrikes(), 0u);
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
