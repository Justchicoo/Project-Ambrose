/*
 * Project Ambrose by Imjustchico
 * Picks the quests an NPC can offer a wizard and the ones it gave that the wizard holds, and builds the blobs of their offer.
 */

#include "QuestOffers.h"
#include "QuestRewards.h"
#include "QuestBook.h"

#include <algorithm>
#include <tuple>
#include <utility>

bool QuestOffers::Qualifies(QuestInfo const& quest, RequirementContext const& context)
{
    if (quest.Requirements && !sRequirementMgr.EvaluateTree(*quest.Requirements, context))
        return false;
    return quest.Row.RequirementListId.empty() || sRequirementMgr.Evaluate(quest.Row.RequirementListId, context);
}

std::vector<QuestInfo const*> QuestOffers::Available(QuestStore const& quests, uint32 templateId, QuestLog const& log, RequirementContext const& context)
{
    std::vector<QuestInfo const*> available;
    for (std::string const& name : quests.GetQuestsOfferedBy(templateId))
    {
        QuestInfo const* const quest = quests.Find(name);
        if (!quest || log.HasQuest(name) || (log.HasCompletedQuest(name) && quest->Row.Repeat == 0))
            continue;
        if (!Qualifies(*quest, context))
            continue;
        available.push_back(quest);
    }
    std::ranges::sort(available, [](QuestInfo const* left, QuestInfo const* right)
    {
        return std::tie(left->Row.TitleKey, left->Row.Name) < std::tie(right->Row.TitleKey, right->Row.Name);
    });
    return available;
}

std::vector<QuestInfo const*> QuestOffers::Underway(QuestStore const& quests, uint32 templateId, QuestLog const& log)
{
    std::vector<QuestInfo const*> underway;
    for (std::string const& name : quests.GetQuestsOfferedBy(templateId))
        if (QuestInfo const* const quest = quests.Find(name); quest && log.HasQuest(name))
            underway.push_back(quest);
    std::ranges::sort(underway, [](QuestInfo const* left, QuestInfo const* right)
    {
        return std::tie(left->Row.TitleKey, left->Row.Name) < std::tie(right->Row.TitleKey, right->Row.Name);
    });
    return underway;
}

Dialogs::ActorDialog QuestOffers::BuildPrepDialog(QuestInfo const& quest, RequirementContext const& facts)
{
    return QuestBook::Dialog(quest.Dialogs, QuestStore::TagPrep, facts);
}

QuestWireEncoder::GoalCompilation QuestOffers::BuildStartGoals(QuestInfo const& quest)
{
    QuestWireEncoder::GoalCompilation compilation;
    for (std::string const& name : quest.StartGoals)
    {
        auto const found = quest.Goals.find(name);
        if (found == quest.Goals.end())
            continue;
        QuestGoalRow const& row = found->second.Row;
        QuestWireEncoder::GoalEntryFull goal;
        goal.DisplayKey = row.TitleKey;
        goal.GoalTitle = row.TitleKey;
        goal.PersonaName = row.PersonaName;
        goal.Type = QuestBook::WireType(found->second.Type);
        goal.Total = static_cast<int32>(row.TallyCount);
        goal.UseTally = row.TallyCount > 0;
        goal.TallyText = row.TallyDescriptorKey;
        goal.TallyText2 = row.TallyDescriptor2Key;
        goal.Location = row.LocationKey;
        goal.DestinationZone = row.DestinationZone;
        goal.Image1 = row.Image1;
        goal.Image2 = row.Image2;
        goal.ClientTags = found->second.ClientTags;
        goal.Madlibs = QuestBook::GoalMadlibs(row, 0);
        compilation.Goals.push_back(std::move(goal));
    }
    return compilation;
}

QuestOffers::Encoded QuestOffers::Encode(TypeCatalogPtr const& catalog, QuestInfo const& quest, RequirementContext const& facts)
{
    Encoded encoded;
    encoded.Dialog = QuestBook::Blob(QuestWireEncoder::BuildActorDialog(catalog, BuildPrepDialog(quest, facts)), QuestWireEncoder::ActorDialogMask, "Prep dialog", encoded.Error);
    if (!encoded.Ok())
        return encoded;
    encoded.GoalData = QuestBook::Blob(QuestWireEncoder::BuildGoalCompilation(catalog, BuildStartGoals(quest)), QuestWireEncoder::GoalsAndRewardsMask, "start goals", encoded.Error);
    if (!encoded.Ok())
        return encoded;
    encoded.Rewards = QuestBook::Blob(QuestWireEncoder::BuildLootInfoList(catalog, QuestRewards::Preview(quest.Results)), QuestWireEncoder::GoalsAndRewardsMask, "rewards", encoded.Error);
    return encoded;
}
