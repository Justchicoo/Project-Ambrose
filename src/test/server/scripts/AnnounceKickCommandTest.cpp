/*
 * Project Ambrose by Imjustchico
 * Checks the commands a game master reaches a wizard in the world with, over real game sessions on loopback: 'server announce' sends MSG_SERVERMESSAGE with the typed text to every session in the world and to none still entering, and says how many it reached; 'kick' finds a wizard by character id or by its name in any case, sends MSG_FORCE_DISCONNECT with the CSR reason the client shows a game master's disconnect for and closes the connection, never touches a session not yet in the world, says so when nobody matches, and when two wizards share the name kicks neither and lists their ids.
 */

#include "AccountMgr.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "DisconnectReason.h"
#include "GameTestHarness.h"
#include "ScriptMgr.h"
#include "SystemMessages.h"
#include "World.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

void AddSC_cs_server();
void AddSC_cs_misc();

namespace
{
    using namespace GameTesting;

    bool AnyLineHas(std::vector<std::string> const& lines, std::string_view text)
    {
        return std::any_of(lines.begin(), lines.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; });
    }

    template<DeclaredMessage T>
    std::optional<T> ReadMessage(FakeSessionClient& client)
    {
        std::optional<DmlMessageData> const received = ReadNextDml(client);
        if (!received || !Is<T>(*received))
            return std::nullopt;
        T message;
        if (sMessageRegistry.GetCatalog()->Decode(received->Body, message) != MessageDecodeStatus::Ok)
            return std::nullopt;
        return message;
    }

    class AnnounceKickCommandTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sCommandMgr.Clear();
            sScriptMgr.Unload();
            AddSC_cs_server();
            AddSC_cs_misc();
            sCommandMgr.Load(sScriptMgr.GetCommands());
        }

        void TearDown() override
        {
            sWorld.Clear();
            sCommandMgr.Clear();
            sScriptMgr.Unload();
        }

        std::shared_ptr<GameSession> Join(std::unique_ptr<FakeSessionClient>& client, uint64 characterId, std::string name, SessionStatus status)
        {
            uint16 sessionId = 0;
            client = _server.Connect(sessionId);
            std::shared_ptr<GameSession> session;
            EXPECT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
            if (!session)
                return nullptr;
            session->SetCharacterId(characterId);
            session->SetCharacterName(std::move(name));
            session->SetStatus(status);
            sWorld.AddSession(session);
            return session;
        }

        static std::vector<std::string> Run(std::string_view line, CommandResult expected = CommandResult::Ran)
        {
            RecordingCaller caller(SEC_GAMEMASTER, true, "console");
            EXPECT_EQ(sCommandMgr.Execute(caller, line), expected) << line;
            return caller.GetLines();
        }

        GameDefinitions _definitions;
        GameListener _server;
    };
}

TEST_F(AnnounceKickCommandTest, AnAnnouncementReachesEveryWizardInTheWorldAndNoOneStillEntering)
{
    std::unique_ptr<FakeSessionClient> zoned;
    std::unique_ptr<FakeSessionClient> loading;
    std::unique_ptr<FakeSessionClient> entering;
    ASSERT_TRUE(Join(zoned, 7001, "Aaron Stormblade", SessionStatus::InWorld));
    ASSERT_TRUE(Join(loading, 7002, "Abby Rider", SessionStatus::LoggedIn));
    ASSERT_TRUE(Join(entering, 7003, "", SessionStatus::Connected));

    EXPECT_TRUE(AnyLineHas(Run("server announce The Commons closes at dusk"), "Shown to 2 wizard(s) in the world"));
    for (FakeSessionClient* client : { zoned.get(), loading.get() })
    {
        std::optional<SystemMessages::ServerMessage> const shown = ReadMessage<SystemMessages::ServerMessage>(*client);
        ASSERT_TRUE(shown);
        EXPECT_EQ(shown->Message, u"The Commons closes at dusk");
        EXPECT_EQ(shown->Modal, 0);
    }
    EXPECT_FALSE(ReadNextDml(*entering, std::chrono::milliseconds(500))) << "a client that has not been handed its wizard is not in the world to be told";

    EXPECT_TRUE(AnyLineHas(Run("server announce", CommandResult::Usage), "Give the message to show"));
}

