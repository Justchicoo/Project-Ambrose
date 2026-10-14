/*
 * Project Ambrose by Imjustchico
 * Opens the sandboxed state, counts every block its allocator hands out against the cap, stops a call that runs past its instruction cap from the count hook, turns an error into its message and traceback, refuses precompiled bytecode, and runs the census: unknown globals answer a do-nothing function so a script's top level runs, a vararg function that names arg is handed its arguments as Lua 5.1 did, then the bytecode of every chunk it loaded, its nested functions included, is read through jit.util for the names each GGET reads and each GSET writes, the instruction numbers taken from two probe chunks rather than assumed.
 */

#include "LuaState.h"

#include <lua.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <utility>

namespace
{
    constexpr char const* StateKey = "ambrose.state";
    constexpr char const* ChunksKey = "ambrose.chunks";
    constexpr char const* FallbackKey = "ambrose.fallback";
    constexpr int HookStep = 1000;

    constexpr std::string_view CensusReader = R"lua(
local util, chunks, readProbe, writeProbe = ...
local band, rshift = bit.band, bit.rshift
local function opOf(probe, name)
    local info = util.funcinfo(probe)
    for pc = 1, info.bytecodes - 1 do
        local ins = util.funcbc(probe, pc)
        if util.funck(probe, -rshift(ins, 16) - 1) == name then
            return band(ins, 255)
        end
    end
end
local get = opOf(readProbe, "ambrose_probe_read")
local set = opOf(writeProbe, "ambrose_probe_write")
local reads, writes = {}, {}
local function walk(f)
    local info = util.funcinfo(f)
    for pc = 1, info.bytecodes - 1 do
        local ins = util.funcbc(f, pc)
        local op = band(ins, 255)
        if op == get or op == set then
            local name = util.funck(f, -rshift(ins, 16) - 1)
            if type(name) == "string" then
                if op == get then reads[name] = true else writes[name] = true end
            end
        end
    end
    if info.children then
        for index = -1, -1000000, -1 do
            local constant = util.funck(f, index)
            if constant == nil then break end
            if type(constant) == "proto" then walk(constant) end
        end
    end
end
for _, chunk in ipairs(chunks) do walk(chunk) end
return reads, writes
)lua";

    int Traceback(lua_State* state)
    {
        char const* const message = lua_tostring(state, 1);
        luaL_traceback(state, state, message ? message : "an error that is not a string", 1);
        return 1;
    }

    int DoNothing(lua_State*)
    {
        return 0;
    }

    int AnswerDoNothing(lua_State* state)
    {
        lua_pushcfunction(state, DoNothing);
        return 1;
    }

    bool StartsWithFolder(std::string_view path)
    {
        if (path.size() < LuaState::ScriptsFolder.size())
            return false;
        return std::equal(LuaState::ScriptsFolder.begin(), LuaState::ScriptsFolder.end(), path.begin(),
            [](char left, char right) { return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right)); });
    }

    void OpenLibrary(lua_State* state, lua_CFunction open, char const* name)
    {
        lua_pushcfunction(state, open);
        lua_pushstring(state, name);
        lua_call(state, 1, 0);
    }

    void RemoveGlobal(lua_State* state, char const* name)
    {
        lua_pushnil(state);
        lua_setglobal(state, name);
    }

    std::set<std::string, std::less<>> KeysOf(lua_State* state, int index)
    {
        std::set<std::string, std::less<>> keys;
        index = index < 0 ? lua_gettop(state) + index + 1 : index;
        lua_pushnil(state);
        while (lua_next(state, index) != 0)
        {
            lua_pop(state, 1);
            if (lua_type(state, -1) == LUA_TSTRING)
            {
                std::size_t length = 0;
                char const* const key = lua_tolstring(state, -1, &length);
                keys.emplace(key, length);
            }
        }
        return keys;
    }
}

std::string LuaState::NameOf(std::string_view path)
{
    std::string name(path);
    std::replace(name.begin(), name.end(), '\\', '/');
    std::size_t start = name.find_first_not_of('/');
    if (start == std::string::npos)
        return std::string();
    std::string_view rest = std::string_view(name).substr(start);
    if (StartsWithFolder(rest))
        rest.remove_prefix(ScriptsFolder.size());
    return std::string(rest);
}

LuaState::LuaState(SourceReader reader, LogSink log, LuaLimits limits)
    : _reader(std::move(reader)), _log(std::move(log)), _limits(limits)
{
}

LuaState::~LuaState()
{
    if (_state)
        lua_close(_state);
}

std::unique_ptr<LuaState> LuaState::Create(SourceReader reader, LogSink log, LuaLimits limits, std::string& error)
{
    std::unique_ptr<LuaState> state(new LuaState(std::move(reader), std::move(log), limits));
    if (!state->Open(false, error))
        return nullptr;
    return state;
}

