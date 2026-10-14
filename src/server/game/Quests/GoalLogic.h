/*
 * Project Ambrose by Imjustchico
 * What completing a goal does to its quest: each of the goal's completion logic entries whose AND goals are all complete and enough of whose OR goals are fires, in order, either completing the quest, which wins over any goal another entry adds, or adding goals the wizard has never held and may start; with no entry adding a goal and none left active, the quest completes. Applied to a wizard's quest log, as are the start goals of a quest it takes.
 */

#ifndef AMBROSE_GOALLOGIC_H
#define AMBROSE_GOALLOGIC_H

#include "GuidGenerator.h"
#include "QuestLog.h"
#include "QuestStore.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct GoalLogicOutcome
{
    std::vector<std::string> GoalsToStart;
    bool CompleteQuest = false;
};

namespace GoalLogic
{
    using CanStart = std::function<bool(QuestGoal const&)>;

    GoalLogicOutcome Evaluate(QuestInfo const& quest, QuestLogQuest const& held, std::string_view completed, CanStart const& canStart);
    GoalLogicOutcome Advance(QuestLog& log, QuestInfo const& quest, std::string_view completed, GuidGenerator& gids, CanStart const& canStart);
    bool Start(QuestLog& log, QuestInfo const& quest, GuidGenerator& gids, uint64 now, CanStart const& canStart);
}

#endif