TEST_F(AnnounceKickCommandTest, AKickFindsTheWizardByNameOrIdAndGivesTheClientTheGameMastersReason)
{
    std::unique_ptr<FakeSessionClient> named;
    std::unique_ptr<FakeSessionClient> numbered;
    std::shared_ptr<GameSession> const byName = Join(named, 7001, "Aaron Stormblade", SessionStatus::InWorld);
    std::shared_ptr<GameSession> const byId = Join(numbered, 7002, "Abby Rider", SessionStatus::InWorld);
    ASSERT_TRUE(byName);
    ASSERT_TRUE(byId);

    EXPECT_TRUE(AnyLineHas(Run("kick aaron STORMBLADE"), "Disconnected Aaron Stormblade (character 7001)"));
    std::optional<SystemMessages::ForceDisconnect> const kicked = ReadMessage<SystemMessages::ForceDisconnect>(*named);
    ASSERT_TRUE(kicked);
    EXPECT_EQ(kicked->Type, DisconnectReason::Csr) << "the client shows a game master's disconnect only for the hash of CSR";
    EXPECT_EQ(kicked->Message, "Disconnected by console");
    EXPECT_TRUE(named->WaitForClose());
    EXPECT_TRUE(byName->IsKicked());

    EXPECT_TRUE(AnyLineHas(Run("kick 7002"), "Disconnected Abby Rider (character 7002)"));
    std::optional<SystemMessages::ForceDisconnect> const second = ReadMessage<SystemMessages::ForceDisconnect>(*numbered);
    ASSERT_TRUE(second);
    EXPECT_EQ(second->Type, DisconnectReason::Csr);
    EXPECT_TRUE(numbered->WaitForClose());
}

TEST_F(AnnounceKickCommandTest, AKickTouchesNobodyWhenNoneOrTwoWizardsMatch)
{
    std::unique_ptr<FakeSessionClient> first;
    std::unique_ptr<FakeSessionClient> second;
    std::unique_ptr<FakeSessionClient> entering;
    std::shared_ptr<GameSession> const one = Join(first, 7001, "Aaron Stormblade", SessionStatus::InWorld);
    std::shared_ptr<GameSession> const two = Join(second, 7002, "Aaron Stormblade", SessionStatus::InWorld);
    std::shared_ptr<GameSession> const waiting = Join(entering, 7003, "Abby Rider", SessionStatus::Connected);
    ASSERT_TRUE(one);
    ASSERT_TRUE(two);
    ASSERT_TRUE(waiting);

    std::vector<std::string> const shared = Run("kick Aaron Stormblade", CommandResult::Usage);
    EXPECT_TRUE(AnyLineHas(shared, "2 wizards in the world are named Aaron Stormblade"));
    EXPECT_TRUE(AnyLineHas(shared, "7001 on session"));
    EXPECT_TRUE(AnyLineHas(shared, "7002 on session"));
    EXPECT_TRUE(AnyLineHas(Run("kick Abby Rider", CommandResult::Usage), "No wizard in the world has the character id or name Abby Rider"))
        << "a session still entering the world is not kicked by this command";
    EXPECT_TRUE(AnyLineHas(Run("kick", CommandResult::Usage), "Give the character id or the name"));
    EXPECT_FALSE(one->IsKicked());
    EXPECT_FALSE(two->IsKicked());
    EXPECT_FALSE(waiting->IsKicked());
    EXPECT_FALSE(ReadNextDml(*first, std::chrono::milliseconds(300)));
}
