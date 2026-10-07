/*
 * Project Ambrose by Imjustchico
 * The hooks every content script hangs off: a script names itself and registers as it is constructed, the manager keeps each kind in its own list and calls them in registration order, and a hook that throws is reported with the script's name and does not stop the others; WorldScript and CommandScript are the first kinds, carrying the server's startup, shutdown, configuration reload and update tick, PlayerScript hears a wizard's live gold and health changes, and later milestones add the npc, quest and zone kinds beside them. It knows nothing of the scripts themselves: the caller hands it the loader CMake wrote, so the hooks do not depend on the content that uses them. ServerScript sees the network: when it starts, each socket as it opens and closes, and each DML message a session receives or sends, which any server script may hold back, so a module can stop a message without the core being edited.
 */

#ifndef AMBROSE_SCRIPTMGR_H
#define AMBROSE_SCRIPTMGR_H

#include "ChatCommand.h"
#include "NetworkHooks.h"
#include "Types.h"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

class Player;

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

class PlayerScript : public ScriptObject
{
public:
    virtual void OnGoldChanged(Player& player, int32 oldValue, int32 newValue) { (void)player; (void)oldValue; (void)newValue; }
    virtual void OnHealthChanged(Player& player, int32 oldValue, int32 newValue) { (void)player; (void)oldValue; (void)newValue; }

protected:
    explicit PlayerScript(std::string name);
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
    void Register(PlayerScript* script);
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
    void OnGoldChanged(Player& player, int32 oldValue, int32 newValue);
    void OnHealthChanged(Player& player, int32 oldValue, int32 newValue);

    std::vector<ChatCommand> GetCommands() const;

private:
    ScriptMgr() = default;
    ~ScriptMgr();

    template<typename Hook>
    void ForEach(std::string_view what, Hook hook);
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
    std::vector<PlayerScript*> _playerScripts;
    std::vector<CommandScript*> _commandScripts;
    std::vector<ServerScript*> _serverScripts;
};

#define sScriptMgr ScriptMgr::Instance()

#endif
