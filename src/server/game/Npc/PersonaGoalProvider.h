/*
 * Project Ambrose by Imjustchico
 * The NPC service provider of talk-to goals: an NPC whose template a persona goal names offers the wizard each such goal it holds active, as a GoalEntryFull titled by the quest's title key with the Complete icon, ahead of the quests it offers, and picking one hands that goal to the session to complete.
 */

#ifndef AMBROSE_PERSONAGOALPROVIDER_H
#define AMBROSE_PERSONAGOALPROVIDER_H

#include "NpcServiceProvider.h"
#include "PersonaGoals.h"

#include <functional>
#include <memory>

class PersonaGoalProvider : public NpcServiceProvider
{
public:
    using CompleteSink = std::function<void(NpcServiceTarget const& target, PersonaGoals::Active const& goal)>;

    static constexpr int32 Priority = 200;

    PersonaGoalProvider(std::shared_ptr<QuestStore const> quests, QuestLog const& log, CompleteSink complete);

    static bool Serves(QuestStore const& quests, uint32 templateId);

    std::string_view GetName() const override { return "quest_persona_goal"; }
    int32 GetPriority() const override { return Priority; }
    std::vector<NpcServiceOption> GetServiceOptions(NpcServiceTarget const& target) const override;
    void OnServiceInteraction(NpcServiceTarget const& target, uint32 index) override;

private:
    std::shared_ptr<QuestStore const> _quests;
    QuestLog const& _log;
    CompleteSink _complete;
};

#endif
