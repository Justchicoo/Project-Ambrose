/*
 * Project Ambrose by Imjustchico
 * What the install's Lua scripts reach a wizard's session through: the ResSyncScript result, which calls m_function of m_script for the wizard the result runs for and saves the quest log the script changed, and the script functions bound to the quest log, ScriptCompleteQuest, which finishes one quest, giving it first when the wizard lacks it, and CompleteQuestChain, which finishes every quest the server holds whose name continues the chain's, as the install's WorldBuilding scripts call them; each takes the wizard by the CharacterID or GameObjectID the call was handed, and the completions run their own results as any other does.
 */

#ifndef AMBROSE_SESSIONSCRIPTS_H
#define AMBROSE_SESSIONSCRIPTS_H

#include "LuaScriptMgr.h"
#include "ResultMgr.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

class PropertyObject;
class QuestLog;
class QuestStore;

class SessionScripts
{
public:
    static constexpr std::string_view SyncScriptClass = "class ResSyncScript";

    static void Register();
    static std::optional<LuaScriptCall> SyncCallOf(PropertyObject const& result, ResultContext const& context, std::string& error);
    static bool CompleteQuest(QuestLog& log, std::string_view quest, uint64 now);
    static std::size_t CompleteChain(QuestLog& log, QuestStore const& quests, std::string_view chain, uint64 now);
};

#endif
