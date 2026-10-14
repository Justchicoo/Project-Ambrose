/*
 * Project Ambrose by Imjustchico
 * One sandboxed Lua 5.1 state on LuaJIT, the dialect the client's own Lua 5.1.5 runs the install's Scripts in: the base, string, table, math and bit libraries without anything that reaches files, the operating system or bytecode, the JIT turned off so a count hook caps the instructions a call may run, a memory cap its allocator holds it to, Log routed to the caller, and Include, which runs a script of the install's Scripts folder once per state through the reader it was given, naming each chunk by its file; and the census of a script, which runs it with its includes in a state of its own and reads the bytecode of every function they hold for the globals they read and never set, which are what the host must give it.
 */

#ifndef AMBROSE_LUASTATE_H
#define AMBROSE_LUASTATE_H

#include "Types.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct lua_State;

struct LuaLimits
{
    std::size_t MemoryBytes = 32 * 1024 * 1024;
    uint64 Instructions = 50'000'000;
};

struct LuaScriptCensus
{
    bool Loaded = false;
    std::string Error;
    std::vector<std::string> Files;
    std::set<std::string, std::less<>> HostGlobals;
};

class LuaState
{
public:
    using SourceReader = std::function<std::optional<std::string>(std::string_view name)>;
    using LogSink = std::function<void(std::string_view line)>;

    static constexpr std::string_view ScriptsFolder = "Scripts/";

    static std::string NameOf(std::string_view path);
    static std::unique_ptr<LuaState> Create(SourceReader reader, LogSink log, LuaLimits limits, std::string& error);
    static LuaScriptCensus Census(SourceReader reader, std::string_view name, LuaLimits limits = {});
    static LuaState& Of(lua_State* state);

    ~LuaState();
    LuaState(LuaState const&) = delete;
    LuaState& operator=(LuaState const&) = delete;

    lua_State* Get() const noexcept { return _state; }
    bool Include(std::string_view name, std::string& error);
    bool Call(int arguments, std::string& error);
    std::vector<std::string> const& GetIncluded() const noexcept { return _included; }
    std::size_t GetMemoryUsed() const noexcept { return _memoryUsed; }

private:
    LuaState(SourceReader reader, LogSink log, LuaLimits limits);

    static void* Allocate(void* owner, void* block, std::size_t oldSize, std::size_t newSize);
    static void CountHook(lua_State* state, struct lua_Debug* debug);
    static int LuaInclude(lua_State* state);
    static int LuaLog(lua_State* state);

    bool Open(bool census, std::string& error);
    bool Protected(int arguments, int results, std::string& error);

    SourceReader _reader;
    LogSink _log;
    LuaLimits _limits;
    lua_State* _state = nullptr;
    std::size_t _memoryUsed = 0;
    uint64 _instructions = 0;
    bool _census = false;
    std::vector<std::string> _included;
};

#endif
