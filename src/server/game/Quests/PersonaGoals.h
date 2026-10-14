/*
 * Project Ambrose by Imjustchico
 * Talk-to goals: the persona goals a wizard holds active whose persona is the NPC's template, matched by the template's object name and never its display name, and the check MSG_COMPLETEPERSONA must pass, a goal the wizard holds by its quest's and its own GIDs, still active, a persona goal, naming this NPC, with the wizard in its range.
 */

#ifndef AMBROSE_PERSONAGOALS_H
#define AMBROSE_PERSONAGOALS_H

#include "QuestLog.h"
#include "QuestStore.h"

#include <string>
#include <string_view>
#include <vector>

namespace PersonaGoals
{
    inline constexpr std::string_view ServiceName = "QuestPersonaGoalService";
    inline constexpr std::string_view CompleteIcon = "Complete";
    inline constexpr std::string_view CompletionType = "Completion";
    inline constexpr int32 ReinteractAfterQuest = 2;

    struct Active
    {
        QuestInfo const* Quest = nullptr;
        QuestGoal const* Goal = nullptr;
        QuestLogQuest const* Held = nullptr;
        QuestLogGoal const* HeldGoal = nullptr;
    };

    enum class Verdict : uint8
    {
        Accepted,
        NotHeld,
        NotActive,
        NotPersona,
        WrongNpc,
        OutOfRange
    };

    std::vector<Active> For(QuestStore const& quests, QuestLog const& log, uint32 templateId);
    Verdict Check(QuestStore const& quests, QuestLog const& log, uint64 questGid, uint64 goalGid, uint32 templateId, bool inRange, Active& found);
    std::string_view GetVerdictName(Verdict verdict) noexcept;
}

#endif
