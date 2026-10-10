/*
 * Project Ambrose by Imjustchico
 * Picks the quests an NPC can offer a wizard and builds the blobs of their offer.
 */

#include "QuestOffers.h"

#include <fmt/format.h>

#include <algorithm>
#include <tuple>
#include <utility>

namespace
{
    Quests::GoalType WireType(QuestGoalType type) noexcept
    {
        switch (type)
        {
            case QuestGoalType::Persona: return Quests::GoalType::Persona;
            case QuestGoalType::Waypoint: return Quests::GoalType::Waypoint;
            case QuestGoalType::Bounty: return Quests::GoalType::Bounty;
            case QuestGoalType::BountyCollect: return Quests::GoalType::BountyCollect;
            case QuestGoalType::Scavenge: return Quests::GoalType::Scavenge;
            case QuestGoalType::Usage: return Quests::GoalType::Usage;
            case QuestGoalType::AchieveRank: return Quests::GoalType::AchieveRank;
        }
        return Quests::GoalType::Unknown;
    }

    std::string Blob(QuestWireEncoder::ObjectBuildResult const& built, uint32 mask, std::string_view what, std::string& error)
    {
        if (!built.Ok())
        {
            error = fmt::format("its {} could not be built: {}", what, built.Error);
            return {};
        }
        QuestWireEncoder::BlobEncodeResult const encoded = QuestWireEncoder::Encode(*built.Object, mask);
        if (!encoded.Ok())
        {
            error = fmt::format("its {} could not be encoded: {}", what, encoded.Error);
            return {};
        }
        return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
    }
}

std::vector<QuestInfo const*> QuestOffers::Available(QuestStore const& quests, uint32 templateId, QuestLog const& log, RequirementContext const& context)
{
    std::vector<QuestInfo const*> available;
    for (std::string const& name : quests.GetQuestsOfferedBy(templateId))
    {
        QuestInfo const* const quest = quests.Find(name);
        if (!quest || log.HasQuest(name) || (log.HasCompletedQuest(name) && quest->Row.Repeat == 0))
            continue;
        if (!quest->Row.RequirementListId.empty() && !sRequirementMgr.Evaluate(quest->Row.RequirementListId, context))
            continue;
        available.push_back(quest);
    }
    std::ranges::sort(available, [](QuestInfo const* left, QuestInfo const* right)
    {
        return std::tie(left->Row.TitleKey, left->Row.Name) < std::tie(right->Row.TitleKey, right->Row.Name);
    });
    return available;
}

Dialogs::ActorDialog QuestOffers::BuildPrepDialog(QuestInfo const& quest)
{
    Dialogs::ActorDialog dialog;
    dialog.Tag = std::string(QuestStore::TagPrep);
    auto const prep = quest.Dialogs.find(QuestStore::TagPrep);
    if (prep == quest.Dialogs.end())
        return dialog;
    for (QuestDialogLine const& line : prep->second.Lines)
    {
        Dialogs::ActorDialogEntry entry;
        entry.ActorTemplateId = line.Row.ActorTemplateId;
        entry.DialogKey = line.Row.DialogKey;
        entry.Picture = line.Row.Picture;
        entry.Sound = line.Row.Sound;
        entry.Action = line.Row.Action;
        entry.DialogEvent = line.Row.DialogEvent;
        if (!line.Row.Animation.empty())
            entry.Animations.push_back(line.Row.Animation);
        dialog.Entries.push_back(std::move(entry));
    }
    return dialog;
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
        goal.PersonaName = row.PersonaName;
        goal.Type = WireType(found->second.Type);
        goal.Total = static_cast<int32>(row.TallyCount);
        goal.UseTally = row.TallyCount > 0;
        goal.TallyText = row.TallyDescriptorKey;
        goal.TallyText2 = row.TallyDescriptor2Key;
        goal.Location = row.LocationKey;
        goal.DestinationZone = row.DestinationZone;
        goal.Image1 = row.Image1;
        goal.Image2 = row.Image2;
        goal.ClientTags = found->second.ClientTags;
        Quests::GoalTemplate madlib;
        madlib.Name = row.Goal;
        madlib.LocationName = row.LocationKey;
        if (row.TallyCount > 0)
            madlib.TallyCounter = Quests::TallyCounterTemplate{ 0.0f, row.TallyDescriptorKey, row.TallyDescriptor2Key, static_cast<int32>(row.TallyCount) };
        goal.Madlibs = QuestMadlibs::BuildGoal(madlib, 0, goal.Total);
        compilation.Goals.push_back(std::move(goal));
    }
    return compilation;
}

QuestOffers::Encoded QuestOffers::Encode(TypeCatalogPtr const& catalog, QuestInfo const& quest)
{
    Encoded encoded;
    encoded.Dialog = Blob(QuestWireEncoder::BuildActorDialog(catalog, BuildPrepDialog(quest)), QuestWireEncoder::ActorDialogMask, "Prep dialog", encoded.Error);
    if (!encoded.Ok())
        return encoded;
    encoded.GoalData = Blob(QuestWireEncoder::BuildGoalCompilation(catalog, BuildStartGoals(quest)), QuestWireEncoder::GoalsAndRewardsMask, "start goals", encoded.Error);
    if (!encoded.Ok())
        return encoded;
    encoded.Rewards = Blob(QuestWireEncoder::BuildLootInfoList(catalog, {}), QuestWireEncoder::GoalsAndRewardsMask, "rewards", encoded.Error);
    return encoded;
}
