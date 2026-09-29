/*
 * Project Ambrose by Imjustchico
 * Drives the chat a client sends through a real game session over loopback: a typed line, a quick chat phrase, an extended phrase and an emote from a wizard that has its object are each taken and queued for the world thread rather than counted as messages the server does not handle, and shown to nobody while the wizard does not yet stand shown in an instance; and a client that has not attached is not listened to at all.
 */

#include "ChatText.h"
#include "GameTestHarness.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>

namespace
{
    using namespace GameTesting;

    constexpr uint64 WizardId = 7002;

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
