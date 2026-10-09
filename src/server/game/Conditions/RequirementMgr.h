/*
 * Project Ambrose by Imjustchico
 * The requirement rows, the facts a wizard can answer, and the reloadable requirement manager that serves complete nested lists from one immutable generation.
 */

#ifndef AMBROSE_REQUIREMENTMGR_H
#define AMBROSE_REQUIREMENTMGR_H

#include "ReloadableStore.h"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

enum class RequirementGoalStatus : std::uint8_t
{
    DontCare = 0,
    Complete = 1,
    Incomplete = 2
};

struct RequirementListRow
{
    std::string Id;
    std::string Operator = "AND";
    bool ApplyNot = false;
    std::optional<std::string> Parent;
};

struct RequirementRow
{
    std::string ListId;
    std::uint32_t Position = 0;
    std::string Type;
    bool ApplyNot = false;
    std::string QuestName;
    std::string GoalName;
    std::string EntryName;
    std::string BadgeName;
    bool IsQuestRegistry = false;
    std::optional<std::uint8_t> RequiredStatus;
    std::optional<double> NumericValue;
    std::optional<std::uint32_t> OperatorType;
    std::string MagicSchool;
    std::optional<std::uint8_t> TargetType;
    std::string ZoneName;
    std::string Gender;
};

struct RequirementRows
{
    std::vector<RequirementListRow> Lists;
    std::vector<RequirementRow> Requirements;
};

struct RequirementListData
{
    RequirementListRow Row;
    std::vector<RequirementRow> Requirements;
    std::vector<std::string> Children;
};

struct RequirementStore
{
    std::map<std::string, RequirementListData, std::less<>> Lists;
};

class RequirementContext
{
public:
    virtual ~RequirementContext() = default;

    virtual std::optional<bool> HasQuest(std::string_view questName) const;
    virtual std::optional<RequirementGoalStatus> GetGoalStatus(
        std::string_view questName,
        std::string_view goalName) const;
    virtual std::optional<bool> HasRegistryEntry(
        std::string_view questName,
        std::string_view entryName,
        bool isQuestRegistry) const;
    virtual std::optional<double> GetRegistryValue(
        std::string_view questName,
        std::string_view entryName,
        bool isQuestRegistry) const;
    virtual std::optional<double> GetGlobalRegistryValue(std::string_view entryName) const;
    virtual std::optional<double> GetMagicLevel(std::string_view school) const;
    virtual std::optional<bool> HasSchoolOfFocus(std::string_view school) const;
    virtual std::optional<bool> IsSchool(std::string_view school, std::uint8_t targetType) const;
    virtual std::optional<bool> IsInZone(std::string_view zoneName) const;
    virtual std::optional<bool> IsGender(std::string_view gender) const;
    virtual std::optional<bool> HasBadge(std::string_view badgeName) const;
};

class RequirementMgr
{
public:
    static constexpr std::string_view Target = "requirement";
    using RowSource = std::function<RequirementRows()>;

    static RequirementMgr& Instance();

    RequirementMgr() = default;
    RequirementMgr(RequirementMgr const&) = delete;
    RequirementMgr& operator=(RequirementMgr const&) = delete;

    void RegisterReloadTargets();
    void SetRowSource(RowSource source);
    bool Load(std::vector<std::string>& errors);
    bool Evaluate(std::string_view listId, RequirementContext const& context) const;
    std::uint64_t GetGeneration() const;
    void Clear();

private:
    static bool ReadWorldRows(RequirementRows& rows, std::vector<std::string>& errors);
    static std::optional<RequirementStore> BuildStore(
        RequirementRows const& rows,
        std::vector<std::string>& errors);
    std::optional<bool> EvaluateList(
        RequirementStore const& store,
        std::string_view listId,
        RequirementContext const& context,
        std::set<std::string, std::less<>>& activeLists) const;
    std::optional<bool> EvaluateRequirement(
        RequirementRow const& row,
        RequirementContext const& context) const;
    void LogUnknownTypeOnce(std::string_view type) const;

    ReloadableStore<RequirementStore> _requirements;
    mutable std::mutex _sourceMutex;
    RowSource _rowSource;
    mutable std::mutex _unknownTypeMutex;
    mutable std::set<std::string, std::less<>> _warnedTypes;
};

#define sRequirementMgr RequirementMgr::Instance()

#endif
