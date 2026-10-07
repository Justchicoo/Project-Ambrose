/*
 * Project Ambrose by Imjustchico
 * One generation of the authored quests, keyed by quest name, each with its start goals, its goals and their adjectives, client tags and completion logic, and the dialogs of the quest and of each goal in line order, together with the indexes the world asks: which quests a template offers through the first line of the quest's Prep dialog, which persona goals name an object, which usage goals carry a client tag or adjective, which bounty goals an adjective counts toward and which waypoint goals lead to a zone.
 */

#ifndef AMBROSE_QUESTSTORE_H
#define AMBROSE_QUESTSTORE_H

#include "QuestRows.h"
#include "Types.h"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum class QuestGoalType : uint8
{
    Persona,
    Waypoint,
    Bounty,
    BountyCollect,
    Scavenge,
    Usage,
    AchieveRank
};

struct QuestGoalRef
{
    std::string Quest;
    std::string Goal;

    bool operator==(QuestGoalRef const&) const = default;
};

struct QuestMadlib
{
    std::string Identifier;
    std::string Value;
};

struct QuestDialogLine
{
    QuestDialogEntryRow Row;
    std::vector<QuestMadlib> Madlibs;
};

struct QuestDialog
{
    std::string Tag;
    std::vector<QuestDialogLine> Lines;
};

struct QuestGoalLogic
{
    uint32 Position = 0;
    bool CompleteQuest = false;
    std::vector<std::string> RequiresAll;
    std::vector<std::string> RequiresAny;
    std::vector<std::string> GoalsToAdd;
};

struct QuestGoal
{
    QuestGoalRow Row;
    QuestGoalType Type = QuestGoalType::Persona;
    std::vector<std::string> Adjectives;
    std::vector<std::string> ClientTags;
    std::vector<QuestGoalLogic> Logic;
    std::map<std::string, QuestDialog, std::less<>> Dialogs;
};

struct QuestInfo
{
    QuestTemplateRow Row;
    std::vector<std::string> StartGoals;
    std::map<std::string, QuestGoal, std::less<>> Goals;
    std::map<std::string, QuestDialog, std::less<>> Dialogs;
    uint32 Starter = 0;
};

class QuestStore
{
public:
    static constexpr std::string_view OwnerQuest = "quest";
    static constexpr std::string_view OwnerGoal = "goal";
    static constexpr std::string_view KindAnd = "AND";
    static constexpr std::string_view KindOr = "OR";
    static constexpr std::string_view KindAdd = "ADD";
    static constexpr std::string_view TagStart = "Start";
    static constexpr std::string_view TagPrep = "Prep";

    QuestStore() = default;
    QuestStore(QuestStore const&) = delete;
    QuestStore& operator=(QuestStore const&) = delete;

    static std::shared_ptr<QuestStore const> Build(QuestRows const& rows, std::set<std::string, std::less<>> const& skipped = {});

    static std::optional<QuestGoalType> ParseGoalType(std::string_view text);
    static bool IsDialogTag(std::string_view tag);
    static bool IsLogicKind(std::string_view kind);

    QuestInfo const* Find(std::string_view name) const;
    std::size_t GetQuestCount() const noexcept { return _quests.size(); }
    std::size_t GetGoalCount() const noexcept { return _goals; }
    std::map<std::string, QuestInfo, std::less<>> const& GetAll() const noexcept { return _quests; }

    std::vector<std::string> GetQuestsOfferedBy(uint32 templateId) const;
    std::vector<QuestGoalRef> GetPersonaGoals(std::string_view objectName) const;
    std::vector<QuestGoalRef> GetUsageGoalsByTag(std::string_view tag) const;
    std::vector<QuestGoalRef> GetUsageGoalsByAdjective(std::string_view adjective) const;
    std::vector<QuestGoalRef> GetBountyGoals(std::string_view adjective) const;
    std::vector<QuestGoalRef> GetWaypointGoals(std::string_view zone) const;

private:
    using GoalIndex = std::map<std::string, std::vector<QuestGoalRef>, std::less<>>;

    static std::vector<QuestGoalRef> Lookup(GoalIndex const& index, std::string_view key);
    void Index();

    std::map<std::string, QuestInfo, std::less<>> _quests;
    std::size_t _goals = 0;
    std::unordered_map<uint32, std::vector<std::string>> _starterByTemplateId;
    GoalIndex _personaGoalsByObjectName;
    GoalIndex _usageGoalsByTag;
    GoalIndex _usageGoalsByAdjective;
    GoalIndex _bountyGoalsByAdjective;
    GoalIndex _waypointGoalsByZone;
};

#endif
