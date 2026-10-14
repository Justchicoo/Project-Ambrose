/*
 * Project Ambrose by Imjustchico
 * Tests the sandboxed Lua state on scripts the test hands it: Include runs a script and its includes once each by name, whatever the case of the Scripts folder prefix; the sandbox leaves out io, os, debug, dofile, loadfile, load, loadstring, require and string.dump and refuses precompiled bytecode; a call past its instruction cap or memory cap stops with the reason while the state stays usable; a vararg function that names arg reads its arguments as Lua 5.1 handed them; an unknown global reaches the fallback; Log reaches the sink; and the census names the globals a script and its includes read and never set.
 */

#include "LuaState.h"

#include <gtest/gtest.h>
#include <lua.hpp>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace
{
    struct Scripts
    {
        std::map<std::string, std::string> Sources;
        std::vector<std::string> Reads;

        LuaState::SourceReader Reader()
        {
            return [this](std::string_view name) -> std::optional<std::string>
            {
                Reads.emplace_back(name);
                auto const found = Sources.find(std::string(name));
                return found == Sources.end() ? std::nullopt : std::optional<std::string>(found->second);
            };
        }
    };

    std::unique_ptr<LuaState> Open(Scripts& scripts, std::vector<std::string>* lines = nullptr, LuaLimits limits = {})
    {
        std::string error;
        std::unique_ptr<LuaState> state = LuaState::Create(scripts.Reader(), [lines](std::string_view line)
        {
            if (lines)
                lines->emplace_back(line);
        }, limits, error);
        EXPECT_TRUE(state) << error;
        return state;
    }

    bool CallGlobal(LuaState& state, char const* function, std::string& error)
    {
        lua_getglobal(state.Get(), function);
        return state.Call(0, error);
    }
}

TEST(LuaStateTest, NameOfDropsTheScriptsFolderWhateverItsCaseOrSlashes)
{
    EXPECT_EQ(LuaState::NameOf("Scripts/WorldBuilding/CompleteWC.lua"), "WorldBuilding/CompleteWC.lua");
    EXPECT_EQ(LuaState::NameOf("scripts\\WorldBuilding\\CompleteWC.lua"), "WorldBuilding/CompleteWC.lua");
    EXPECT_EQ(LuaState::NameOf("WorldBuilding/CompleteWC.lua"), "WorldBuilding/CompleteWC.lua");
    EXPECT_EQ(LuaState::NameOf("/Scripts/class.lua"), "class.lua");
}

TEST(LuaStateTest, IncludeRunsEachScriptOnceByName)
{
    Scripts scripts;
    scripts.Sources["main.lua"] = "Include('Scripts/common.lua') Include('common.lua') Count = Count + 1";
    scripts.Sources["common.lua"] = "Count = (Count or 0) + 10";
    std::unique_ptr<LuaState> const state = Open(scripts);
    ASSERT_TRUE(state);
    std::string error;
    ASSERT_TRUE(state->Include("main.lua", error)) << error;
    EXPECT_TRUE(state->Include("Scripts/main.lua", error)) << error;
    lua_getglobal(state->Get(), "Count");
    EXPECT_EQ(lua_tointeger(state->Get(), -1), 11) << "common.lua and main.lua each ran once";
    lua_pop(state->Get(), 1);
    EXPECT_EQ(scripts.Reads, (std::vector<std::string>{ "main.lua", "common.lua" }));
}

TEST(LuaStateTest, AMissingIncludeOrAScriptThatDoesNotCompileIsAnError)
{
    Scripts scripts;
    scripts.Sources["main.lua"] = "Include('gone.lua')";
    scripts.Sources["broken.lua"] = "function (";
    std::unique_ptr<LuaState> const state = Open(scripts);
    ASSERT_TRUE(state);
    std::string error;
    EXPECT_FALSE(state->Include("main.lua", error));
    EXPECT_NE(error.find("Scripts/gone.lua is not in the install"), std::string::npos) << error;
    error.clear();
    EXPECT_FALSE(state->Include("broken.lua", error));
    EXPECT_NE(error.find("Scripts/broken.lua"), std::string::npos) << error;
}

TEST(LuaStateTest, TheSandboxLeavesOutFilesTheSystemAndBytecode)
{
    Scripts scripts;
    scripts.Sources["probe.lua"] =
        "Found = {} "
        "for _, name in ipairs({ 'io', 'os', 'debug', 'dofile', 'loadfile', 'load', 'loadstring', 'require', 'module', 'print', 'package' }) do "
        "  if rawget(_G, name) ~= nil then table.insert(Found, name) end "
        "end "
        "if string.dump ~= nil then table.insert(Found, 'string.dump') end "
        "Kept = type(string.format) == 'function' and type(table.insert) == 'function' and type(math.floor) == 'function' and type(bit.band) == 'function' "
        "Found = table.concat(Found, ',')";
    std::unique_ptr<LuaState> const state = Open(scripts);
    ASSERT_TRUE(state);
    std::string error;
    ASSERT_TRUE(state->Include("probe.lua", error)) << error;
    lua_getglobal(state->Get(), "Found");
    EXPECT_STREQ(lua_tostring(state->Get(), -1), "");
    lua_getglobal(state->Get(), "Kept");
    EXPECT_TRUE(lua_toboolean(state->Get(), -1));
    lua_pop(state->Get(), 2);

    scripts.Sources["compiled.lua"] = std::string(LUA_SIGNATURE) + "rest";
    EXPECT_FALSE(state->Include("compiled.lua", error));
    EXPECT_NE(error.find("precompiled bytecode"), std::string::npos) << error;
}

