/*
 * Project Ambrose by Imjustchico
 * Builds MSG_SENDQUEST and MSG_SENDGOAL from a quest's template and the wizard's own quest and goal, with their GIDs, keys, counts and encoded blobs, and turns a quest log, or the changes it handed out, into the quest book messages that show them.
 */

#include "QuestBookMessages.h"
#include "QuestBook.h"
#include "QuestRewards.h"

#include <fmt/format.h>

#include <utility>

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

QuestBookUpdate QuestBookMessages::Resume(TypeCatalogPtr const& catalog, QuestStore const& quests, QuestLog const& log)
{
    QuestBookUpdate update;
    for (QuestLogQuest const& held : log.GetQuests())
    {
        QuestInfo const* const quest = quests.Find(held.Name);
        std::string problem;
        std::optional<GameMessages::SendQuest> message = quest ? Quest(catalog, *quest, held, false, problem) : std::nullopt;
        if (!message)
        {
            update.Problems.push_back(fmt::format("quest {}: {}", held.Name, quest ? problem : std::string("it has no template")));
            continue;
        }
        update.Messages.emplace_back(std::move(*message));
        for (QuestLogGoal const& goal : held.Goals)
        {
            if (goal.Complete)
                continue;
            if (std::optional<GameMessages::SendGoal> sent = Goal(catalog, *quest, held, goal, QuestBook::SendResume, problem))
                update.Messages.emplace_back(std::move(*sent));
            else
                update.Problems.push_back(fmt::format("goal {} of {}: {}", goal.Name, held.Name, problem));
        }
    }
    return update;
}

QuestBookUpdate QuestBookMessages::Changes(TypeCatalogPtr const& catalog, QuestStore const& quests, QuestLog const& log, std::vector<QuestLogChange> const& changes)
{
    QuestBookUpdate update;
    for (QuestLogChange const& change : changes)
    {
        QuestInfo const* const quest = quests.Find(change.Quest);
        switch (change.Kind)
        {
            case QuestLogChangeKind::QuestRemoved:
                update.Messages.emplace_back(GameMessages::RemoveQuest{ change.QuestGid, 0 });
                continue;
            case QuestLogChangeKind::QuestCompleted:
                update.Messages.emplace_back(GameMessages::CompleteQuest{ change.QuestGid, quest ? quest->Row.CompleteKey : std::string() });
                continue;
            case QuestLogChangeKind::GoalCompleted:
            {
                std::string text;
                if (quest)
                    if (auto const goal = quest->Goals.find(change.Goal); goal != quest->Goals.end())
                        text = goal->second.Row.CompleteKey;
                update.Messages.emplace_back(GameMessages::CompleteGoal{ change.QuestGid, change.GoalGid, std::move(text) });
                continue;
            }
            default:
                break;
        }
        QuestLogQuest const* const held = log.Find(change.Quest);
        if (!held || !quest || held->Gid != change.QuestGid)
            continue;
        std::string problem;
        if (change.Kind == QuestLogChangeKind::QuestAdded)
        {
            if (std::optional<GameMessages::SendQuest> message = Quest(catalog, *quest, *held, true, problem))
                update.Messages.emplace_back(std::move(*message));
            else
                update.Problems.push_back(fmt::format("new quest {}: {}", change.Quest, problem));
            continue;
        }
        QuestLogGoal const* const goal = held->FindGoal(change.Goal);
        if (!goal || (goal->Complete && change.Kind != QuestLogChangeKind::GoalStarted))
            continue;
        int32 const sendType = change.Kind == QuestLogChangeKind::GoalStarted ? QuestBook::SendStart : QuestBook::SendProgress;
        if (std::optional<GameMessages::SendGoal> message = Goal(catalog, *quest, *held, *goal, sendType, problem))
            update.Messages.emplace_back(std::move(*message));
        else
            update.Problems.push_back(fmt::format("goal {} of {}: {}", change.Goal, change.Quest, problem));
    }
    return update;
}
