/*
 * Project Ambrose by Imjustchico
 * Reads the library from the install's Root.wad and logs how many scripts it holds, registers the reload target that swaps it, and runs a synchronous call: each registered function becomes a closure over the function and the call, a read of a global nobody defined is logged the first time its name is seen, the script and its includes run once, the named function is called with the arguments table, and what the script logs is written under server.scripts with its name.
 */

#include "LuaScriptMgr.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ReloadMgr.h"

#include <lua.hpp>

#include <fmt/format.h>

#include <chrono>
#include <utility>

namespace
{
    constexpr char const* ScriptLog = "server.scripts";

    void SetNumber(lua_State* state, char const* field, double value)
    {
        lua_pushnumber(state, value);
        lua_setfield(state, -2, field);
    }
}

LuaScriptMgr& LuaScriptMgr::Instance()
{
    static LuaScriptMgr instance;
    return instance;
}

void LuaScriptMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void LuaScriptMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); });
}

bool LuaScriptMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path install;
    {
        std::lock_guard const lock(_installMutex);
        install = _install;
    }
    if (install.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no Lua script can be read");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::filesystem::path const rootWad = install / "Data" / "GameData" / std::filesystem::path(RootArchive);
    std::string error;
    std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(rootWad, error);
    if (!root)
    {
        errors.push_back(fmt::format("{} cannot be opened: {}", ConfigMgr::PathToUtf8(rootWad), error));
        return false;
    }
    std::shared_ptr<LuaScriptLibrary const> library = LuaScriptLibrary::Read(*root, errors);
    if (!library)
        return false;
    std::size_t const count = library->Size();
    _library.Replace(std::move(library));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO("server.loading", "Read {} Lua scripts from {}'s {} folder in {} ms", count, RootArchive, LuaState::ScriptsFolder, took.count());
    return true;
}

void LuaScriptMgr::RegisterFunction(std::string name, LuaHostFunction function)
{
    std::lock_guard const lock(_functionsMutex);
    _functions.insert_or_assign(std::move(name), std::move(function));
}

int LuaScriptMgr::Dispatch(lua_State* state)
{
    auto const* const function = static_cast<LuaHostFunction const*>(lua_touserdata(state, lua_upvalueindex(1)));
    auto const* const call = static_cast<LuaScriptCall const*>(lua_touserdata(state, lua_upvalueindex(2)));
    return (*function)(state, *call);
}

bool LuaScriptMgr::FirstUnbound(std::string_view name) const
{
    std::lock_guard const lock(_functionsMutex);
    return _unbound.emplace(name).second;
}

int LuaScriptMgr::Unbound(lua_State* state)
{
    auto const* const manager = static_cast<LuaScriptMgr const*>(lua_touserdata(state, lua_upvalueindex(1)));
    auto const* const call = static_cast<LuaScriptCall const*>(lua_touserdata(state, lua_upvalueindex(2)));
    std::size_t length = 0;
    char const* const name = lua_type(state, 2) == LUA_TSTRING ? lua_tolstring(state, 2, &length) : nullptr;
    if (name && manager->FirstUnbound(std::string_view(name, length)))
        LOG_WARN(ScriptLog, "{}{} reads the global {}, which neither the install's scripts nor the server define, so the server does not bind it yet",
            LuaState::ScriptsFolder, LuaState::NameOf(call->Script), std::string_view(name, length));
    return 0;
}

bool LuaScriptMgr::Call(LuaScriptCall const& call, std::string& error) const
{
    std::shared_ptr<LuaScriptLibrary const> const library = GetLibrary();
    std::string const script = LuaState::NameOf(call.Script);
    if (!library->Find(script))
    {
        error = fmt::format("{}{} is not in the install's scripts", LuaState::ScriptsFolder, script);
        return false;
    }
    std::map<std::string, LuaHostFunction, std::less<>> functions;
    {
        std::lock_guard const lock(_functionsMutex);
        functions = _functions;
    }
    std::unique_ptr<LuaState> const state = LuaState::Create(LuaScriptLibrary::ReaderOf(library),
        [script](std::string_view line) { LOG_INFO(ScriptLog, "{}{}: {}", LuaState::ScriptsFolder, script, line); }, LuaLimits{}, error);
    if (!state)
        return false;
    lua_State* const lua = state->Get();
    for (auto const& [name, function] : functions)
    {
        lua_pushlightuserdata(lua, const_cast<LuaHostFunction*>(&function));
        lua_pushlightuserdata(lua, const_cast<LuaScriptCall*>(&call));
        lua_pushcclosure(lua, &LuaScriptMgr::Dispatch, 2);
        lua_setglobal(lua, name.c_str());
    }
    lua_pushlightuserdata(lua, const_cast<LuaScriptMgr*>(this));
    lua_pushlightuserdata(lua, const_cast<LuaScriptCall*>(&call));
    lua_pushcclosure(lua, &LuaScriptMgr::Unbound, 2);
    state->SetGlobalFallback();
    if (!state->Include(script, error))
        return false;
    lua_getglobal(lua, call.Function.c_str());
    if (!lua_isfunction(lua, -1))
    {
        lua_pop(lua, 1);
        error = fmt::format("{}{} has no function {}", LuaState::ScriptsFolder, script, call.Function);
        return false;
    }
    lua_newtable(lua);
    SetNumber(lua, "CharacterID", static_cast<double>(call.CharacterId));
    SetNumber(lua, "GameObjectID", static_cast<double>(call.GameObjectId));
    SetNumber(lua, "ZoneID", static_cast<double>(call.ZoneId));
    return state->Call(1, error);
}