TEST(LuaStateTest, ACallPastItsInstructionCapStopsAndTheStateStaysUsable)
{
    Scripts scripts;
    scripts.Sources["loop.lua"] = "function Spin() while true do end end function Answer() Result = 42 end";
    std::unique_ptr<LuaState> const state = Open(scripts, nullptr, LuaLimits{ 32 * 1024 * 1024, 1'000'000 });
    ASSERT_TRUE(state);
    std::string error;
    ASSERT_TRUE(state->Include("loop.lua", error)) << error;
    EXPECT_FALSE(CallGlobal(*state, "Spin", error));
    EXPECT_NE(error.find("limit of 1 million instructions"), std::string::npos) << error;
    error.clear();
    EXPECT_TRUE(CallGlobal(*state, "Answer", error)) << "each call has its own count: " << error;
    lua_getglobal(state->Get(), "Result");
    EXPECT_EQ(lua_tointeger(state->Get(), -1), 42);
    lua_pop(state->Get(), 1);
}

TEST(LuaStateTest, ACallPastItsMemoryCapStopsWithTheReason)
{
    Scripts scripts;
    scripts.Sources["grow.lua"] = "function Grow() local t = {} for i = 1, 100000000 do t[i] = string.rep('x', 64) .. i end end";
    std::unique_ptr<LuaState> const state = Open(scripts, nullptr, LuaLimits{ 4 * 1024 * 1024, 1'000'000'000 });
    ASSERT_TRUE(state);
    std::string error;
    ASSERT_TRUE(state->Include("grow.lua", error)) << error;
    EXPECT_FALSE(CallGlobal(*state, "Grow", error));
    EXPECT_NE(error.find("ran out of its 4 MB of memory"), std::string::npos) << error;
}

TEST(LuaStateTest, AVarargFunctionThatNamesArgReadsItsArgumentsAsLua51HandedThem)
{
    Scripts scripts;
    scripts.Sources["class.lua"] = "function Pack(...) return arg.n, unpack(arg) end function Run() Count, First, Second = Pack('a', 'b') end";
    std::unique_ptr<LuaState> const state = Open(scripts);
    ASSERT_TRUE(state);
    std::string error;
    ASSERT_TRUE(state->Include("class.lua", error)) << error;
    ASSERT_TRUE(CallGlobal(*state, "Run", error)) << error;
    lua_State* const lua = state->Get();
    lua_getglobal(lua, "Count");
    lua_getglobal(lua, "First");
    lua_getglobal(lua, "Second");
    EXPECT_EQ(lua_tointeger(lua, -3), 2);
    EXPECT_STREQ(lua_tostring(lua, -2), "a");
    EXPECT_STREQ(lua_tostring(lua, -1), "b");
    lua_pop(lua, 3);
}

TEST(LuaStateTest, AnUnknownGlobalReachesTheFallbackAndLogReachesTheSink)
{
    Scripts scripts;
    scripts.Sources["main.lua"] = "Seen = Missing Log('ran', 3, true, {})";
    std::vector<std::string> lines;
    std::unique_ptr<LuaState> const state = Open(scripts, &lines);
    ASSERT_TRUE(state);
    lua_State* const lua = state->Get();
    lua_pushcfunction(lua, [](lua_State* inner) -> int
    {
        lua_pushfstring(inner, "fallback:%s", lua_tostring(inner, 2));
        return 1;
    });
    state->SetGlobalFallback();
    std::string error;
    ASSERT_TRUE(state->Include("main.lua", error)) << error;
    lua_getglobal(lua, "Seen");
    EXPECT_STREQ(lua_tostring(lua, -1), "fallback:Missing");
    lua_pop(lua, 1);
    EXPECT_EQ(lines, (std::vector<std::string>{ "ran\t3\ttrue\ttable" }));
}

TEST(LuaStateTest, TheCensusNamesTheGlobalsAScriptAndItsIncludesReadAndNeverSet)
{
    Scripts scripts;
    scripts.Sources["main.lua"] = "Include('helper.lua') function Run(args) Helper() GrantXP(args.CharacterID, 10) return table.getn({}) end Ready = OwnerGID";
    scripts.Sources["helper.lua"] = "function Helper() local x = ScriptCompleteQuest return Ready end";
    LuaScriptCensus const census = LuaState::Census(scripts.Reader(), "Scripts/main.lua");
    EXPECT_TRUE(census.Loaded) << census.Error;
    EXPECT_TRUE(census.Compiled);
    EXPECT_EQ(census.Files, (std::vector<std::string>{ "main.lua", "helper.lua" }));
    EXPECT_EQ(census.HostGlobals, (std::set<std::string, std::less<>>{ "GrantXP", "OwnerGID", "ScriptCompleteQuest" }));
}

TEST(LuaStateTest, TheCensusOfAScriptThatDoesNotCompileSaysSo)
{
    Scripts scripts;
    scripts.Sources["broken.lua"] = "function (";
    LuaScriptCensus const census = LuaState::Census(scripts.Reader(), "broken.lua");
    EXPECT_FALSE(census.Loaded);
    EXPECT_FALSE(census.Compiled);
    EXPECT_FALSE(census.Error.empty());
}
