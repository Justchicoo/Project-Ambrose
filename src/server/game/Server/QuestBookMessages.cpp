/*
 * Project Ambrose by Imjustchico
 * Builds MSG_SENDQUEST and MSG_SENDGOAL from a quest's template and the wizard's own quest and goal, with their GIDs, keys, counts and encoded blobs.
 */

#include "QuestBookMessages.h"
#include "QuestBook.h"
#include "QuestRewards.h"

#include <fmt/format.h>

std::optional<GameMessages::SendQuest> QuestBookMessages::Quest(TypeCatalogPtr const& catalog, QuestInfo const& quest, QuestLogQuest const& held, bool isNew, std::string& error)
{
    GameMessages::SendQuest message;
    message.QuestId = held.Gid;
    message.QuestNameId = quest.Row.NameId;
    message.QuestLevel = static_cast<int32>(quest.Row.Level);
    message.QuestTitle = quest.Row.TitleKey;
    message.QuestInfo = quest.Row.InfoKey;
    message.New = isNew ? 1 : 0;
    Quests::QuestTemplate madlib;
    madlib.Name = quest.Row.Name;
    madlib.Level = static_cast<int32>(quest.Row.Level);
    message.QuestMadlibs = QuestBook::Blob(QuestWireEncoder::BuildMadlibBlock(catalog, QuestMadlibs::BuildQuest(madlib)), QuestWireEncoder::GoalsAndRewardsMask, "madlibs", error);
    if (!error.empty())
        return std::nullopt;
    message.Rewards = QuestBook::Blob(QuestWireEncoder::BuildLootInfoList(catalog, QuestRewards::Preview(quest.Results)), QuestWireEncoder::GoalsAndRewardsMask, "rewards", error);
    if (!error.empty())
        return std::nullopt;
    message.AssociatedWorlds = QuestBook::Blob(QuestWireEncoder::BuildAssociatedWorldsList(catalog, { QuestBook::AssociatedWorlds(quest) }), QuestWireEncoder::GoalsAndRewardsMask, "associated worlds", error);
    if (!error.empty())
        return std::nullopt;
    message.NoQuestHelper = quest.Row.NoQuestHelper ? 1 : 0;
    message.Mainline = quest.Row.Mainline ? 1 : 0;
    message.SkipQhAutoSelect = quest.Row.SkipQhAutoselect ? 1 : 0;
    message.PetOnlyQuest = quest.Row.PetOnly ? 1 : 0;
    message.ActivityType = static_cast<uint8>(quest.Row.ActivityType);
    return message;
}

std::optional<GameMessages::SendGoal> QuestBookMessages::Goal(TypeCatalogPtr const& catalog, QuestInfo const& quest, QuestLogQuest const& held, QuestLogGoal const& goal, int32 sendType, std::string& error)
{
    auto const found = quest.Goals.find(goal.Name);
    if (found == quest.Goals.end())
    {
        error = fmt::format("quest {} has no goal {}", quest.Row.Name, goal.Name);
        return std::nullopt;
    }
    QuestGoalRow const& row = found->second.Row;
    GameMessages::SendGoal message;
    message.QuestId = held.Gid;
    message.GoalId = goal.Gid;
    message.GoalNameId = row.NameId;
    message.GoalTitle = row.TitleKey;
    message.GoalLocation = row.LocationKey;
    message.GoalDestinationZone = row.DestinationZone;
    message.GoalImage1 = row.Image1;
    message.GoalImage2 = row.Image2;
    message.PersonaName = row.PersonaName;
    message.GoalType = static_cast<uint8>(QuestBook::WireType(found->second.Type));
    message.GoalStatus = goal.Complete ? QuestBook::GoalComplete : QuestBook::GoalActive;
    message.GoalCount = static_cast<int32>(goal.Count);
    message.GoalTotal = static_cast<int32>(row.TallyCount);
    message.UseTally = row.TallyCount > 0 ? 1 : 0;
    message.TallyText = row.TallyDescriptorKey;
    message.SendType = sendType;
    message.GoalMadlibs = QuestBook::Blob(QuestWireEncoder::BuildMadlibBlock(catalog, QuestBook::GoalMadlibs(row, message.GoalCount)), QuestWireEncoder::GoalsAndRewardsMask, "goal madlibs", error);
    if (!error.empty())
        return std::nullopt;
    if (!found->second.ClientTags.empty())
    {
        message.ClientTags = QuestBook::Blob(QuestWireEncoder::BuildClientTagList(catalog, { found->second.ClientTags }), QuestWireEncoder::GoalsAndRewardsMask, "client tags", error);
        if (!error.empty())
            return std::nullopt;
    }
    auto const prep = quest.Dialogs.find(QuestStore::TagPrep);
    if (prep != quest.Dialogs.end() && !prep->second.Lines.empty())
        message.PatronIcon = prep->second.Lines.front().Row.Picture;
    message.NoQuestHelper = quest.Row.NoQuestHelper ? 1 : 0;
    message.PetOnlyQuest = quest.Row.PetOnly ? 1 : 0;
    return message;
}
