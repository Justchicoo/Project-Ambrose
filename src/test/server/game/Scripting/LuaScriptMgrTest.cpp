/*
 * Project Ambrose by Imjustchico
 * Tests the server's Lua host on a Root.wad the test writes: it reads the install's scripts and the reload target scripts swaps in an edited Root.wad; a call runs the named function with CharacterID, GameObjectID and ZoneID, a registered function is handed the call it runs for, a script or function that is not there is an error, and a global nobody defines is logged once by name however many calls read it.
 */

#include "KiwadBuilder.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "LogTestDirectory.h"
#include "LuaScriptMgr.h"
#include "ReloadMgr.h"
#include "TestAppender.h"

#include <gtest/gtest.h>
#include <lua.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    class LuaScriptMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sReloadMgr.Clear();
            _store = std::make_shared<TestAppenderStore>();
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(_store));
            sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n"));
            _host.SetInstall(_directory.Path());
        }

        void TearDown() override
        {
            sLog.Reset();
            sReloadMgr.Clear();
        }

        void Write(std::vector<std::pair<std::string, std::string>> const& scripts)
        {
            std::filesystem::path const folder = _directory.Path() / "Data" / "GameData";
            std::filesystem::create_directories(folder);
            KiwadBuilder builder(2);
            for (auto const& [name, source] : scripts)
                builder.Add(name, std::string_view(source), true);
            std::vector<uint8> const bytes = builder.Build();
            std::ofstream stream(folder / "Root.wad", std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }

        void LoadOrFail()
        {
            std::vector<std::string> errors;
            ASSERT_TRUE(_host.Load(errors)) << (errors.empty() ? std::string() : errors.front());
        }

        std::size_t Logged(std::string_view text) const
        {
            auto const messages = _store->Messages("Capture");
            return static_cast<std::size_t>(std::count_if(messages.begin(), messages.end(), [text](LogMessage const& message)
            {
                return message.Text.find(text) != std::string::npos;
            }));
        }

        LogTestDirectory _directory;
        LuaScriptMgr _host;
        std::shared_ptr<TestAppenderStore> _store;
    };

    LuaScriptCall CallOf(std::string script, std::string function)
    {
        LuaScriptCall call;
        call.Script = std::move(script);
        call.Function = std::move(function);
        call.CharacterId = 41;
        call.GameObjectId = 0x1000000000ull;
        call.ZoneId = 7;
        return call;
    }
}

TEST_F(LuaScriptMgrTest, ItReadsTheInstallsScriptsAndTheReloadTargetSwapsInAnEditedRootWad)
{
    Write({ { "Scripts/One.lua", "x = 1" } });
    LoadOrFail();
    EXPECT_EQ(_host.GetLibrary()->Size(), 1u);
    EXPECT_EQ(Logged("Read 1 Lua scripts from Root.wad's Scripts/ folder"), 1u);

    _host.RegisterReloadTargets();
    Write({ { "Scripts/One.lua", "x = 1" }, { "Scripts/Two.lua", "x = 2" } });
    ReloadOutcome const outcome = sReloadMgr.Reload(std::string(LuaScriptMgr::Target));
    EXPECT_TRUE(outcome.Ok);
    EXPECT_EQ(_host.GetLibrary()->Size(), 2u);

    std::filesystem::remove(_directory.Path() / "Data" / "GameData" / "Root.wad");
    EXPECT_FALSE(sReloadMgr.Reload(std::string(LuaScriptMgr::Target)).Ok);
    EXPECT_EQ(_host.GetLibrary()->Size(), 2u) << "a reload that fails keeps the library serving";
}

TEST_F(LuaScriptMgrTest, ACallRunsTheFunctionWithTheArgumentsAndRegisteredFunctionsGetTheCall)
{
    Write({ { "Scripts/WorldBuilding/Open.lua", "Include('Scripts/WorldBuilding/Common.lua') function OpenGate(args) Report(args.CharacterID, args.GameObjectID, args.ZoneID, Prefix) end" },
        { "Scripts/WorldBuilding/Common.lua", "Prefix = 'gate'" } });
    LoadOrFail();
    std::vector<std::string> seen;
    _host.RegisterFunction("Report", [&seen](lua_State* lua, LuaScriptCall const& call) -> int
    {
        seen.push_back(fmt::format("{} {} {} {} {} {}", lua_tonumber(lua, 1), lua_tonumber(lua, 2), lua_tonumber(lua, 3), lua_tostring(lua, 4), call.Function, call.CharacterId));
        return 0;
    });
    std::string error;
    EXPECT_TRUE(_host.Call(CallOf("Scripts/WorldBuilding/Open.lua", "OpenGate"), error)) << error;
    EXPECT_TRUE(_host.Call(CallOf("worldbuilding/open.lua", "OpenGate"), error)) << "the script is found as the client's archives find it: " << error;
    EXPECT_EQ(seen, (std::vector<std::string>(2, "41 68719476736 7 gate OpenGate 41")));
}

TEST_F(LuaScriptMgrTest, AScriptOrFunctionThatIsNotThereIsAnError)
{
    Write({ { "Scripts/Open.lua", "function OpenGate(args) end Value = 3" } });
    LoadOrFail();
    std::string error;
    EXPECT_FALSE(_host.Call(CallOf("Scripts/Gone.lua", "OpenGate"), error));
    EXPECT_EQ(error, "Scripts/Gone.lua is not in the install's scripts");
    EXPECT_FALSE(_host.Call(CallOf("Open.lua", "CloseGate"), error));
    EXPECT_EQ(error, "Scripts/Open.lua has no function CloseGate");
    EXPECT_FALSE(_host.Call(CallOf("Open.lua", "Value"), error));
    EXPECT_EQ(error, "Scripts/Open.lua has no function Value");
}

TEST_F(LuaScriptMgrTest, AGlobalNobodyDefinesIsLoggedOnceByName)
{
    Write({ { "Scripts/Minigame.lua", "function Start(args) local id = OwnerGID if Probe then Probe() end end" } });
    LoadOrFail();
    std::string error;
    EXPECT_TRUE(_host.Call(CallOf("Minigame.lua", "Start"), error)) << error;
    EXPECT_TRUE(_host.Call(CallOf("Minigame.lua", "Start"), error)) << error;
    EXPECT_EQ(Logged("Scripts/Minigame.lua reads the global OwnerGID"), 1u);
    EXPECT_EQ(Logged("reads the global Probe"), 1u);
}

TEST_F(LuaScriptMgrTest, AnErrorTheScriptRaisesComesBackWithItsTraceback)
{
    Write({ { "Scripts/Fail.lua", "function Run(args) error('stopped here') end" } });
    LoadOrFail();
    std::string error;
    EXPECT_FALSE(_host.Call(CallOf("Fail.lua", "Run"), error));
    EXPECT_NE(error.find("Scripts/Fail.lua:1: stopped here"), std::string::npos) << error;
    EXPECT_NE(error.find("stack traceback"), std::string::npos) << error;
}
