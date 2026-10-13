/*
 * Project Ambrose by Imjustchico
 * Reads the requirement engine's quest, goal and registry answers out of a quest log, where a goal the log does not hold is absent rather than unknown, since the log is the whole of what the wizard holds.
 */

#include "QuestLogContext.h"

std::optional<bool> QuestLogContext::HasQuest(std::string_view questName) const
{
    return _log.HasQuest(questName);
}

std::optional<RequirementGoalStatus> QuestLogContext::GetGoalStatus(std::string_view questName, std::string_view goalName) const
{
    if (_log.IsGoalCompleted(questName, goalName))
        return RequirementGoalStatus::Complete;
    if (_log.IsGoalActive(questName, goalName))
        return RequirementGoalStatus::Incomplete;
    return RequirementGoalStatus::Absent;
}

std::optional<bool> QuestLogContext::HasRegistryEntry(std::string_view questName, std::string_view entryName, bool isQuestRegistry) const
{
    std::optional<double> const value = GetRegistryValue(questName, entryName, isQuestRegistry);
    return value && *value != 0.0;
}

std::optional<double> QuestLogContext::GetRegistryValue(std::string_view questName, std::string_view entryName, bool isQuestRegistry) const
{
    return isQuestRegistry ? _log.GetQuestRegistry(questName, entryName) : _log.GetRegistry(entryName);
}

std::optional<double> QuestLogContext::GetGlobalRegistryValue(std::string_view entryName) const
{
    return _log.GetRegistry(entryName);
}
