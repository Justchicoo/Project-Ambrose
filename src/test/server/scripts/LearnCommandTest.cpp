/*
 * Project Ambrose by Imjustchico
 * Checks the commands that change a wizard's spellbook over real game sessions on loopback: 'learn' and 'unlearn' say how they are used when the spell or the wizard is missing, find no wizard for a name nobody in the world has, list the ids to choose between when two wizards share a name, and name a spell the server does not hold; an unlearn given on another thread is handed to the world thread, answered once it has run there, and answered as not done when no world thread takes it within the command timeout.
 */

#include "AccountMgr.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "GameTestHarness.h"
#include "ScriptMgr.h"
#include "World.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

void AddSC_cs_learn();

namespace
{
    using namespace GameTesting;

    bool AnyLineHas(std::vector<std::string> const& lines, std::string_view text)
    {
        return std::any_of(lines.begin(), lines.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; });
    }

    class LearnCommandTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sCommandMgr.Clear();
            sScriptMgr.Unload();
            AddSC_cs_learn();
            sCommandMgr.Load(sScriptMgr.GetCommands());
        }

        void TearDown() override
        {
            sWorld.Clear();
            sCommandMgr.Clear();
            sScriptMgr.Unload();
        }

        std::shared_ptr<GameSession> Join(std::unique_ptr<FakeSessionClient>& client, uint64 characterId, std::string name)
        {
            uint16 sessionId = 0;
            client = _server.Connect(sessionId);
            std::shared_ptr<GameSession> session;
            EXPECT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
            if (!session)
                return nullptr;
            session->SetCharacterId(characterId);
            session->SetCharacterName(std::move(name));
            session->SetStatus(SessionStatus::InWorld);
            sWorld.AddSession(session);
            return session;
        }

        static std::vector<std::string> Run(std::vector<std::string> words, CommandResult expected)
        {
            RecordingCaller caller(SEC_GAMEMASTER, true, "console");
            EXPECT_EQ(sCommandMgr.Execute(caller, std::move(words)), expected);
            return caller.GetLines();
        }

        GameDefinitions _definitions;
        GameListener _server;
    };
}

TEST_F(LearnCommandTest, ASpellOrAWizardThatIsMissingOrSharedIsSaidRatherThanGuessed)
{
    std::unique_ptr<FakeSessionClient> first;
    std::unique_ptr<FakeSessionClient> second;
    ASSERT_TRUE(Join(first, 7101, "Tyler Firecaller"));
    ASSERT_TRUE(Join(second, 7102, "Tyler Firecaller"));

    EXPECT_TRUE(AnyLineHas(Run({ "learn" }, CommandResult::Usage), "learn takes a spell"));
    EXPECT_TRUE(AnyLineHas(Run({ "learn", "Fire Cat" }, CommandResult::Usage), "and then the wizard"));
    EXPECT_TRUE(AnyLineHas(Run({ "learn", "Fire Cat", "Nobody Here" }, CommandResult::Usage), "No wizard in the world has the character id or name Nobody Here"));
    std::vector<std::string> const shared = Run({ "unlearn", "Fire Cat", "tyler firecaller" }, CommandResult::Usage);
    EXPECT_TRUE(AnyLineHas(shared, "2 wizards in the world are named tyler firecaller"));
    EXPECT_TRUE(AnyLineHas(shared, "7101 on session"));
    EXPECT_TRUE(AnyLineHas(shared, "7102 on session"));
    EXPECT_TRUE(AnyLineHas(Run({ "learn", "No Such Spell", "7101" }, CommandResult::Usage), "No spell has the template id or name No Such Spell"))
        << "a spell the server does not hold is named, and nothing is sent";
}

TEST_F(LearnCommandTest, AnUnlearnFromAnotherThreadRunsOnTheWorldThreadAndIsAnsweredOnceItHas)
{
    std::unique_ptr<FakeSessionClient> client;
    ASSERT_TRUE(Join(client, 7201, "Ellie Stormshard"));
    std::atomic<bool> stop{ false };
    std::atomic<bool> ranOnWorld{ false };
    std::thread world([&]
    {
        while (!stop)
        {
            sWorld.Update(std::chrono::milliseconds(10));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    std::vector<std::string> const lines = Run({ "unlearn", "12345", "7201" }, CommandResult::Usage);
    std::shared_ptr<GameSession> const session = sWorld.FindInWorld("7201").front();
    EXPECT_TRUE(sWorld.RunFor(session, [&ranOnWorld](GameSession&) { ranOnWorld = sWorld.IsWorldThread(); }, World::CommandTimeout));
    stop = true;
    world.join();
    EXPECT_TRUE(AnyLineHas(lines, "Ellie Stormshard left the world before it could lose spell 12345"))
        << "a wizard whose book was never opened has none to change, which the world thread answers";
    EXPECT_TRUE(ranOnWorld) << "the work ran on the world thread, not the caller's";
}

TEST_F(LearnCommandTest, AChangeNoWorldThreadTakesIsAnsweredAsNotDone)
{
    std::unique_ptr<FakeSessionClient> client;
    ASSERT_TRUE(Join(client, 7301, "Blaze Moonshade"));
    std::thread asker([] { EXPECT_TRUE(AnyLineHas(Run({ "unlearn", "12345", "7301" }, CommandResult::Usage), "The world did not answer within 5 s")); });
    asker.join();
}
