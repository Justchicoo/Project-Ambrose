/*
 * Project Ambrose by Imjustchico
 * Keeps a wizard's quests, goals, registry and hidden quests, and converts them to and from the stored rows.
 */

#include "QuestLog.h"
#include "QuestStore.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

QuestLogGoal const* QuestLogQuest::FindGoal(std::string_view goal) const
{
    auto const found = std::ranges::find(Goals, goal, &QuestLogGoal::Name);
    return found == Goals.end() ? nullptr : &*found;
}

QuestLog QuestLog::FromStored(CharacterQuests const& stored, QuestStore const* templates, std::vector<std::string>& warnings)
{
    QuestLog log;
    for (CharacterQuestRow const& row : stored.Quests)
    {
        QuestInfo const* const quest = templates ? templates->Find(row.QuestName) : nullptr;
        if (templates && !quest)
        {
            std::size_t const goals = std::ranges::count(stored.Goals, row.QuestGid, &CharacterQuestGoalRow::QuestGid);
            warnings.push_back(fmt::format("quest {} (GID {}) is no longer a quest template, so it and its {} goal(s) are dropped", row.QuestName, row.QuestGid, goals));
            log._dirty = true;
            continue;
        }
        QuestLogQuest entry{ row.QuestGid, row.QuestName, row.AcceptedAt, {} };
        for (CharacterQuestGoalRow const& goal : stored.Goals)
        {
            if (goal.QuestGid != row.QuestGid)
                continue;
            if (quest && !quest->Goals.contains(goal.GoalName))
            {
                warnings.push_back(fmt::format("goal {} (GID {}) of quest {} is no longer one of its goals, so it is dropped", goal.GoalName, goal.GoalGid, row.QuestName));
                log._dirty = true;
                continue;
            }
            entry.Goals.push_back({ goal.GoalGid, goal.GoalName, goal.Complete, goal.Count });
        }
        log._quests.push_back(std::move(entry));
    }
    for (CharacterRegistryRow const& row : stored.QuestRegistry)
        log._questRegistry[row.QuestName][row.Entry] = row.Value;
    for (CharacterRegistryRow const& row : stored.Registry)
        log._registry[row.Entry] = row.Value;
    log._hidden.insert(stored.Hidden.begin(), stored.Hidden.end());
    return log;
}

CharacterQuests QuestLog::ToStored(uint64 revision) const
{
    CharacterQuests stored;
    stored.Revision = revision;
    for (QuestLogQuest const& quest : _quests)
    {
        stored.Quests.push_back({ quest.Gid, quest.Name, quest.AcceptedAt });
        for (QuestLogGoal const& goal : quest.Goals)
            stored.Goals.push_back({ goal.Gid, quest.Gid, goal.Name, goal.Complete, goal.Count });
    }
    for (auto const& [quest, entries] : _questRegistry)
        for (auto const& [entry, value] : entries)
            stored.QuestRegistry.push_back({ quest, entry, value });
    for (auto const& [entry, value] : _registry)
        stored.Registry.push_back({ {}, entry, value });
    stored.Hidden.assign(_hidden.begin(), _hidden.end());
    return stored;
}

QuestLogQuest const* QuestLog::Add(std::string_view quest, GuidGenerator& gids, uint64 now)
{
    if (quest.empty() || HasQuest(quest))
        return nullptr;
    std::optional<uint64> const gid = gids.Generate();
    if (!gid)
        return nullptr;
    _quests.push_back({ *gid, std::string(quest), now, {} });
    _dirty = true;
    return &_quests.back();
}

QuestLogGoal const* QuestLog::StartGoal(std::string_view quest, std::string_view goal, GuidGenerator& gids)
{
    QuestLogQuest* const entry = FindMutable(quest);
    if (!entry || goal.empty() || entry->FindGoal(goal))
        return nullptr;
    std::optional<uint64> const gid = gids.Generate();
    if (!gid)
        return nullptr;
    entry->Goals.push_back({ *gid, std::string(goal), false, 0 });
    _dirty = true;
    return &entry->Goals.back();
}

