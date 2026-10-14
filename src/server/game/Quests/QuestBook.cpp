/*
 * Project Ambrose by Imjustchico
 * Maps a quest's goal types to the client's, builds a goal's madlibs, encodes a built object bare, gathers the worlds a quest's goals lead to, and turns a stored dialog's lines into an ActorDialog.
 */

#include "QuestBook.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

Quests::GoalType QuestBook::WireType(QuestGoalType type) noexcept
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

QuestMadlibs::Block QuestBook::GoalMadlibs(QuestGoalRow const& row, int32 count)
{
    Quests::GoalTemplate madlib;
    madlib.Name = row.Goal;
    madlib.LocationName = row.LocationKey;
    if (row.TallyCount > 0)
        madlib.TallyCounter = Quests::TallyCounterTemplate{ 0.0f, row.TallyDescriptorKey, row.TallyDescriptor2Key, static_cast<int32>(row.TallyCount) };
    return QuestMadlibs::BuildGoal(madlib, count, static_cast<int32>(row.TallyCount));
}

std::string QuestBook::Blob(QuestWireEncoder::ObjectBuildResult const& built, uint32 mask, std::string_view what, std::string& error)
{
    if (!built.Ok())
    {
        error = fmt::format("its {} could not be built: {}", what, built.Error);
        return {};
    }
    QuestWireEncoder::BlobEncodeResult const encoded = QuestWireEncoder::EncodeBare(*built.Object, mask);
    if (!encoded.Ok())
    {
        error = fmt::format("its {} could not be encoded: {}", what, encoded.Error);
        return {};
    }
    return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
}

std::vector<std::string> QuestBook::AssociatedWorlds(QuestInfo const& quest)
{
    std::vector<std::string> worlds;
    for (auto const& [name, goal] : quest.Goals)
    {
        std::string_view const zone = goal.Row.DestinationZone;
        std::string const world(zone.substr(0, zone.find('/')));
        if (!world.empty() && std::ranges::find(worlds, world) == worlds.end())
            worlds.push_back(world);
    }
    return worlds;
}

Dialogs::ActorDialog QuestBook::Dialog(std::map<std::string, QuestDialog, std::less<>> const& dialogs, std::string_view tag)
{
    Dialogs::ActorDialog dialog;
    dialog.Tag = std::string(tag);
    auto const found = dialogs.find(tag);
    if (found == dialogs.end())
        return dialog;
    for (QuestDialogLine const& line : found->second.Lines)
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