LuaState& LuaState::Of(lua_State* state)
{
    lua_getfield(state, LUA_REGISTRYINDEX, StateKey);
    LuaState* const owner = static_cast<LuaState*>(lua_touserdata(state, -1));
    lua_pop(state, 1);
    return *owner;
}

void* LuaState::Allocate(void* owner, void* block, std::size_t oldSize, std::size_t newSize)
{
    LuaState& self = *static_cast<LuaState*>(owner);
    std::size_t const before = block ? oldSize : 0;
    if (newSize == 0)
    {
        std::free(block);
        self._memoryUsed -= before;
        return nullptr;
    }
    if (newSize > before && self._memoryUsed - before + newSize > self._limits.MemoryBytes)
        return nullptr;
    void* const resized = std::realloc(block, newSize);
    if (resized)
        self._memoryUsed = self._memoryUsed - before + newSize;
    return resized;
}

void LuaState::CountHook(lua_State* state, lua_Debug*)
{
    LuaState& self = Of(state);
    self._instructions += HookStep;
    if (self._instructions > self._limits.Instructions)
        luaL_error(state, "the script ran past its limit of %d million instructions", static_cast<int>(self._limits.Instructions / 1'000'000));
}

bool LuaState::Open(bool census, std::string& error)
{
    _census = census;
    _state = lua_newstate(&LuaState::Allocate, this);
    if (!_state)
    {
        error = "LuaJIT could not open a state";
        return false;
    }
    lua_pushlightuserdata(_state, this);
    lua_setfield(_state, LUA_REGISTRYINDEX, StateKey);
    OpenLibrary(_state, luaopen_base, "");
    OpenLibrary(_state, luaopen_table, LUA_TABLIBNAME);
    OpenLibrary(_state, luaopen_string, LUA_STRLIBNAME);
    OpenLibrary(_state, luaopen_math, LUA_MATHLIBNAME);
    OpenLibrary(_state, luaopen_bit, LUA_BITLIBNAME);
    if (census)
        OpenLibrary(_state, luaopen_jit, LUA_JITLIBNAME);
    luaJIT_setmode(_state, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_OFF);
    for (char const* const name : { "dofile", "loadfile", "load", "loadstring", "print", "require", "module" })
        RemoveGlobal(_state, name);
    lua_getglobal(_state, LUA_STRLIBNAME);
    lua_pushnil(_state);
    lua_setfield(_state, -2, "dump");
    lua_pop(_state, 1);
    lua_register(_state, "Include", &LuaState::LuaInclude);
    lua_register(_state, "Log", &LuaState::LuaLog);
    lua_newtable(_state);
    lua_setfield(_state, LUA_REGISTRYINDEX, ChunksKey);
    lua_pushvalue(_state, LUA_GLOBALSINDEX);
    lua_newtable(_state);
    lua_pushcfunction(_state, &LuaState::GlobalIndex);
    lua_setfield(_state, -2, "__index");
    lua_setmetatable(_state, -2);
    lua_pop(_state, 1);
    lua_sethook(_state, &LuaState::CountHook, LUA_MASKCOUNT, HookStep);
    return true;
}

bool LuaState::Protected(int arguments, int results, std::string& error)
{
    int const base = lua_gettop(_state) - arguments;
    lua_pushcfunction(_state, Traceback);
    lua_insert(_state, base);
    int const status = lua_pcall(_state, arguments, results, base);
    lua_remove(_state, base);
    if (status == 0)
        return true;
    std::size_t length = 0;
    char const* const message = lua_tolstring(_state, -1, &length);
    if (status == LUA_ERRMEM)
        error = fmt::format("the script ran out of its {} MB of memory", _limits.MemoryBytes / (1024 * 1024));
    else
        error = message ? std::string(message, length) : std::string("an error that is not a string");
    lua_pop(_state, 1);
    return false;
}

bool LuaState::Call(int arguments, std::string& error)
{
    _instructions = 0;
    return Protected(arguments, 0, error);
}

bool LuaState::Include(std::string_view name, std::string& error)
{
    std::string const script = NameOf(name);
    if (std::find(_included.begin(), _included.end(), script) != _included.end())
        return true;
    std::optional<std::string> const source = _reader ? _reader(script) : std::nullopt;
    if (!source)
    {
        error = fmt::format("{}{} is not in the install", ScriptsFolder, script);
        return false;
    }
    if (!source->empty() && (*source)[0] == LUA_SIGNATURE[0])
    {
        error = fmt::format("{}{} is precompiled bytecode, which the host refuses", ScriptsFolder, script);
        return false;
    }
    std::string const chunkName = fmt::format("@{}{}", ScriptsFolder, script);
    if (luaL_loadbuffer(_state, source->data(), source->size(), chunkName.c_str()) != 0)
    {
        std::size_t length = 0;
        char const* const message = lua_tolstring(_state, -1, &length);
        error = message ? std::string(message, length) : fmt::format("{} does not compile", chunkName.substr(1));
        lua_pop(_state, 1);
        _compileFailed = true;
        return false;
    }
    if (_census)
    {
        lua_getfield(_state, LUA_REGISTRYINDEX, ChunksKey);
        lua_pushvalue(_state, -2);
        lua_rawseti(_state, -2, static_cast<int>(lua_objlen(_state, -2)) + 1);
        lua_pop(_state, 1);
    }
    _included.push_back(script);
    return Protected(0, 0, error);
}

