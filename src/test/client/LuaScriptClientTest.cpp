/*
 * Project Ambrose by Imjustchico
 * Reads the user's install's Lua scripts and runs the census of each, which every one but a script the install ships broken passes, finds that CompleteWC.lua and CompleteKT.lua, the scripts the old quest data's ResSyncScript results call, need only ScriptCompleteQuest, CompleteQuestChain and GrantXP from the host, and that the server reaches its own binding of them when it runs OpenKrok and OpenMarleybone, skipping without a client installation.
 */

#include "Environment.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "LuaScriptLibrary.h"
#include "LuaScriptMgr.h"
#include "SessionScripts.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
    std::optional<std::filesystem::path> Install()
    {
        std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
        if (!client || client->empty())
            return std::nullopt;
        return LogConfig::Utf8Path(*client);
    }
}

TEST(LuaScriptClientTest, EveryInstallScriptButOneShippedBrokenCompilesAndRunsItsTopLevel)
{
    std::optional<std::filesystem::path> const install = Install();
    if (!install)
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install to run this test";
    std::string error;
    std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(*install / "Data" / "GameData" / "Root.wad", error);
    ASSERT_TRUE(root) << error;
    std::vector<std::string> errors;
    std::shared_ptr<LuaScriptLibrary const> const library = LuaScriptLibrary::Read(*root, errors);
    ASSERT_TRUE(library) << errors.front();
    ASSERT_GT(library->Size(), 0u);
    std::vector<std::string> broken;
    std::size_t loaded = 0;
    for (std::string const& name : library->GetNames())
    {
        LuaScriptCensus const census = LuaState::Census(LuaScriptLibrary::ReaderOf(library), name);
        if (!census.Compiled)
            broken.push_back(name);
        else if (census.Loaded)
            ++loaded;
    }
    EXPECT_LE(broken.size(), 1u) << broken.front();
    EXPECT_GE(loaded + broken.size() + 2, library->Size()) << "at most two scripts stop at their top level on an include the install lacks";
    std::cout << "[ LUA SCRIPTS ] " << library->Size() << " scripts, " << loaded << " ran their top level, " << broken.size() << " shipped broken" << std::endl;

    for (char const* const script : { "WorldBuilding/CompleteWC.lua", "WorldBuilding/CompleteKT.lua" })
    {
        LuaScriptCensus const census = LuaState::Census(LuaScriptLibrary::ReaderOf(library), script);
        EXPECT_TRUE(census.Loaded) << script << ": " << census.Error;
        EXPECT_EQ(census.HostGlobals, (std::set<std::string, std::less<>>{ "CompleteQuestChain", "GrantXP", "ScriptCompleteQuest" })) << script;
    }
}

TEST(LuaScriptClientTest, TheServerReachesItsOwnBindingOfTheWorldBuildingScriptsFunctions)
{
    std::optional<std::filesystem::path> const install = Install();
    if (!install)
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install to run this test";
    sLuaScriptMgr.SetInstall(*install);
    std::vector<std::string> errors;
    ASSERT_TRUE(sLuaScriptMgr.Load(errors)) << errors.front();
    SessionScripts::Register();
    for (auto const& [script, function] : { std::pair{ "Scripts/WorldBuilding/CompleteWC.lua", "OpenKrok" }, std::pair{ "Scripts/WorldBuilding/CompleteKT.lua", "OpenMarleybone" } })
    {
        LuaScriptCall call;
        call.Script = script;
        call.Function = function;
        call.CharacterId = 1;
        call.GameObjectId = 2;
        std::string error;
        EXPECT_FALSE(sLuaScriptMgr.Call(call, error)) << "no session is loaded for the call";
        EXPECT_NE(error.find("the script names a wizard this call does not run for"), std::string::npos) << script << ": " << error;
    }
}
