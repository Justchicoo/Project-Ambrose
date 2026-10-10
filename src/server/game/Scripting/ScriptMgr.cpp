/*
 * Project Ambrose by Imjustchico
 * A script registers itself from its own constructor, which is why the loader only has to call each AddSC function, and why the manager takes that loader as an argument rather than calling it by name: the hooks would otherwise depend on the scripts that use them, which is a circle a linker is right to refuse. The manager takes ownership there and frees them when it unloads, so a script file names its classes and nothing else has to know they exist. Every hook runs each script in the order it registered and catches what one throws, naming the script and the hook; a condition hook returns its first definite answer and leaves unknown types unanswered when no script handles them. The manager watches the network for its server scripts once the first registers, asks them in registration order, stops at the first that holds a message back, and takes a script that throws for one that let it through, reported by name, so a broken script never cuts the network.
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

ZoneScript::ZoneScript(std::string name, std::string zone) : ScriptObject(std::move(name)), _zone(std::move(zone))
{
    sScriptMgr.Register(this);
}

PlayerScript::PlayerScript(std::string name) : ScriptObject(std::move(name))
{
    sScriptMgr.Register(this);
}

ConditionScript::ConditionScript(std::string name) : ScriptObject(std::move(name))
{
    sScriptMgr.Register(this);
}

NpcScript::NpcScript(std::string name, uint32 templateId) : ScriptObject(std::move(name)), _templateId(templateId)
{
    sScriptMgr.Register(this);
}

CommandScript::CommandScript(std::string name) : ScriptObject(std::move(name))
{
    sScriptMgr.Register(this);
}

ServerScript::ServerScript(std::string name) : ScriptObject(std::move(name))
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

void ScriptMgr::Register(ZoneScript* script)
{
    _zoneScripts.push_back(script);
}

void ScriptMgr::Register(PlayerScript* script)
{
    _playerScripts.push_back(script);
}

void ScriptMgr::Register(ConditionScript* script)
{
    _conditionScripts.push_back(script);
}

void ScriptMgr::Register(NpcScript* script)
{
    _npcScripts.push_back(script);
}

void ScriptMgr::Register(CommandScript* script)
{
    _commandScripts.push_back(script);
}

void ScriptMgr::Register(ServerScript* script)
{
    _serverScripts.push_back(script);
    NetworkHooks::Add(this);
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
    NetworkHooks::Remove(this);
    for (ServerScript* script : _serverScripts)
        delete script;
    _serverScripts.clear();
    for (WorldScript* script : _worldScripts)
        delete script;
    _worldScripts.clear();
    for (ZoneScript* script : _zoneScripts)
        delete script;
    _zoneScripts.clear();
    for (PlayerScript* script : _playerScripts)
        delete script;
    _playerScripts.clear();
    for (ConditionScript* script : _conditionScripts)
        delete script;
    _conditionScripts.clear();
    for (NpcScript* script : _npcScripts)
        delete script;
    _npcScripts.clear();
    for (CommandScript* script : _commandScripts)
        delete script;
    _commandScripts.clear();
    _loaded = false;
}

std::size_t ScriptMgr::GetScriptCount() const
{
    return _worldScripts.size() + _zoneScripts.size() + _playerScripts.size() + _conditionScripts.size() + _npcScripts.size() + _commandScripts.size() + _serverScripts.size();
}

std::vector<std::string> ScriptMgr::GetScriptNames() const
{
    std::vector<std::string> names;
    names.reserve(GetScriptCount());
    for (WorldScript const* script : _worldScripts)
        names.push_back(script->GetName());
    for (ZoneScript const* script : _zoneScripts)
        names.push_back(script->GetName());
    for (PlayerScript const* script : _playerScripts)
        names.push_back(script->GetName());
    for (ConditionScript const* script : _conditionScripts)
        names.push_back(script->GetName());
    for (NpcScript const* script : _npcScripts)
        names.push_back(script->GetName());
    for (CommandScript const* script : _commandScripts)
        names.push_back(script->GetName());
    for (ServerScript const* script : _serverScripts)
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

template<typename Hook>
void ScriptMgr::ForEachPlayer(std::string_view what, Hook hook)
{
    for (PlayerScript* script : _playerScripts)
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

void ScriptMgr::OnGoldChanged(Player& player, int32 oldValue, int32 newValue)
{
    ForEachPlayer("OnGoldChanged", [&player, oldValue, newValue](PlayerScript* script) { script->OnGoldChanged(player, oldValue, newValue); });
}

void ScriptMgr::OnHealthChanged(Player& player, int32 oldValue, int32 newValue)
{
    ForEachPlayer("OnHealthChanged", [&player, oldValue, newValue](PlayerScript* script) { script->OnHealthChanged(player, oldValue, newValue); });
}

void ScriptMgr::OnEquip(Player& player, uint64 itemGuid, uint32 templateId, std::string_view slot)
{
    ForEachPlayer("OnEquip", [&player, itemGuid, templateId, slot](PlayerScript* script) { script->OnEquip(player, itemGuid, templateId, slot); });
}

void ScriptMgr::OnUnequip(Player& player, uint64 itemGuid, uint32 templateId, std::string_view slot)
{
    ForEachPlayer("OnUnequip", [&player, itemGuid, templateId, slot](PlayerScript* script) { script->OnUnequip(player, itemGuid, templateId, slot); });
}

std::optional<bool> ScriptMgr::EvaluateCondition(RequirementRow const& requirement, RequirementContext const& context) const
{
    for (ConditionScript* script : _conditionScripts)
    {
        try
        {
            std::optional<bool> const result = script->OnConditionCheck(requirement, context);
            if (result)
                return result;
        }
        catch (std::exception const& failure)
        {
            LOG_ERROR("server.scripts", "The script {} threw from OnConditionCheck: {}", script->GetName(), failure.what());
        }
        catch (...)
        {
            LOG_ERROR("server.scripts", "The script {} threw from OnConditionCheck for a reason it did not say", script->GetName());
        }
    }
    return std::nullopt;
}

std::vector<NpcScript*> ScriptMgr::GetNpcScripts(uint32 templateId) const
{
    std::vector<NpcScript*> scripts;
    for (NpcScript* script : _npcScripts)
        if (script->Serves(templateId))
            scripts.push_back(script);
    return scripts;
}

template<typename Hook>
void ScriptMgr::ForZone(std::string_view zone, std::string_view what, Hook hook)
{
    for (ZoneScript* script : _zoneScripts)
    {
        if (!script->GetZone().empty() && script->GetZone() != zone)
            continue;
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

void ScriptMgr::OnVolumeEnter(std::string_view zone, uint32 mapId, std::string_view volume, uint64 wizard)
{
    ForZone(zone, "OnVolumeEnter", [&](ZoneScript* script) { script->OnVolumeEnter(mapId, volume, wizard); });
}

void ScriptMgr::OnVolumeExit(std::string_view zone, uint32 mapId, std::string_view volume, uint64 wizard)
{
    ForZone(zone, "OnVolumeExit", [&](ZoneScript* script) { script->OnVolumeExit(mapId, volume, wizard); });
}

void ScriptMgr::OnTriggerFired(std::string_view zone, uint32 mapId, std::string_view trigger, uint64 wizard)
{
    ForZone(zone, "OnTriggerFired", [&](ZoneScript* script) { script->OnTriggerFired(mapId, trigger, wizard); });
}

template<typename Hook>
bool ScriptMgr::AllServer(std::string_view what, Hook hook)
{
    for (ServerScript* script : _serverScripts)
    {
        try
        {
            if (!hook(script))
                return false;
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
    return true;
}

void ScriptMgr::OnNetworkStart(std::string_view app)
{
    AllServer("OnNetworkStart", [app](ServerScript* script) { script->OnNetworkStart(app); return true; });
}

void ScriptMgr::OnSocketOpen(uint16 sessionId, std::string_view address)
{
    AllServer("OnSocketOpen", [sessionId, address](ServerScript* script) { script->OnSocketOpen(sessionId, address); return true; });
}

void ScriptMgr::OnSocketClose(uint16 sessionId)
{
    AllServer("OnSocketClose", [sessionId](ServerScript* script) { script->OnSocketClose(sessionId); return true; });
}

bool ScriptMgr::CanPacketReceive(uint16 sessionId, uint8 serviceId, uint8 order)
{
    return AllServer("CanPacketReceive", [=](ServerScript* script) { return script->CanPacketReceive(sessionId, serviceId, order); });
}

bool ScriptMgr::CanPacketSend(uint16 sessionId, uint8 serviceId, uint8 order)
{
    return AllServer("CanPacketSend", [=](ServerScript* script) { return script->CanPacketSend(sessionId, serviceId, order); });
}
