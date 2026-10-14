/*
 * Project Ambrose by Imjustchico
 * Finds the active persona goals an NPC's template answers and checks a client's claim to have talked to one.
 */

#include "PersonaGoals.h"

#include <algorithm>

std::vector<PersonaGoals::Active> PersonaGoals::For(QuestStore const& quests, QuestLog const& log, uint32 templateId)
{
    std::vector<Active> active;
    for (QuestGoalRef const& ref : quests.GetPersonaGoals(templateId))
    {
        QuestInfo const* const quest = quests.Find(ref.Quest);
        QuestLogQuest const* const held = log.Find(ref.Quest);
        QuestLogGoal const* const goal = held ? held->FindGoal(ref.Goal) : nullptr;
        if (!quest || !goal || goal->Complete)
            continue;
        auto const found = quest->Goals.find(ref.Goal);
        if (found != quest->Goals.end())
            active.push_back({ quest, &found->second, held, goal });
    }
    return active;
}

PersonaGoals::Verdict PersonaGoals::Check(QuestStore const& quests, QuestLog const& log, uint64 questGid, uint64 goalGid, uint32 templateId, bool inRange, Active& found)
{
    auto const held = std::ranges::find(log.GetQuests(), questGid, &QuestLogQuest::Gid);
    if (held == log.GetQuests().end())
        return Verdict::NotHeld;
    auto const goal = std::ranges::find(held->Goals, goalGid, &QuestLogGoal::Gid);
    QuestInfo const* const quest = quests.Find(held->Name);
    if (goal == held->Goals.end() || !quest || !quest->Goals.contains(goal->Name))
        return Verdict::NotHeld;
    if (goal->Complete)
        return Verdict::NotActive;
    QuestGoal const& goalTemplate = quest->Goals.find(goal->Name)->second;
    if (goalTemplate.Type != QuestGoalType::Persona)
        return Verdict::NotPersona;
    std::vector<QuestGoalRef> const answered = quests.GetPersonaGoals(templateId);
    if (std::ranges::find(answered, QuestGoalRef{ held->Name, goal->Name }) == answered.end())
        return Verdict::WrongNpc;
    if (!inRange)
        return Verdict::OutOfRange;
    found = { quest, &goalTemplate, &*held, &*goal };
    return Verdict::Accepted;
}

std::string_view PersonaGoals::GetVerdictName(Verdict verdict) noexcept
{
    switch (verdict)
    {
        case Verdict::Accepted: return "accepted";
        case Verdict::NotHeld: return "for a goal the wizard does not hold";
        case Verdict::NotActive: return "for a goal already complete";
        case Verdict::NotPersona: return "for a goal that is not a persona goal";
        case Verdict::WrongNpc: return "to an NPC the goal does not name";
        case Verdict::OutOfRange: return "from out of the NPC's range";
    }
    return "unknown";
}
