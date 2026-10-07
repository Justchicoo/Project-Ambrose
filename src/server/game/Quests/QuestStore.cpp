/*
 * Project Ambrose by Imjustchico
 * Builds a quest generation from the quest table rows, leaving out the quests it is told to skip and any row that names a quest or goal it does not hold, puts adjectives, client tags, logic, dialog lines and madlibs in their authored order, takes each quest's starter from the first line of its Prep dialog, and fills the lookups by template, object name, tag, adjective and zone.
 */

#include "QuestStore.h"

#include <algorithm>
#include <array>
#include <utility>

namespace
{
    struct GoalTypeName
    {
        std::string_view Name;
        QuestGoalType Type;
    };

    constexpr std::array<GoalTypeName, 7> GoalTypes{ {
        { "persona", QuestGoalType::Persona },
        { "waypoint", QuestGoalType::Waypoint },
        { "bounty", QuestGoalType::Bounty },
        { "bounty_collect", QuestGoalType::BountyCollect },
        { "scavenge", QuestGoalType::Scavenge },
        { "usage", QuestGoalType::Usage },
        { "achieve_rank", QuestGoalType::AchieveRank } } };

    constexpr std::array<std::string_view, 5> DialogTags{ "Start", "Prep", "Underway", "Completion", "Complete" };

    template<typename Value>
    void Ordered(std::vector<std::pair<uint32, Value>>& items, std::vector<Value>& out)
    {
        std::stable_sort(items.begin(), items.end(), [](auto const& left, auto const& right) { return left.first < right.first; });
        out.clear();
        for (auto& item : items)
            out.push_back(std::move(item.second));
    }
}

std::optional<QuestGoalType> QuestStore::ParseGoalType(std::string_view text)
{
    for (GoalTypeName const& entry : GoalTypes)
        if (entry.Name == text)
            return entry.Type;
    return std::nullopt;
}

bool QuestStore::IsDialogTag(std::string_view tag)
{
    return std::find(DialogTags.begin(), DialogTags.end(), tag) != DialogTags.end();
}

bool QuestStore::IsLogicKind(std::string_view kind)
{
    return kind == KindAnd || kind == KindOr || kind == KindAdd;
}

std::shared_ptr<QuestStore const> QuestStore::Build(QuestRows const& rows, std::set<std::string, std::less<>> const& skipped)
{
    auto store = std::make_shared<QuestStore>();
    for (QuestTemplateRow const& row : rows.Quests)
        if (!row.Name.empty() && !skipped.contains(row.Name))
            store->_quests.try_emplace(row.Name, QuestInfo{ row, {}, {}, {}, 0 });

    auto quest = [&store](std::string const& name) -> QuestInfo*
    {
        auto const found = store->_quests.find(name);
        return found == store->_quests.end() ? nullptr : &found->second;
    };
    auto goal = [&quest](std::string const& questName, std::string const& goalName) -> QuestGoal*
    {
        QuestInfo* const info = quest(questName);
        if (!info)
            return nullptr;
        auto const found = info->Goals.find(goalName);
        return found == info->Goals.end() ? nullptr : &found->second;
    };
    auto dialogs = [&quest, &goal](std::string const& questName, std::string const& owner, std::string const& goalName) -> std::map<std::string, QuestDialog, std::less<>>*
    {
        if (owner == OwnerQuest && goalName.empty())
        {
            QuestInfo* const info = quest(questName);
            return info ? &info->Dialogs : nullptr;
        }
        if (owner == OwnerGoal)
        {
            QuestGoal* const found = goal(questName, goalName);
            return found ? &found->Dialogs : nullptr;
        }
        return nullptr;
    };

    for (QuestGoalRow const& row : rows.Goals)
    {
        QuestInfo* const info = quest(row.Quest);
        std::optional<QuestGoalType> const type = ParseGoalType(row.Type);
        if (!info || !type)
            continue;
        QuestGoal next;
        next.Row = row;
        next.Type = *type;
        info->Goals.try_emplace(row.Goal, std::move(next));
    }
    for (QuestStartGoalRow const& row : rows.StartGoals)
        if (QuestInfo* const info = quest(row.Quest); info && info->Goals.contains(row.Goal))
            info->StartGoals.push_back(row.Goal);

    std::map<std::pair<std::string, std::string>, std::vector<std::pair<uint32, std::string>>> adjectives;
    for (QuestGoalAdjectiveRow const& row : rows.Adjectives)
        adjectives[{ row.Quest, row.Goal }].emplace_back(row.Position, row.Adjective);
    for (auto& [key, items] : adjectives)
        if (QuestGoal* const found = goal(key.first, key.second))
            Ordered(items, found->Adjectives);

    std::map<std::pair<std::string, std::string>, std::vector<std::pair<uint32, std::string>>> tags;
    for (QuestGoalClientTagRow const& row : rows.ClientTags)
        tags[{ row.Quest, row.Goal }].emplace_back(row.Position, row.Tag);
    for (auto& [key, items] : tags)
        if (QuestGoal* const found = goal(key.first, key.second))
            Ordered(items, found->ClientTags);

    for (QuestGoalLogicRow const& row : rows.Logic)
        if (QuestGoal* const found = goal(row.Quest, row.Goal))
            found->Logic.push_back(QuestGoalLogic{ row.Position, row.CompleteQuest, {}, {}, {} });
    std::vector<QuestGoalLogicMemberRow const*> members;
    for (QuestGoalLogicMemberRow const& row : rows.LogicMembers)
        members.push_back(&row);
    std::stable_sort(members.begin(), members.end(), [](auto const* left, auto const* right) { return left->Position < right->Position; });
    for (QuestGoalLogicMemberRow const* row : members)
    {
        QuestGoal* const found = goal(row->Quest, row->Goal);
        if (!found)
            continue;
        auto const logic = std::find_if(found->Logic.begin(), found->Logic.end(), [row](QuestGoalLogic const& entry) { return entry.Position == row->Logic; });
        if (logic == found->Logic.end())
            continue;
        if (row->Kind == KindAnd)
            logic->RequiresAll.push_back(row->Member);
        else if (row->Kind == KindOr)
            logic->RequiresAny.push_back(row->Member);
        else if (row->Kind == KindAdd)
            logic->GoalsToAdd.push_back(row->Member);
    }

    for (QuestDialogRow const& row : rows.Dialogs)
        if (auto* const owned = dialogs(row.Quest, row.Owner, row.Goal); owned && IsDialogTag(row.Tag))
            owned->try_emplace(row.Tag, QuestDialog{ row.Tag, {} });
    for (QuestDialogEntryRow const& row : rows.DialogEntries)
    {
        auto* const owned = dialogs(row.Quest, row.Owner, row.Goal);
        if (!owned)
            continue;
        auto const found = owned->find(row.Tag);
        if (found != owned->end())
            found->second.Lines.push_back(QuestDialogLine{ row, {} });
    }
    for (QuestDialogMadlibRow const& row : rows.Madlibs)
    {
        auto* const owned = dialogs(row.Quest, row.Owner, row.Goal);
        if (!owned)
            continue;
        auto const found = owned->find(row.Tag);
        if (found == owned->end())
            continue;
        for (QuestDialogLine& line : found->second.Lines)
            if (line.Row.Position == row.Entry)
                line.Madlibs.push_back(QuestMadlib{ row.Identifier, row.Value });
    }

    auto const sortDialogs = [](std::map<std::string, QuestDialog, std::less<>>& owned)
    {
        for (auto& [tag, dialog] : owned)
            std::stable_sort(dialog.Lines.begin(), dialog.Lines.end(), [](QuestDialogLine const& left, QuestDialogLine const& right) { return left.Row.Position < right.Row.Position; });
    };
    for (auto& [name, info] : store->_quests)
    {
        sortDialogs(info.Dialogs);
        for (auto& [goalName, entry] : info.Goals)
        {
            sortDialogs(entry.Dialogs);
            std::stable_sort(entry.Logic.begin(), entry.Logic.end(), [](QuestGoalLogic const& left, QuestGoalLogic const& right) { return left.Position < right.Position; });
        }
    }
    store->Index();
    return store;
}

