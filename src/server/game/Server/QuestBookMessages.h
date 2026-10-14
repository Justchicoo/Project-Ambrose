/*
 * Project Ambrose by Imjustchico
 * What the client's quest book and quest helper are told of a wizard's quests: MSG_SENDQUEST for a quest it holds, new when it was just accepted, with its title, info, level, madlibs, the rewards its results give at the live quest rates and the worlds its goals lead to, and MSG_SENDGOAL for one of its goals, started, moved on, completed or resumed at login, with its title, location, destination, persona, tally, madlibs, client tags and the patron icon of the quest's first Prep line; each blob encoded bare as the offer's are.
 */

#ifndef AMBROSE_QUESTBOOKMESSAGES_H
#define AMBROSE_QUESTBOOKMESSAGES_H

#include "GameMessages.h"
#include "QuestLog.h"
#include "QuestStore.h"
#include "TypeRegistry.h"

#include <optional>
#include <string>

namespace QuestBookMessages
{
    std::optional<GameMessages::SendQuest> Quest(TypeCatalogPtr const& catalog, QuestInfo const& quest, QuestLogQuest const& held, bool isNew, std::string& error);
    std::optional<GameMessages::SendGoal> Goal(TypeCatalogPtr const& catalog, QuestInfo const& quest, QuestLogQuest const& held, QuestLogGoal const& goal, int32 sendType, std::string& error);
}

#endif
