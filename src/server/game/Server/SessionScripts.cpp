/*
 * Project Ambrose by Imjustchico
 * Registers the ResSyncScript handler with the results runner and the quest functions with the Lua host, reads a result's script and function, refuses a call whose script or function is empty, and answers a script that names a wizard other than the one its call runs for with an error rather than a change.
 */

#include "SessionScripts.h"
#include "GameSession.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "PropertyObject.h"
#include "QuestLog.h"
#include "QuestMgr.h"
#include "QuestStore.h"

#include <lua.hpp>

#include <fmt/format.h>

#include <chrono>
#include <memory>

namespace
{
    constexpr char const* ScriptLog = "server.scripts";

    uint64 Now()
    {
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    std::string TextOf(PropertyObject const& result, std::string_view property)
    {
        PropertyValue const* const value = result.Get(property);
        std::string const* const text = value ? value->GetIf<std::string>() : nullptr;
        return text ? *text : std::string();
    }

    QuestLog* WizardLog(lua_State* state, LuaScriptCall const& call)
    {
        lua_Number const wizard = luaL_checknumber(state, 1);
        bool const named = wizard == static_cast<lua_Number>(call.CharacterId) || wizard == static_cast<lua_Number>(call.GameObjectId);
        QuestLog* const log = named && call.Session ? call.Session->GetQuestLog() : nullptr;
        if (!log)
            luaL_error(state, "the script names a wizard whose quest log this call does not hold");
        return log;
    }

    int ScriptCompleteQuest(lua_State* state, LuaScriptCall const& call)
    {
        QuestLog* const log = WizardLog(state, call);
        std::size_t length = 0;
        char const* const quest = luaL_checklstring(state, 2, &length);
        bool const completed = SessionScripts::CompleteQuest(*log, std::string_view(quest, length), Now());
        if (completed)
            LOG_INFO(ScriptLog, "{} completed {} for wizard {}", call.Script, std::string_view(quest, length), call.GameObjectId);
        lua_pushboolean(state, completed ? 1 : 0);
        return 1;
    }

    int CompleteQuestChain(lua_State* state, LuaScriptCall const& call)
    {
        QuestLog* const log = WizardLog(state, call);
        std::size_t length = 0;
        char const* const chain = luaL_checklstring(state, 2, &length);
        std::shared_ptr<QuestStore const> const quests = sQuestMgr.GetQuests();
        std::size_t const completed = quests ? SessionScripts::CompleteChain(*log, *quests, std::string_view(chain, length), Now()) : 0;
        LOG_INFO(ScriptLog, "{} completed {} quest(s) of the chain {} for wizard {}", call.Script, completed, std::string_view(chain, length), call.GameObjectId);
        lua_pushnumber(state, static_cast<lua_Number>(completed));
        return 1;
    }
}

std::optional<LuaScriptCall> SessionScripts::SyncCallOf(PropertyObject const& result, ResultContext const& context, std::string& error)
{
    if (!result.IsA(SyncScriptClass))
    {
        error = fmt::format("it holds a {}", result.GetClass().Name);
        return std::nullopt;
    }
    LuaScriptCall call;
    call.Script = TextOf(result, "m_script");
    call.Function = TextOf(result, "m_function");
    if (call.Script.empty() || call.Function.empty())
    {
        error = "its m_script or m_function is empty";
        return std::nullopt;
    }
    call.Session = context.Session;
    call.CharacterId = context.Session ? context.Session->GetCharacterId() : 0;
    call.GameObjectId = context.Wizard;
    call.ZoneId = context.MapId;
    return call;
}

bool SessionScripts::CompleteQuest(QuestLog& log, std::string_view quest, uint64 now)
{
    if (log.HasCompletedQuest(quest))
        return false;
    if (!log.HasQuest(quest) && !log.Add(quest, ObjectGuid::QuestGuids(), now))
        return false;
    return log.CompleteQuest(quest);
}

std::size_t SessionScripts::CompleteChain(QuestLog& log, QuestStore const& quests, std::string_view chain, uint64 now)
{
    std::string const prefix = fmt::format("{}-", chain);
    std::size_t completed = 0;
    for (auto const& [name, quest] : quests.GetAll())
        if (name.starts_with(prefix) && CompleteQuest(log, name, now))
            ++completed;
    return completed;
}

void SessionScripts::Register()
{
    sLuaScriptMgr.RegisterFunction("ScriptCompleteQuest", ScriptCompleteQuest);
    sLuaScriptMgr.RegisterFunction("CompleteQuestChain", CompleteQuestChain);
    sResultMgr.Register(SyncScriptClass, [](PropertyObject const& result, ResultContext const& context)
    {
        std::string error;
        std::optional<LuaScriptCall> const call = SyncCallOf(result, context, error);
        if (!call)
        {
            LOG_WARN(ScriptLog, "{} holds a ResSyncScript the server cannot run: {}", context.Source, error);
            return;
        }
        if (!sLuaScriptMgr.Call(*call, error))
            LOG_WARN(ScriptLog, "{} could not run {} of {} for wizard {}: {}", context.Source, call->Function, call->Script, context.Wizard, error);
        if (context.Session)
            context.Session->SaveQuestLogIfDirty();
    });
}