std::optional<uint32> QuestLog::IncrementGoal(std::string_view quest, std::string_view goal, uint32 by)
{
    QuestLogGoal* const entry = FindGoalMutable(quest, goal);
    if (!entry || entry->Complete)
        return std::nullopt;
    uint32 constexpr most = std::numeric_limits<uint32>::max();
    entry->Count = by > most - entry->Count ? most : entry->Count + by;
    _dirty = true;
    return entry->Count;
}

bool QuestLog::CompleteGoal(std::string_view quest, std::string_view goal)
{
    QuestLogGoal* const entry = FindGoalMutable(quest, goal);
    if (!entry || entry->Complete)
        return false;
    entry->Complete = true;
    _dirty = true;
    return true;
}

bool QuestLog::CompleteQuest(std::string_view quest)
{
    std::string const name(quest);
    if (!Remove(name))
        return false;
    SetQuestRegistry(name, CompleteEntry, 1.0);
    return true;
}

bool QuestLog::Remove(std::string_view quest)
{
    auto const found = std::ranges::find(_quests, quest, &QuestLogQuest::Name);
    if (found == _quests.end())
        return false;
    _quests.erase(found);
    _dirty = true;
    return true;
}

QuestLogQuest const* QuestLog::Find(std::string_view quest) const
{
    auto const found = std::ranges::find(_quests, quest, &QuestLogQuest::Name);
    return found == _quests.end() ? nullptr : &*found;
}

bool QuestLog::IsGoalActive(std::string_view quest, std::string_view goal) const
{
    QuestLogQuest const* const entry = Find(quest);
    QuestLogGoal const* const found = entry ? entry->FindGoal(goal) : nullptr;
    return found && !found->Complete;
}

bool QuestLog::IsGoalCompleted(std::string_view quest, std::string_view goal) const
{
    QuestLogQuest const* const entry = Find(quest);
    QuestLogGoal const* const found = entry ? entry->FindGoal(goal) : nullptr;
    return found && found->Complete;
}

bool QuestLog::HasCompletedQuest(std::string_view quest) const
{
    std::optional<double> const value = GetQuestRegistry(quest, CompleteEntry);
    return value && *value >= 1.0;
}

std::optional<double> QuestLog::GetQuestRegistry(std::string_view quest, std::string_view entry) const
{
    auto const entries = _questRegistry.find(quest);
    if (entries == _questRegistry.end())
        return std::nullopt;
    auto const found = entries->second.find(entry);
    return found == entries->second.end() ? std::nullopt : std::optional<double>(found->second);
}

void QuestLog::SetQuestRegistry(std::string_view quest, std::string_view entry, double value)
{
    auto entries = _questRegistry.find(quest);
    if (entries == _questRegistry.end())
        entries = _questRegistry.emplace(std::string(quest), std::map<std::string, double, std::less<>>{}).first;
    auto const found = entries->second.find(entry);
    if (found != entries->second.end() && found->second == value)
        return;
    entries->second.insert_or_assign(std::string(entry), value);
    _dirty = true;
}

std::optional<double> QuestLog::GetRegistry(std::string_view entry) const
{
    auto const found = _registry.find(entry);
    return found == _registry.end() ? std::nullopt : std::optional<double>(found->second);
}

void QuestLog::SetRegistry(std::string_view entry, double value)
{
    auto const found = _registry.find(entry);
    if (found != _registry.end() && found->second == value)
        return;
    _registry.insert_or_assign(std::string(entry), value);
    _dirty = true;
}

void QuestLog::SetHidden(std::string_view quest, bool hidden)
{
    auto const found = _hidden.find(quest);
    if (hidden == (found != _hidden.end()))
        return;
    if (hidden)
        _hidden.emplace(quest);
    else
        _hidden.erase(found);
    _dirty = true;
}

QuestLogQuest* QuestLog::FindMutable(std::string_view quest)
{
    auto const found = std::ranges::find(_quests, quest, &QuestLogQuest::Name);
    return found == _quests.end() ? nullptr : &*found;
}

QuestLogGoal* QuestLog::FindGoalMutable(std::string_view quest, std::string_view goal)
{
    QuestLogQuest* const entry = FindMutable(quest);
    if (!entry)
        return nullptr;
    auto const found = std::ranges::find(entry->Goals, goal, &QuestLogGoal::Name);
    return found == entry->Goals.end() ? nullptr : &*found;
}
