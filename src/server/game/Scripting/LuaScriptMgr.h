/*
 * Project Ambrose by Imjustchico
 * The server's host for the install's own Lua scripts (sLuaScriptMgr): the library of Root.wad's Scripts folder, swapped in whole through the reload target scripts so an edited install is picked up without a restart, the functions each owning system registers for scripts to call, and the synchronous call a ResSyncScript result makes, which runs one function of one script in a fresh sandboxed state with its includes and the arguments table the install's scripts read, CharacterID, GameObjectID and ZoneID, and logs once by name each global a script reads that neither it nor any system defines, which is how a function the server does not bind yet shows.
 */

#ifndef AMBROSE_LUASCRIPTMGR_H
#define AMBROSE_LUASCRIPTMGR_H

#include "LuaScriptLibrary.h"
#include "ReloadableStore.h"
#include "Types.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class GameSession;
struct lua_State;

struct LuaScriptCall
{
    std::string Script;
    std::string Function;
    GameSession* Session = nullptr;
    uint64 CharacterId = 0;
    uint64 GameObjectId = 0;
    uint32 ZoneId = 0;
};

using LuaHostFunction = std::function<int(lua_State*, LuaScriptCall const&)>;

class LuaScriptMgr
{
public:
    static constexpr std::string_view Target = "scripts";
    static constexpr std::string_view RootArchive = "Root.wad";

    static LuaScriptMgr& Instance();

    LuaScriptMgr() = default;
    LuaScriptMgr(LuaScriptMgr const&) = delete;
    LuaScriptMgr& operator=(LuaScriptMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<LuaScriptLibrary const> GetLibrary() const { return _library.Get(); }

    void RegisterFunction(std::string name, LuaHostFunction function);
    bool Call(LuaScriptCall const& call, std::string& error) const;

private:
    static int Dispatch(lua_State* state);
    static int Unbound(lua_State* state);
    bool FirstUnbound(std::string_view name) const;

    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<LuaScriptLibrary> _library;
    mutable std::mutex _functionsMutex;
    std::map<std::string, LuaHostFunction, std::less<>> _functions;
    mutable std::set<std::string, std::less<>> _unbound;
};

#define sLuaScriptMgr LuaScriptMgr::Instance()

#endif
