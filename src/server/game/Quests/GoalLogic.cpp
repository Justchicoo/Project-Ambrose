/*
 * Project Ambrose by Imjustchico
 * Evaluates a completed goal's logic entries against the goals a wizard holds and starts the goals they add or completes the quest.
 */

#include "GoalLogic.h"

#include <algorithm>

namespace
{
    bool IsComplete(QuestLogQuest const& held, std::string_view goal)
    {
        QuestLogGoal const* const found = held.FindGoal(goal);
        return found && found->Complete;
    }

    bool Fires(QuestGoalLogic const& logic, QuestLogQuest const& held)
    {
        if (!std::ranges::all_of(logic.RequiresAll, [&held](std::string const& goal) { return IsComplete(held, goal); }))
            return false;
        if (logic.RequiresAny.empty())
            return true;
        std::size_t const complete = std::ranges::count_if(logic.RequiresAny, [&held](std::string const& goal) { return IsComplete(held, goal); });
        return complete >= std::min<std::size_t>(logic.RequiredOrCount, logic.RequiresAny.size());
    }
}

GoalLogicOutcome GoalLogic::Evaluate(QuestInfo const& quest, QuestLogQuest const& held, std::string_view completed, CanStart const& canStart)
{
    GoalLogicOutcome outcome;
    auto const goal = quest.Goals.find(completed);
    if (goal != quest.Goals.end())
    {
        for (QuestGoalLogic const& logic : goal->second.Logic)
        {
            if (!Fires(logic, held))
                continue;
            if (logic.CompleteQuest)
                return GoalLogicOutcome{ {}, true };
            for (std::string const& add : logic.GoalsToAdd)
            {
                auto const next = quest.Goals.find(add);
                if (next == quest.Goals.end() || held.FindGoal(add) || std::ranges::find(outcome.GoalsToStart, add) != outcome.GoalsToStart.end() || !canStart(next->second))
                    continue;
                outcome.GoalsToStart.push_back(add);
            }
        }
    }
    outcome.CompleteQuest = outcome.GoalsToStart.empty() && std::ranges::none_of(held.Goals, [](QuestLogGoal const& entry) { return !entry.Complete; });
    return outcome;
}

GoalLogicOutcome GoalLogic::Advance(QuestLog& log, QuestInfo const& quest, std::string_view completed, GuidGenerator& gids, CanStart const& canStart)
{
    QuestLogQuest const* const held = log.Find(quest.Row.Name);
    if (!held)
        return {};
    GoalLogicOutcome const outcome = Evaluate(quest, *held, completed, canStart);
    if (outcome.CompleteQuest)
        log.CompleteQuest(quest.Row.Name);
    for (std::string const& goal : outcome.GoalsToStart)
        log.StartGoal(quest.Row.Name, goal, gids);
    return outcome;
}

bool GoalLogic::Start(QuestLog& log, QuestInfo const& quest, GuidGenerator& gids, uint64 now, CanStart const& canStart)
{
    if (!log.Add(quest.Row.Name, gids, now))
        return false;
    for (std::string const& goal : quest.StartGoals)
        if (auto const found = quest.Goals.find(goal); found != quest.Goals.end() && canStart(found->second))
            log.StartGoal(quest.Row.Name, goal, gids);
    return true;
}
