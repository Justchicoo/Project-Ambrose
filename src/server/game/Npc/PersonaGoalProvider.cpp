/*
 * Project Ambrose by Imjustchico
 * Lists the talk-to goals an NPC answers as goal options and passes the picked one on.
 */

#include "PersonaGoalProvider.h"
#include "QuestBook.h"

#include <utility>

PersonaGoalProvider::PersonaGoalProvider(std::shared_ptr<QuestStore const> quests, QuestLog const& log, CompleteSink complete)
    : _quests(std::move(quests)), _log(log), _complete(std::move(complete))
{
}

bool PersonaGoalProvider::Serves(QuestStore const& quests, uint32 templateId)
{
    return !quests.GetPersonaGoals(templateId).empty();
}

std::vector<NpcServiceOption> PersonaGoalProvider::GetServiceOptions(NpcServiceTarget const& target) const
{
    std::vector<NpcServiceOption> options;
    if (!_quests)
        return options;
    for (PersonaGoals::Active const& active : PersonaGoals::For(*_quests, _log, target.TemplateId))
    {
        QuestGoalRow const& row = active.Goal->Row;
        QuestWireEncoder::GoalEntryFull goal;
        goal.QuestId = active.Held->Gid;
        goal.GoalId = active.HeldGoal->Gid;
        goal.GoalTitle = row.TitleKey;
        goal.QuestTitle = active.Quest->Row.TitleKey;
        goal.GoalNameId = row.NameId;
        goal.PersonaName = row.PersonaName;
        goal.Type = QuestBook::WireType(active.Goal->Type);
        goal.Location = row.LocationKey;
        goal.DestinationZone = row.DestinationZone;
        goal.Image1 = row.Image1;
        goal.Image2 = row.Image2;
        goal.ClientTags = active.Goal->ClientTags;
        options.push_back({ std::string(PersonaGoals::ServiceName), std::string(PersonaGoals::CompleteIcon), active.Quest->Row.TitleKey, std::nullopt, std::move(goal), std::nullopt });
    }
    return options;
}

void PersonaGoalProvider::OnServiceInteraction(NpcServiceTarget const& target, uint32 index)
{
    if (!_quests)
        return;
    std::vector<PersonaGoals::Active> const goals = PersonaGoals::For(*_quests, _log, target.TemplateId);
    if (index < goals.size() && _complete)
        _complete(target, goals[index]);
}
