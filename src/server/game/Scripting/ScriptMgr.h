/*
 * Project Ambrose by Imjustchico
 * The hooks every content script hangs off: a script names itself and registers as it is constructed, the manager keeps each kind in its own list and calls them in registration order, and a hook that throws is reported with the script's name and does not stop the others; WorldScript carries the server's startup, shutdown, configuration reload and update tick, ZoneScript hears a wizard enter or leave a volume and a trigger fire in its own zone, or in every zone when it names none, PlayerScript hears a wizard's live gold and health changes, and ConditionScript may evaluate a custom requirement type. It knows nothing of the scripts themselves: the caller hands it the loader CMake wrote, so the hooks do not depend on the content that uses them. ServerScript sees the network: when it starts, each socket as it opens and closes, and each DML message a session receives or sends, which any server script may hold back, so a module can stop a message without the core being edited.
 */

#ifndef AMBROSE_SCRIPTMGR_H
#define AMBROSE_SCRIPTMGR_H

#include "ChatCommand.h"
#include "NetworkHooks.h"
#include "Types.h"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Player;
struct RequirementRow;
class RequirementContext;

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

class ZoneScript : public ScriptObject
{
public:
    std::string const& GetZone() const { return _zone; }

    virtual void OnVolumeEnter(uint32 mapId, std::string_view volume, uint64 wizard) { (void)mapId; (void)volume; (void)wizard; }
    virtual void OnVolumeExit(uint32 mapId, std::string_view volume, uint64 wizard) { (void)mapId; (void)volume; (void)wizard; }
    virtual void OnTriggerFired(uint32 mapId, std::string_view trigger, uint64 wizard) { (void)mapId; (void)trigger; (void)wizard; }

protected:
    ZoneScript(std::string name, std::string zone);

private:
    std::string _zone;
};

class PlayerScript : public ScriptObject
{
public:
    virtual void OnGoldChanged(Player& player, int32 oldValue, int32 newValue) { (void)player; (void)oldValue; (void)newValue; }
    virtual void OnHealthChanged(Player& player, int32 oldValue, int32 newValue) { (void)player; (void)oldValue; (void)newValue; }

protected:
    explicit PlayerScript(std::string name);
};

class ConditionScript : public ScriptObject
{
public:
    virtual std::optional<bool> OnConditionCheck(RequirementRow const& requirement, RequirementContext const& context) const
    {
        (void)requirement;
        (void)context;
        return std::nullopt;
    }

protected:
    explicit ConditionScript(std::string name);
};

class CommandScript : public ScriptObject
{
public:
    virtual std::vector<ChatCommand> GetCommands() const = 0;

protected:
    explicit CommandScript(std::string name);
};

class ServerScript : public ScriptObject
{
public:
    virtual void OnNetworkStart(std::string_view app) { (void)app; }
    virtual void OnSocketOpen(uint16 sessionId, std::string_view address) { (void)sessionId; (void)address; }
    virtual void OnSocketClose(uint16 sessionId) { (void)sessionId; }
    virtual bool CanPacketReceive(uint16 sessionId, uint8 serviceId, uint8 order) { (void)sessionId; (void)serviceId; (void)order; return true; }
    virtual bool CanPacketSend(uint16 sessionId, uint8 serviceId, uint8 order) { (void)sessionId; (void)serviceId; (void)order; return true; }

protected:
    explicit ServerScript(std::string name);
};

class ScriptMgr : private NetworkObserver
{
public:
    static ScriptMgr& Instance();

    ScriptMgr(ScriptMgr const&) = delete;
    ScriptMgr& operator=(ScriptMgr const&) = delete;

    using ScriptLoader = void (*)();

    void Register(WorldScript* script);
    void Register(ZoneScript* script);
    void Register(PlayerScript* script);
    void Register(ConditionScript* script);
    void Register(CommandScript* script);
    void Register(ServerScript* script);
    void LoadScripts(ScriptLoader loader);
    void Unload();

    std::size_t GetScriptCount() const;
    std::vector<std::string> GetScriptNames() const;

    void OnStartup();
    void OnShutdown();
    void OnConfigLoad(bool reload);
    void OnWorldUpdate(std::chrono::milliseconds diff);
    void OnVolumeEnter(std::string_view zone, uint32 mapId, std::string_view volume, uint64 wizard);
    void OnVolumeExit(std::string_view zone, uint32 mapId, std::string_view volume, uint64 wizard);
    void OnTriggerFired(std::string_view zone, uint32 mapId, std::string_view trigger, uint64 wizard);
    void OnGoldChanged(Player& player, int32 oldValue, int32 newValue);
    void OnHealthChanged(Player& player, int32 oldValue, int32 newValue);
    std::optional<bool> EvaluateCondition(RequirementRow const& requirement, RequirementContext const& context) const;

    std::vector<ChatCommand> GetCommands() const;

private:
    ScriptMgr() = default;
    ~ScriptMgr();

    template<typename Hook>
    void ForEach(std::string_view what, Hook hook);
    template<typename Hook>
    void ForZone(std::string_view zone, std::string_view what, Hook hook);
    template<typename Hook>
    void ForEachPlayer(std::string_view what, Hook hook);
    template<typename Hook>
    bool AllServer(std::string_view what, Hook hook);

    void OnNetworkStart(std::string_view app) override;
    void OnSocketOpen(uint16 sessionId, std::string_view address) override;
    void OnSocketClose(uint16 sessionId) override;
    bool CanPacketReceive(uint16 sessionId, uint8 serviceId, uint8 order) override;
    bool CanPacketSend(uint16 sessionId, uint8 serviceId, uint8 order) override;

    std::vector<ChatCommand> CollectCommands() const;

    bool _loaded = false;
    std::vector<WorldScript*> _worldScripts;
    std::vector<ZoneScript*> _zoneScripts;
    std::vector<PlayerScript*> _playerScripts;
    std::vector<ConditionScript*> _conditionScripts;
    std::vector<CommandScript*> _commandScripts;
    std::vector<ServerScript*> _serverScripts;
};

#define sScriptMgr ScriptMgr::Instance()

#endif
