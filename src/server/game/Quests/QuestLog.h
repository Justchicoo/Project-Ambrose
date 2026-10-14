/*
 * Project Ambrose by Imjustchico
 * A wizard's quest log: its active quests in the order it took them, each with the GID it keeps for life and its goals by their own GIDs, status and count, its per-quest registry, where a completed quest leaves 'Complete' = 1, its other registry entries, the quests it hid and the Dynamods its progress made, and the quest events its changes publish, such as GoalComplete_<quest>_<goal>, held until the wizard's zone takes them, with each goal and quest it completes held until its session runs their results, and each quest added, completed or removed and goal started, moved or completed held until its session shows the client's quest book. Built from the stored rows, dropping with a warning each quest whose template is gone and each goal its quest no longer has, and turned back into rows under a save's revision; any change marks it for saving and is counted, so what depends on it can tell it moved.
 */

#ifndef AMBROSE_QUESTLOG_H
#define AMBROSE_QUESTLOG_H

#include "CharacterQuests.h"
#include "GuidGenerator.h"
#include "WizardDynaMods.h"

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

struct QuestCompletion
{
    std::string Quest;
    std::string Goal;
};

enum class QuestLogChangeKind : uint8
{
    QuestAdded,
    GoalStarted,
    GoalProgressed,
    GoalCompleted,
    QuestCompleted,
    QuestRemoved
};

struct QuestLogChange
{
    QuestLogChangeKind Kind = QuestLogChangeKind::QuestAdded;
    std::string Quest;
    uint64 QuestGid = 0;
    std::string Goal;
    uint64 GoalGid = 0;
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

    static std::string GoalCompleteEvent(std::string_view quest, std::string_view goal);

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
    WizardDynaMods const& GetDynaMods() const noexcept { return _dynaMods; }
    DynaModChange ApplyDynaMod(std::string_view zone, std::string_view tag, std::string_view state, bool remove, uint64 originator);
    std::vector<DynaMod> EraseDynaMods(std::string_view tag);
    std::vector<std::string> TakeEvents();
    std::vector<QuestCompletion> TakeCompletions();
    std::vector<QuestLogChange> TakeChanges();

    bool IsDirty() const noexcept { return _dirty; }
    void MarkSaved() noexcept { _dirty = false; }
    uint64 GetChangeCount() const noexcept { return _changes; }

private:
    QuestLogQuest* FindMutable(std::string_view quest);
    QuestLogGoal* FindGoalMutable(std::string_view quest, std::string_view goal);
    void MarkChanged() noexcept;

    std::vector<QuestLogQuest> _quests;
    std::map<std::string, std::map<std::string, double, std::less<>>, std::less<>> _questRegistry;
    std::map<std::string, double, std::less<>> _registry;
    std::set<std::string, std::less<>> _hidden;
    WizardDynaMods _dynaMods;
    std::vector<std::string> _events;
    std::vector<QuestCompletion> _completions;
    std::vector<QuestLogChange> _changeFeed;
    bool _dirty = false;
    uint64 _changes = 0;
};

#endif
