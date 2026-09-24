/*
 * Project Ambrose by Imjustchico
 * Tests the hook framework every later domain hangs off: a script registers itself by being constructed, the loader CMake wrote brings in the scripts that are merely present in the source tree, every hook reaches every script in the order they registered, a module under modules/ arrives by the same loader with no edit to anything in the core, a script that throws from a hook is reported and the scripts after it still run, and unloading frees them and leaves the manager empty.
 */

#include "ScriptLoader.h"
#include "ScriptMgr.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    std::vector<std::string> Calls;

    class CountingScript : public WorldScript
    {
    public:
        explicit CountingScript(std::string name) : WorldScript(std::move(name)) {}

        void OnStartup() override { Calls.push_back(GetName() + ":startup"); }
        void OnShutdown() override { Calls.push_back(GetName() + ":shutdown"); }
        void OnConfigLoad(bool reload) override { Calls.push_back(GetName() + (reload ? ":reload" : ":config")); }
        void OnUpdate(std::chrono::milliseconds diff) override { Calls.push_back(GetName() + ":update:" + std::to_string(diff.count())); }
    };

    class ThrowingScript : public WorldScript
    {
    public:
        ThrowingScript() : WorldScript("throwing") {}

        void OnUpdate(std::chrono::milliseconds) override { throw std::runtime_error("this script is broken"); }
    };

    class ScriptMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sScriptMgr.Unload();
            Calls.clear();
        }

        void TearDown() override
        {
            sScriptMgr.Unload();
            Calls.clear();
        }
    };
}

TEST_F(ScriptMgrTest, AScriptRegistersItselfByBeingConstructed)
{
    EXPECT_EQ(sScriptMgr.GetScriptCount(), 0u);
    new CountingScript("first");
    EXPECT_EQ(sScriptMgr.GetScriptCount(), 1u);
    new CountingScript("second");
    EXPECT_EQ(sScriptMgr.GetScriptCount(), 2u);
    EXPECT_EQ(sScriptMgr.GetScriptNames(), (std::vector<std::string>{ "first", "second" }));
}

TEST_F(ScriptMgrTest, EveryHookReachesEveryScriptInTheOrderTheyRegistered)
{
    new CountingScript("first");
    new CountingScript("second");

    sScriptMgr.OnConfigLoad(false);
    sScriptMgr.OnStartup();
    sScriptMgr.OnWorldUpdate(std::chrono::milliseconds(50));
    sScriptMgr.OnWorldUpdate(std::chrono::milliseconds(50));
    sScriptMgr.OnConfigLoad(true);
    sScriptMgr.OnShutdown();

    EXPECT_EQ(Calls, (std::vector<std::string>{
        "first:config", "second:config",
        "first:startup", "second:startup",
        "first:update:50", "second:update:50",
        "first:update:50", "second:update:50",
        "first:reload", "second:reload",
        "first:shutdown", "second:shutdown",
    }));
}

TEST_F(ScriptMgrTest, AScriptThatThrowsDoesNotStopTheOnesAfterIt)
{
    new CountingScript("before");
    new ThrowingScript();
    new CountingScript("after");

    sScriptMgr.OnWorldUpdate(std::chrono::milliseconds(10));

    EXPECT_EQ(Calls, (std::vector<std::string>{ "before:update:10", "after:update:10" }));
    EXPECT_EQ(sScriptMgr.GetScriptCount(), 3u);
}

TEST_F(ScriptMgrTest, UnloadingFreesEveryScriptAndLeavesNothingBehind)
{
    new CountingScript("first");
    new CountingScript("second");
    ASSERT_EQ(sScriptMgr.GetScriptCount(), 2u);

    sScriptMgr.Unload();
    EXPECT_EQ(sScriptMgr.GetScriptCount(), 0u);
    EXPECT_TRUE(sScriptMgr.GetScriptNames().empty());

    sScriptMgr.OnWorldUpdate(std::chrono::milliseconds(10));
    EXPECT_TRUE(Calls.empty());
}

TEST_F(ScriptMgrTest, TheGeneratedLoaderBringsInTheScriptsThatAreMerelyPresent)
{
    ASSERT_EQ(sScriptMgr.GetScriptCount(), 0u);
    sScriptMgr.LoadScripts(&AddScripts);

    std::vector<std::string> const names = sScriptMgr.GetScriptNames();
    EXPECT_FALSE(names.empty()) << "CMake found no AddSC function in src/server/scripts";
    EXPECT_NE(std::find(names.begin(), names.end(), "world_heartbeat"), names.end())
        << "the loader CMake wrote did not call AddSC_world_heartbeat";

    std::size_t const loaded = sScriptMgr.GetScriptCount();
    sScriptMgr.LoadScripts(&AddScripts);
    EXPECT_EQ(sScriptMgr.GetScriptCount(), loaded) << "loading twice registered the scripts twice";

    sScriptMgr.OnWorldUpdate(std::chrono::milliseconds(50));
}

TEST_F(ScriptMgrTest, AModuleUnderModulesIsLoadedTheSameWayAScriptIs)
{
    ASSERT_EQ(sScriptMgr.GetScriptCount(), 0u);
    sScriptMgr.LoadScripts(&AddScripts);

    std::vector<std::string> const names = sScriptMgr.GetScriptNames();
    EXPECT_NE(std::find(names.begin(), names.end(), "example_module"), names.end())
        << "CMake did not find the loader in modules/example, so a module needs a core edit after all";
}
