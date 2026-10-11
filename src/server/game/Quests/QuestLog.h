/*
 * Project Ambrose by Imjustchico
 * A wizard's quest log: its active quests in the order it took them, each with the GID it keeps for life and its goals by their own GIDs, status and count, its per-quest registry, where a completed quest leaves 'Complete' = 1, its other registry entries and the quests it hid. Built from the stored rows, dropping with a warning each quest whose template is gone and each goal its quest no longer has, and turned back into rows under a save's revision; any change marks it for saving.
 */

#ifndef AMBROSE_QUESTLOG_H
#define AMBROSE_QUESTLOG_H

#include "CharacterQuests.h"
#include "GuidGenerator.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class QuestStore;

struct QuestLogGoal
{
    uint64 Gid = 0;
    std::string Name;
    bool Complete = false;
    uint32 Count = 0;
};

struct QuestLogQuest
{
    uint64 Gid = 0;
    std::string Name;
    uint64 AcceptedAt = 0;
    std::vector<QuestLogGoal> Goals;

    QuestLogGoal const* FindGoal(std::string_view goal) const;
};

class QuestLog
{
public:
    static constexpr std::string_view CompleteEntry = "Complete";

    static QuestLog FromStored(CharacterQuests const& stored, QuestStore const* templates, std::vector<std::string>& warnings);
    CharacterQuests ToStored(uint64 revision) const;

    QuestLogQuest const* Add(std::string_view quest, GuidGenerator& gids, uint64 now);
    QuestLogGoal const* StartGoal(std::string_view quest, std::string_view goal, GuidGenerator& gids);
    std::optional<uint32> IncrementGoal(std::string_view quest, std::string_view goal, uint32 by = 1);
    bool CompleteGoal(std::string_view quest, std::string_view goal);
    bool CompleteQuest(std::string_view quest);
    bool Remove(std::string_view quest);

    QuestLogQuest const* Find(std::string_view quest) const;
    std::vector<QuestLogQuest> const& GetQuests() const noexcept { return _quests; }
    bool HasQuest(std::string_view quest) const { return Find(quest) != nullptr; }
    bool IsGoalActive(std::string_view quest, std::string_view goal) const;
    bool IsGoalCompleted(std::string_view quest, std::string_view goal) const;
    bool HasCompletedQuest(std::string_view quest) const;

    std::optional<double> GetQuestRegistry(std::string_view quest, std::string_view entry) const;
    void SetQuestRegistry(std::string_view quest, std::string_view entry, double value);
    std::optional<double> GetRegistry(std::string_view entry) const;
    void SetRegistry(std::string_view entry, double value);
    bool IsHidden(std::string_view quest) const { return _hidden.contains(quest); }
    void SetHidden(std::string_view quest, bool hidden);

    bool IsDirty() const noexcept { return _dirty; }
    void MarkSaved() noexcept { _dirty = false; }

private:
    QuestLogQuest* FindMutable(std::string_view quest);
    QuestLogGoal* FindGoalMutable(std::string_view quest, std::string_view goal);

    std::vector<QuestLogQuest> _quests;
    std::map<std::string, std::map<std::string, double, std::less<>>, std::less<>> _questRegistry;
    std::map<std::string, double, std::less<>> _registry;
    std::set<std::string, std::less<>> _hidden;
    bool _dirty = false;
};

#endif
