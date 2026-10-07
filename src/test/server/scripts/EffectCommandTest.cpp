/*
 * Project Ambrose by Imjustchico
 * Checks the effect group over real game sessions on loopback, with game effect templates read from a Root.wad the test writes: each command says how it is used when what it needs is missing, an effect the server does not hold is named with the names that hold the text, a wizard nobody in the world is is said, 'effect info' describes a template, 'effect add' on the world thread refuses a wizard that stands on no map rather than sending an effect no client can place, and 'effect list' and 'effect remove' answer for a wizard that carries none.
 */

#include "AccountMgr.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "GameEffectFixtures.h"
#include "GameEffectMgr.h"
#include "GameTestHarness.h"
#include "LogTestDirectory.h"
#include "ScriptMgr.h"
#include "TypeRegistry.h"
#include "TypedView.h"
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

void AddSC_cs_effect();

namespace
{
    using namespace GameTesting;

    bool AnyLineHas(std::vector<std::string> const& lines, std::string_view text)
    {
        return std::any_of(lines.begin(), lines.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; });
    }

    class EffectCommandTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sCommandMgr.Clear();
            sScriptMgr.Unload();
            AddSC_cs_effect();
            sCommandMgr.Load(sScriptMgr.GetCommands());
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            CharacterTypeFixtures::Detail::Json dump = CharacterTypeFixtures::Detail::Json::parse(CharacterTypeFixtures::Dump());
            GameEffectFixtures::AddClasses(dump["classes"]);
            GameEffectFixtures::AddTemplateClasses(dump["classes"]);
            ASSERT_TRUE(sTypeRegistry.LoadFromText(dump.dump(), "effects.json")) << sTypeRegistry.GetErrors().front();
            TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
            std::vector<PropertyObjectPtr> templates;
            templates.push_back(GameEffectFixtures::Template(catalog, "class NamedEffectTemplate", "PostCombatEffect", 30.0, true, false));
            templates.push_back(GameEffectFixtures::Template(catalog, "class NamedEffectTemplate", "PostCombatGlow", 10.0, true, false));
            GameEffectFixtures::WriteRoot(_directory.Path(), { { "GameEffectData/NamedEffects.xml", GameEffectFixtures::ListFile(catalog, std::move(templates)) } });
            sGameEffectMgr.SetInstall(_directory.Path());
            std::vector<std::string> errors;
            ASSERT_TRUE(sGameEffectMgr.Load(errors)) << errors.front();
        }

        void TearDown() override
        {
            sWorld.Clear();
            sCommandMgr.Clear();
            sScriptMgr.Unload();
            sGameEffectMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&sTypedViewRegistry);
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

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        GameDefinitions _definitions;
        GameListener _server;
    };
}

TEST_F(EffectCommandTest, WhatIsMissingOrUnknownIsSaidRatherThanGuessed)
{
    EXPECT_TRUE(AnyLineHas(Run({ "effect", "info" }, CommandResult::Usage), "effect info takes a game effect's name or template id"));
    EXPECT_TRUE(AnyLineHas(Run({ "effect", "add", "PostCombatEffect" }, CommandResult::Usage), "and then the wizard"));
    EXPECT_TRUE(AnyLineHas(Run({ "effect", "remove", "one", "555" }, CommandResult::Usage), "effect remove takes the internal id"));
    EXPECT_TRUE(AnyLineHas(Run({ "effect", "list" }, CommandResult::Usage), "effect list takes the wizard"));
    EXPECT_TRUE(AnyLineHas(Run({ "effect", "add", "PostCombatEffect", "Nobody Here" }, CommandResult::Usage), "No wizard in the world has the character id or name Nobody Here"));
    std::vector<std::string> const similar = Run({ "effect", "info", "PostCombat" }, CommandResult::Usage);
    EXPECT_TRUE(AnyLineHas(similar, "No game effect is named PostCombat, but 2 hold(s) it in their names"));
    EXPECT_TRUE(AnyLineHas(similar, "PostCombatGlow"));
    EXPECT_TRUE(AnyLineHas(Run({ "effect", "info", "postcombateffect" }, CommandResult::Ran), "lasts 30 s, seen by everyone, shown on the wizard"));
}

TEST_F(EffectCommandTest, AWizardOnNoMapIsRefusedAndOneWithoutEffectsListsNone)
{
    std::unique_ptr<FakeSessionClient> client;
    ASSERT_TRUE(Join(client, 7401, "Morgan Ravenflame"));
    std::atomic<bool> stop{ false };
    std::thread world([&]
    {
        while (!stop)
        {
            sWorld.Update(std::chrono::milliseconds(10));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    std::vector<std::string> const added = Run({ "effect", "add", "PostCombatEffect", "7401" }, CommandResult::Usage);
    std::vector<std::string> const listed = Run({ "effect", "list", "Morgan", "Ravenflame" }, CommandResult::Ran);
    std::vector<std::string> const removed = Run({ "effect", "remove", "1", "7401" }, CommandResult::Usage);
    stop = true;
    world.join();
    EXPECT_TRUE(AnyLineHas(added, "Morgan Ravenflame cannot carry PostCombatEffect: the wizard is not in the world"));
    EXPECT_TRUE(AnyLineHas(listed, "Morgan Ravenflame carries no effect"));
    EXPECT_TRUE(AnyLineHas(removed, "Morgan Ravenflame carries no effect with internal id 1"));
}