int LuaState::LuaInclude(lua_State* state)
{
    std::size_t length = 0;
    char const* const name = luaL_checklstring(state, 1, &length);
    bool included = false;
    {
        std::string error;
        included = Of(state).Include(std::string_view(name, length), error);
        if (!included)
            lua_pushlstring(state, error.data(), error.size());
    }
    if (!included)
        return lua_error(state);
    return 0;
}

void LuaState::SetGlobalFallback()
{
    lua_setfield(_state, LUA_REGISTRYINDEX, FallbackKey);
}

int LuaState::GlobalIndex(lua_State* state)
{
    std::size_t length = 0;
    char const* const key = lua_type(state, 2) == LUA_TSTRING ? lua_tolstring(state, 2, &length) : nullptr;
    lua_Debug caller;
    if (key && std::string_view(key, length) == "arg" && lua_getstack(state, 1, &caller))
    {
        lua_newtable(state);
        int count = 0;
        while (lua_getlocal(state, &caller, -(count + 1)))
            lua_rawseti(state, -2, ++count);
        lua_pushinteger(state, count);
        lua_setfield(state, -2, "n");
        return 1;
    }
    lua_getfield(state, LUA_REGISTRYINDEX, FallbackKey);
    if (lua_isnil(state, -1))
        return 1;
    lua_pushvalue(state, 1);
    lua_pushvalue(state, 2);
    lua_call(state, 2, 1);
    return 1;
}

int LuaState::LuaLog(lua_State* state)
{
    LuaState& self = Of(state);
    if (!self._log)
        return 0;
    std::string line;
    int const count = lua_gettop(state);
    for (int index = 1; index <= count; ++index)
    {
        if (index > 1)
            line += '\t';
        std::size_t length = 0;
        if (lua_type(state, index) == LUA_TSTRING || lua_type(state, index) == LUA_TNUMBER)
        {
            char const* const text = lua_tolstring(state, index, &length);
            line.append(text, length);
        }
        else if (lua_type(state, index) == LUA_TBOOLEAN)
            line += lua_toboolean(state, index) ? "true" : "false";
        else
            line += luaL_typename(state, index);
    }
    self._log(line);
    return 0;
}

LuaScriptCensus LuaState::Census(SourceReader reader, std::string_view name, LuaLimits limits)
{
    LuaScriptCensus census;
    LuaState state(std::move(reader), nullptr, limits);
    if (!state.Open(true, census.Error))
        return census;
    lua_State* const lua = state._state;
    lua_pushcfunction(lua, AnswerDoNothing);
    state.SetGlobalFallback();
    census.Loaded = state.Include(name, census.Error);
    census.Files = state._included;
    census.Compiled = !state._compileFailed;
    std::string error;
    luaL_loadbuffer(lua, CensusReader.data(), CensusReader.size(), "=census");
    lua_getfield(lua, LUA_REGISTRYINDEX, "_PRELOAD");
    lua_getfield(lua, -1, "jit.util");
    lua_remove(lua, -2);
    if (!lua_isfunction(lua, -1) || lua_pcall(lua, 0, 1, 0) != 0 || !lua_istable(lua, -1))
    {
        census.Loaded = false;
        census.Error = "LuaJIT was built without jit.util, so no bytecode can be read";
        return census;
    }
    lua_getfield(lua, LUA_REGISTRYINDEX, ChunksKey);
    luaL_loadbuffer(lua, "return ambrose_probe_read", 25, "=probe");
    luaL_loadbuffer(lua, "ambrose_probe_write = 1", 23, "=probe");
    state._instructions = 0;
    if (!state.Protected(4, 2, error))
    {
        census.Loaded = false;
        census.Error = error;
        return census;
    }
    std::set<std::string, std::less<>> const reads = KeysOf(lua, -2);
    std::set<std::string, std::less<>> const writes = KeysOf(lua, -1);
    lua_pop(lua, 2);
    for (std::string const& read : reads)
    {
        if (writes.contains(read) || read == "arg")
            continue;
        lua_pushlstring(lua, read.data(), read.size());
        lua_rawget(lua, LUA_GLOBALSINDEX);
        bool const defined = !lua_isnil(lua, -1);
        lua_pop(lua, 1);
        if (!defined)
            census.HostGlobals.insert(read);
    }
    return census;
}
