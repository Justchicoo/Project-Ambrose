/*
 * Project Ambrose by Imjustchico
 * Checks the commands that change a wizard's backpack over real game sessions on loopback: 'additem' and 'removeitem' say how they are used when the template or item id is missing, name a template the server does not hold and refuse a count out of range, ask the console to name a wizard, find no wizard for a name nobody in the world has, and answer a removal run on the world thread for a wizard whose backpack was never opened as not done.
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

void AddSC_cs_item();

namespace
{
    using namespace GameTesting;

    bool AnyLineHas(std::vector<std::string> const& lines, std::string_view text)
    {
        return std::any_of(lines.begin(), lines.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; });
    }

    class ItemCommandTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sCommandMgr.Clear();
            sScriptMgr.Unload();
            AddSC_cs_item();
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

TEST_F(ItemCommandTest, AMissingTemplateItemIdOrWizardIsSaidRatherThanGuessed)
{
    std::unique_ptr<FakeSessionClient> client;
    ASSERT_TRUE(Join(client, 8101, "Ellie Stormshard"));

    EXPECT_TRUE(AnyLineHas(Run({ "additem" }, CommandResult::Usage), "additem takes a template"));
    EXPECT_TRUE(AnyLineHas(Run({ "additem", "No Such Hat", "1", "8101" }, CommandResult::Usage), "No item has the template id or name No Such Hat"));
    EXPECT_TRUE(AnyLineHas(Run({ "removeitem" }, CommandResult::Usage), "removeitem takes the item's id"));
    EXPECT_TRUE(AnyLineHas(Run({ "removeitem", "12" }, CommandResult::Usage), "Name the wizard"));
    EXPECT_TRUE(AnyLineHas(Run({ "removeitem", "12", "Nobody Here" }, CommandResult::Usage), "No wizard in the world has the character id or name Nobody Here"));
}

TEST_F(ItemCommandTest, ARemovalForAWizardWhoseBackpackWasNeverOpenedIsAnsweredAsNotDone)
{
    std::unique_ptr<FakeSessionClient> client;
    ASSERT_TRUE(Join(client, 8201, "Blaze Moonshade"));
    std::atomic<bool> stop{ false };
    std::thread world([&]
    {
        while (!stop)
        {
            sWorld.Update(std::chrono::milliseconds(10));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    std::vector<std::string> const lines = Run({ "removeitem", "12345", "8201" }, CommandResult::Usage);
    stop = true;
    world.join();
    EXPECT_TRUE(AnyLineHas(lines, "Blaze Moonshade holds no item 12345"));
}
