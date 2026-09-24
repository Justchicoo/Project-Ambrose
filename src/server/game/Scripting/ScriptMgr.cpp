/*
 * Project Ambrose by Imjustchico
 * A script registers itself from its own constructor, which is why the loader only has to call each AddSC function, and why the manager takes that loader as an argument rather than calling it by name: the hooks would otherwise depend on the scripts that use them, which is a circle a linker is right to refuse. the manager takes ownership there and frees them when it unloads, so a script file names its classes and nothing else has to know they exist. Every hook runs each script in the order it registered and catches what one throws, naming the script and the hook, because one bad content script must not take the tick down with it.
 */

#include "ScriptMgr.h"
#include "Log.h"

#include <exception>
#include <iterator>
#include <utility>

WorldScript::WorldScript(std::string name) : ScriptObject(std::move(name))
{
    sScriptMgr.Register(this);
}

CommandScript::CommandScript(std::string name) : ScriptObject(std::move(name))
{
    sScriptMgr.Register(this);
}

ScriptMgr& ScriptMgr::Instance()
{
    static ScriptMgr instance;
    return instance;
}

ScriptMgr::~ScriptMgr()
{
    Unload();
}

void ScriptMgr::Register(WorldScript* script)
{
    _worldScripts.push_back(script);
}

void ScriptMgr::Register(CommandScript* script)
{
    _commandScripts.push_back(script);
}

std::vector<ChatCommand> ScriptMgr::CollectCommands() const
{
    std::vector<ChatCommand> commands;
    for (CommandScript const* script : _commandScripts)
    {
        std::vector<ChatCommand> offered = script->GetCommands();
        commands.insert(commands.end(), std::make_move_iterator(offered.begin()), std::make_move_iterator(offered.end()));
    }
    return commands;
}

std::vector<ChatCommand> ScriptMgr::GetCommands() const
{
    return CollectCommands();
}

void ScriptMgr::LoadScripts(ScriptLoader loader)
{
    if (_loaded)
        return;
    _loaded = true;
    if (loader)
        loader();
    LOG_INFO("server.scripts", "Loaded {} script(s)", GetScriptCount());
}

void ScriptMgr::Unload()
{
    for (WorldScript* script : _worldScripts)
        delete script;
    _worldScripts.clear();
    for (CommandScript* script : _commandScripts)
        delete script;
    _commandScripts.clear();
    _loaded = false;
}

std::size_t ScriptMgr::GetScriptCount() const
{
    return _worldScripts.size() + _commandScripts.size();
}

std::vector<std::string> ScriptMgr::GetScriptNames() const
{
    std::vector<std::string> names;
    names.reserve(GetScriptCount());
    for (WorldScript const* script : _worldScripts)
        names.push_back(script->GetName());
    for (CommandScript const* script : _commandScripts)
        names.push_back(script->GetName());
    return names;
}

template<typename Hook>
void ScriptMgr::ForEach(std::string_view what, Hook hook)
{
    for (WorldScript* script : _worldScripts)
    {
        try
        {
            hook(script);
        }
        catch (std::exception const& failure)
        {
            LOG_ERROR("server.scripts", "The script {} threw from {}: {}", script->GetName(), what, failure.what());
        }
        catch (...)
        {
            LOG_ERROR("server.scripts", "The script {} threw from {} for a reason it did not say", script->GetName(), what);
        }
    }
}

void ScriptMgr::OnStartup()
{
    ForEach("OnStartup", [](WorldScript* script) { script->OnStartup(); });
}

void ScriptMgr::OnShutdown()
{
    ForEach("OnShutdown", [](WorldScript* script) { script->OnShutdown(); });
}

void ScriptMgr::OnConfigLoad(bool reload)
{
    ForEach("OnConfigLoad", [reload](WorldScript* script) { script->OnConfigLoad(reload); });
}

void ScriptMgr::OnWorldUpdate(std::chrono::milliseconds diff)
{
    ForEach("OnUpdate", [diff](WorldScript* script) { script->OnUpdate(diff); });
}
