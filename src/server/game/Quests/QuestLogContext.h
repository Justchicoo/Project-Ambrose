/*
 * Project Ambrose by Imjustchico
 * Answers the requirement engine's quest and registry questions from a wizard's quest log: whether it holds a quest, a goal's status, whether a per-quest or global registry entry is set and its value. Everything else it does not know, so those requirements fail closed.
 */

#ifndef AMBROSE_QUESTLOGCONTEXT_H
#define AMBROSE_QUESTLOGCONTEXT_H

#include "QuestLog.h"
#include "RequirementMgr.h"

class QuestLogContext : public RequirementContext
{
public:
    explicit QuestLogContext(QuestLog const& log) : _log(log) { }

    std::optional<bool> HasQuest(std::string_view questName) const override;
    std::optional<RequirementGoalStatus> GetGoalStatus(std::string_view questName, std::string_view goalName) const override;
    std::optional<bool> HasRegistryEntry(std::string_view questName, std::string_view entryName, bool isQuestRegistry) const override;
    std::optional<double> GetRegistryValue(std::string_view questName, std::string_view entryName, bool isQuestRegistry) const override;
    std::optional<double> GetGlobalRegistryValue(std::string_view entryName) const override;

private:
    QuestLog const& _log;
};

#endif