void QuestStore::Index()
{
    _goals = 0;
    for (auto& [name, info] : _quests)
    {
        auto const prep = info.Dialogs.find(TagPrep);
        if (prep != info.Dialogs.end() && !prep->second.Lines.empty())
            info.Starter = prep->second.Lines.front().Row.ActorTemplateId;
        if (info.Starter != 0)
            _starterByTemplateId[info.Starter].push_back(name);
        for (auto const& [goalName, goal] : info.Goals)
        {
            ++_goals;
            QuestGoalRef const ref{ name, goalName };
            switch (goal.Type)
            {
                case QuestGoalType::Persona:
                    if (!goal.Row.PersonaName.empty())
                        _personaGoalsByObjectName[goal.Row.PersonaName].push_back(ref);
                    break;
                case QuestGoalType::Usage:
                    for (std::string const& tag : goal.ClientTags)
                        _usageGoalsByTag[tag].push_back(ref);
                    for (std::string const& adjective : goal.Adjectives)
                        _usageGoalsByAdjective[adjective].push_back(ref);
                    break;
                case QuestGoalType::Bounty:
                case QuestGoalType::BountyCollect:
                    for (std::string const& adjective : goal.Adjectives)
                        _bountyGoalsByAdjective[adjective].push_back(ref);
                    break;
                case QuestGoalType::Waypoint:
                    if (!goal.Row.DestinationZone.empty())
                        _waypointGoalsByZone[goal.Row.DestinationZone].push_back(ref);
                    break;
                default:
                    break;
            }
        }
    }
}

QuestInfo const* QuestStore::Find(std::string_view name) const
{
    auto const found = _quests.find(name);
    return found == _quests.end() ? nullptr : &found->second;
}

std::vector<std::string> QuestStore::GetQuestsOfferedBy(uint32 templateId) const
{
    if (templateId == 0)
        return {};
    auto const found = _starterByTemplateId.find(templateId);
    return found == _starterByTemplateId.end() ? std::vector<std::string>{} : found->second;
}

std::vector<QuestGoalRef> QuestStore::Lookup(GoalIndex const& index, std::string_view key)
{
    auto const found = index.find(key);
    return found == index.end() ? std::vector<QuestGoalRef>{} : found->second;
}

std::vector<QuestGoalRef> QuestStore::GetPersonaGoals(std::string_view objectName) const
{
    return Lookup(_personaGoalsByObjectName, objectName);
}

std::vector<QuestGoalRef> QuestStore::GetUsageGoalsByTag(std::string_view tag) const
{
    return Lookup(_usageGoalsByTag, tag);
}

std::vector<QuestGoalRef> QuestStore::GetUsageGoalsByAdjective(std::string_view adjective) const
{
    return Lookup(_usageGoalsByAdjective, adjective);
}

std::vector<QuestGoalRef> QuestStore::GetBountyGoals(std::string_view adjective) const
{
    return Lookup(_bountyGoalsByAdjective, adjective);
}

std::vector<QuestGoalRef> QuestStore::GetWaypointGoals(std::string_view zone) const
{
    return Lookup(_waypointGoalsByZone, zone);
}
