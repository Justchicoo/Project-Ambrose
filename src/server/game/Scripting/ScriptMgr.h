/*
 * Project Ambrose by Imjustchico
 * The hooks every content script hangs off: a script names itself and registers as it is constructed, the manager keeps each kind in its own list and calls them in registration order, and a hook that throws is reported with the script's name and does not stop the others; WorldScript and CommandScript are the first kinds, carrying the server's startup, shutdown, configuration reload and update tick, and later milestones add the player, npc, quest, zone and command kinds beside it. It knows nothing of the scripts themselves: the caller hands it the loader CMake wrote, so the hooks do not depend on the content that uses them.
 */

#ifndef AMBROSE_SCRIPTMGR_H
#define AMBROSE_SCRIPTMGR_H

#include "ChatCommand.h"
#include "Types.h"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

class ScriptObject
{
public:
    virtual ~ScriptObject() = default;

    ScriptObject(ScriptObject const&) = delete;
    ScriptObject& operator=(ScriptObject const&) = delete;

    std::string const& GetName() const { return _name; }

protected:
    explicit ScriptObject(std::string name) : _name(std::move(name)) {}

private:
    std::string _name;
};

class WorldScript : public ScriptObject
{
public:
    virtual void OnStartup() {}
    virtual void OnShutdown() {}
    virtual void OnConfigLoad(bool reload) { (void)reload; }
    virtual void OnUpdate(std::chrono::milliseconds diff) { (void)diff; }

protected:
    explicit WorldScript(std::string name);
};

class CommandScript : public ScriptObject
{
public:
    virtual std::vector<ChatCommand> GetCommands() const = 0;

protected:
    explicit CommandScript(std::string name);
};

class ScriptMgr
{
public:
    static ScriptMgr& Instance();

    ScriptMgr(ScriptMgr const&) = delete;
    ScriptMgr& operator=(ScriptMgr const&) = delete;

    using ScriptLoader = void (*)();

    void Register(WorldScript* script);
    void Register(CommandScript* script);
    void LoadScripts(ScriptLoader loader);
    void Unload();

    std::size_t GetScriptCount() const;
    std::vector<std::string> GetScriptNames() const;

    void OnStartup();
    void OnShutdown();
    void OnConfigLoad(bool reload);
    void OnWorldUpdate(std::chrono::milliseconds diff);

    std::vector<ChatCommand> GetCommands() const;

private:
    ScriptMgr() = default;
    ~ScriptMgr();

    template<typename Hook>
    void ForEach(std::string_view what, Hook hook);

    std::vector<ChatCommand> CollectCommands() const;

    bool _loaded = false;
    std::vector<WorldScript*> _worldScripts;
    std::vector<CommandScript*> _commandScripts;
};

#define sScriptMgr ScriptMgr::Instance()

#endif
