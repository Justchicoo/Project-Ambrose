/*
 * Project Ambrose by Imjustchico
 * Loads, validates and evaluates nested world requirement lists, and a flat list a caller builds itself, such as an item's equip requirements, retaining the serving generation on an invalid reload and treating unavailable facts and unrecognized types as false.
 */

#include "RequirementMgr.h"

#include "DatabaseEnv.h"
#include "Log.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace
{
    constexpr char const* RequirementLog = "server.conditions";

    std::string NormalizeType(std::string type)
    {
        constexpr std::string_view Prefix = "class ";
        if (type.starts_with(Prefix))
            type.erase(0, Prefix.size());
        return type;
    }

    template<typename Read>
    bool Select(std::string_view table, std::string_view sql, std::vector<std::string>& errors, Read read)
    {
        QueryResult result;
        if (!WorldDatabase.TryQuery(sql, result))
        {
            errors.push_back(std::string(table) + " could not be read");
            return false;
        }
        if (result)
        {
            do
                read(result->Fetch());
            while (result->NextRow());
        }
        return true;
    }

    void Combine(std::string_view listOperator, std::optional<bool>& result, std::optional<bool> value)
    {
        if (listOperator == "AND")
        {
            if ((result && !*result) || (value && !*value))
                result = false;
            else if (!result || !value)
                result = std::nullopt;
        }
        else
        {
            if ((result && *result) || (value && *value))
                result = true;
            else if (!result || !value)
                result = std::nullopt;
        }
    }

    std::optional<bool> Compare(double actual, std::optional<double> expected, std::optional<std::uint32_t> operatorType)
    {
        if (!expected || !operatorType)
            return std::nullopt;
        switch (*operatorType)
        {
            case 0: return actual == *expected;
            case 1: return actual > *expected;
            case 2: return actual < *expected;
            case 3: return actual >= *expected;
            case 4: return actual <= *expected;
            default: return std::nullopt;
        }
    }

    std::string ReadText(Field const& field)
    {
        return field.IsNull() ? std::string{} : field.Get<std::string>();
    }
}

RequirementMgr& RequirementMgr::Instance()
{
    static RequirementMgr instance;
    return instance;
}

std::optional<bool> RequirementContext::HasQuest(std::string_view) const
{
    return std::nullopt;
}

std::optional<RequirementGoalStatus> RequirementContext::GetGoalStatus(std::string_view, std::string_view) const
{
    return std::nullopt;
}

std::optional<bool> RequirementContext::HasRegistryEntry(std::string_view, std::string_view, bool) const
{
    return std::nullopt;
}

std::optional<double> RequirementContext::GetRegistryValue(std::string_view, std::string_view, bool) const
{
    return std::nullopt;
}

std::optional<double> RequirementContext::GetGlobalRegistryValue(std::string_view) const
{
    return std::nullopt;
}

std::optional<double> RequirementContext::GetMagicLevel(std::string_view) const
{
    return std::nullopt;
}

std::optional<bool> RequirementContext::HasSchoolOfFocus(std::string_view) const
{
    return std::nullopt;
}

std::optional<bool> RequirementContext::IsSchool(std::string_view, std::uint8_t) const
{
    return std::nullopt;
}

std::optional<bool> RequirementContext::IsInZone(std::string_view) const
{
    return std::nullopt;
}

std::optional<bool> RequirementContext::IsGender(std::string_view) const
{
    return std::nullopt;
}

std::optional<bool> RequirementContext::HasBadge(std::string_view) const
{
    return std::nullopt;
}

void RequirementMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); });
}

void RequirementMgr::SetRowSource(RowSource source)
{
    std::lock_guard const lock(_sourceMutex);
    _rowSource = std::move(source);
}

bool RequirementMgr::ReadWorldRows(RequirementRows& rows, std::vector<std::string>& errors)
{
    if (!WorldDatabase.IsOpen())
    {
        errors.push_back("the world database is not open, so requirement lists could not be read");
        return false;
    }

    bool ok = true;
    ok &= Select("requirement_list", "SELECT `id`, `operator`, `apply_not`, `parent` FROM `requirement_list` ORDER BY `id`", errors, [&rows](Field const* fields)
    {
        RequirementListRow row;
        row.Id = fields[0].Get<std::string>();
        row.Operator = fields[1].Get<std::string>();
        row.ApplyNot = fields[2].Get<bool>();
        if (!fields[3].IsNull())
            row.Parent = fields[3].Get<std::string>();
        rows.Lists.push_back(std::move(row));
    });
    ok &= Select("requirement", "SELECT `list_id`, `position`, `type`, `apply_not`, `quest_name`, `goal_name`, `entry_name`, `badge_name`, `is_quest_registry`, `required_status`, `numeric_value`, "
        "`operator_type`, `magic_school`, `target_type`, `zone`, `gender` FROM `requirement` ORDER BY `list_id`, `position`", errors, [&rows](Field const* fields)
    {
        RequirementRow row;
        row.ListId = fields[0].Get<std::string>();
        row.Position = fields[1].Get<std::uint32_t>();
        row.Type = NormalizeType(fields[2].Get<std::string>());
        row.ApplyNot = fields[3].Get<bool>();
        row.QuestName = ReadText(fields[4]);
        row.GoalName = ReadText(fields[5]);
        row.EntryName = ReadText(fields[6]);
        row.BadgeName = ReadText(fields[7]);
        row.IsQuestRegistry = fields[8].Get<bool>();
        if (!fields[9].IsNull())
            row.RequiredStatus = fields[9].Get<std::uint8_t>();
        if (!fields[10].IsNull())
            row.NumericValue = fields[10].Get<double>();
        if (!fields[11].IsNull())
            row.OperatorType = fields[11].Get<std::uint32_t>();
        row.MagicSchool = ReadText(fields[12]);
        if (!fields[13].IsNull())
            row.TargetType = fields[13].Get<std::uint8_t>();
        row.ZoneName = ReadText(fields[14]);
        row.Gender = ReadText(fields[15]);
        rows.Requirements.push_back(std::move(row));
    });
    return ok;
}

std::optional<RequirementStore> RequirementMgr::BuildStore(RequirementRows const& rows, std::vector<std::string>& errors)
{
    RequirementStore store;
    for (RequirementListRow const& row : rows.Lists)
    {
        if (row.Id.empty())
        {
            errors.push_back("requirement_list has an empty id");
            continue;
        }
        if (row.Operator != "AND" && row.Operator != "OR")
            errors.push_back("requirement_list " + row.Id + " has an operator other than AND or OR");
        if (!store.Lists.emplace(row.Id, RequirementListData{ row, {}, {} }).second)
            errors.push_back("requirement_list duplicates id " + row.Id);
    }

    for (auto& [id, list] : store.Lists)
    {
        (void)id;
        if (!list.Row.Parent)
            continue;
        auto parent = store.Lists.find(*list.Row.Parent);
        if (parent == store.Lists.end())
            errors.push_back("requirement_list " + list.Row.Id + " names missing parent " + *list.Row.Parent);
        else
            parent->second.Children.push_back(list.Row.Id);
    }

    std::map<std::string, std::set<std::uint32_t>, std::less<>> positions;
    for (RequirementRow row : rows.Requirements)
    {
        row.Type = NormalizeType(std::move(row.Type));
        auto list = store.Lists.find(row.ListId);
        if (list == store.Lists.end())
        {
            errors.push_back("requirement names missing list " + row.ListId);
            continue;
        }
        if (!positions[row.ListId].insert(row.Position).second)
            errors.push_back("requirement list " + row.ListId + " duplicates position " + std::to_string(row.Position));

        auto numeric = [&row, &errors](std::string_view type)
        {
            if (!row.NumericValue || !std::isfinite(*row.NumericValue) || !row.OperatorType || *row.OperatorType > 4)
                errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (" + std::string(type) + ") has invalid numeric comparison data");
        };

        if (row.Type == "ReqHasQuest" && row.QuestName.empty())
            errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqHasQuest) has no quest_name");
        else if (row.Type == "ReqHasGoal" && (row.QuestName.empty() || row.GoalName.empty() || !row.RequiredStatus || *row.RequiredStatus > 2))
            errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqHasGoal) has invalid goal data");
        else if (row.Type == "ReqHasEntry" && (row.EntryName.empty() || (row.IsQuestRegistry && row.QuestName.empty())))
            errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqHasEntry) has invalid registry data");
        else if (row.Type == "ReqEntryValue")
        {
            if (row.EntryName.empty() || (row.IsQuestRegistry && row.QuestName.empty()))
                errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqEntryValue) has invalid registry data");
            numeric("ReqEntryValue");
        }
        else if (row.Type == "ReqGlobalRegistryValue")
        {
            if (row.EntryName.empty())
                errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqGlobalRegistryValue) has no entry_name");
            numeric("ReqGlobalRegistryValue");
        }
        else if (row.Type == "ReqMagicLevel")
        {
            if (row.MagicSchool.empty())
                errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqMagicLevel) has no magic_school");
            numeric("ReqMagicLevel");
        }
        else if (row.Type == "ReqSchoolOfFocus" && row.MagicSchool.empty())
            errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqSchoolOfFocus) has no magic_school");
        else if (row.Type == "ReqIsSchool" && (row.MagicSchool.empty() || !row.TargetType || *row.TargetType > 1))
            errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqIsSchool) has invalid school data");
        else if (row.Type == "ReqInZone" && row.ZoneName.empty())
            errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqInZone) has no zone");
        else if (row.Type == "ReqIsGender" && row.Gender.empty())
            errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqIsGender) has no gender");
        else if (row.Type == "ReqHasBadge" && row.BadgeName.empty())
            errors.push_back("requirement " + row.ListId + ":" + std::to_string(row.Position) + " (ReqHasBadge) has no badge_name");

        list->second.Requirements.push_back(std::move(row));
    }

    for (auto& [id, list] : store.Lists)
    {
        std::sort(list.Requirements.begin(), list.Requirements.end(), [](RequirementRow const& left, RequirementRow const& right)
        {
            return left.Position < right.Position;
        });
        std::sort(list.Children.begin(), list.Children.end());
        if (list.Requirements.empty() && list.Children.empty())
            errors.push_back("requirement_list " + id + " is empty");
    }

    enum class Visit : std::uint8_t
    {
        Visiting,
        Complete
    };
    std::map<std::string, Visit, std::less<>> visits;
    std::function<void(std::string const&)> visit = [&](std::string const& id)
    {
        auto state = visits.find(id);
        if (state != visits.end())
        {
            if (state->second == Visit::Visiting)
                errors.push_back("requirement_list nesting contains a cycle at " + id);
            return;
        }
        visits.emplace(id, Visit::Visiting);
        for (std::string const& child : store.Lists.at(id).Children)
            visit(child);
        visits[id] = Visit::Complete;
    };
    for (auto const& [id, list] : store.Lists)
    {
        (void)list;
        visit(id);
    }

    if (!errors.empty())
        return std::nullopt;
    return store;
}

bool RequirementMgr::Load(std::vector<std::string>& errors)
{
    RequirementRows rows;
    RowSource source;
    {
        std::lock_guard const lock(_sourceMutex);
        source = _rowSource;
    }
    if (source)
        rows = source();
    else if (!ReadWorldRows(rows, errors))
        return false;

    std::optional<RequirementStore> next = BuildStore(rows, errors);
    if (!next)
        return false;
    _requirements.Replace(std::move(*next));
    LOG_INFO(RequirementLog, "Loaded {} requirement list(s)", _requirements.Get()->Lists.size());
    return true;
}

bool RequirementMgr::Evaluate(std::string_view listId, RequirementContext const& context) const
{
    auto const store = _requirements.Get();
    if (!store->Lists.contains(listId))
    {
        std::string const missing = "missing-list:" + std::string(listId);
        {
            std::lock_guard const lock(_unknownTypeMutex);
            if (!_warnedTypes.insert(missing).second)
                return false;
        }
        LOG_WARN(RequirementLog, "Unknown requirement list {} evaluates false", listId);
        return false;
    }
    std::set<std::string, std::less<>> activeLists;
    return EvaluateList(*store, listId, context, activeLists).value_or(false);
}

bool RequirementMgr::EvaluateRequirements(RequirementListRow const& list, std::vector<RequirementRow> const& requirements, RequirementContext const& context) const
{
    std::optional<bool> result = list.Operator == "AND";
    for (RequirementRow const& row : requirements)
    {
        std::optional<bool> value = EvaluateRequirement(row, context);
        if (value && row.ApplyNot)
            value = !*value;
        Combine(list.Operator, result, value);
    }
    if (result && list.ApplyNot)
        result = !*result;
    return result.value_or(false);
}

bool RequirementMgr::Evaluate(PropertyObject const& list, RequirementContext const& context) const
{
    std::set<PropertyObject const*> activeLists;
    return EvaluateObjectList(list, context, activeLists).value_or(false);
}

std::optional<bool> RequirementMgr::EvaluateObjectList(
    PropertyObject const& object,
    RequirementContext const& context,
    std::set<PropertyObject const*>& activeLists) const
{
    std::optional<RequirementListView> const list = RequirementListView::From(object);
    if (!list || !activeLists.insert(&object).second)
        return std::nullopt;

    int64 const operation = list->GetOperator();
    if (operation != 0 && operation != 1)
    {
        activeLists.erase(&object);
        return std::nullopt;
    }
    bool const isAnd = operation == 0;
    std::optional<bool> result = isAnd;
    auto combine = [&result, isAnd](std::optional<bool> value)
    {
        if (isAnd)
        {
            if ((result && !*result) || (value && !*value))
                result = false;
            else if (!result || !value)
                result = std::nullopt;
        }
        else
        {
            if ((result && *result) || (value && *value))
                result = true;
            else if (!result || !value)
                result = std::nullopt;
        }
    };

    std::uint32_t position = 0;
    for (PropertyValue const& value : list->GetRequirements())
    {
        PropertyObject const* const requirement = value.AsObject();
        if (!requirement)
        {
            combine(std::nullopt);
            ++position;
            continue;
        }
        if (requirement->IsA(RequirementListView::Definition.ClassName))
        {
            combine(EvaluateObjectList(*requirement, context, activeLists));
            ++position;
            continue;
        }

        std::optional<RequirementRow> row = DecodeObjectRequirement(*requirement, "embedded", position++);
        if (!row)
        {
            combine(std::nullopt);
            continue;
        }
        std::optional<bool> valueResult = EvaluateRequirement(*row, context);
        if (valueResult && row->ApplyNot)
            valueResult = !*valueResult;
        combine(valueResult);
    }

    activeLists.erase(&object);
    if (result && list->AppliesNot())
        result = !*result;
    return result;
}

std::optional<RequirementRow> RequirementMgr::DecodeObjectRequirement(
    PropertyObject const& requirement,
    std::string const& listId,
    std::uint32_t position) const
{
    RequirementRow row;
    row.ListId = listId;
    row.Position = position;
    row.Type = requirement.GetClass().Name;
    constexpr std::string_view ClassPrefix = "class ";
    if (row.Type.starts_with(ClassPrefix))
        row.Type.erase(0, ClassPrefix.size());

    auto const text = [&requirement](std::string_view name) -> std::optional<std::string>
    {
        PropertyValue const* const value = requirement.Get(name);
        std::string const* const result = value ? value->GetIf<std::string>() : nullptr;
        return result ? std::optional<std::string>(*result) : std::nullopt;
    };
    auto const number = [&requirement](std::string_view name) -> std::optional<double>
    {
        PropertyValue const* const value = requirement.Get(name);
        if (value)
        {
            if (float const* const result = value->GetIf<float>())
                return *result;
            if (double const* const result = value->GetIf<double>())
                return *result;
        }
        return std::nullopt;
    };
    auto const boolean = [&requirement](std::string_view name) -> std::optional<bool>
    {
        PropertyValue const* const value = requirement.Get(name);
        bool const* const result = value ? value->GetIf<bool>() : nullptr;
        return result ? std::optional<bool>(*result) : std::nullopt;
    };
    auto const integer = [&requirement](std::string_view name) -> std::optional<std::uint32_t>
    {
        PropertyValue const* const value = requirement.Get(name);
        if (!value)
            return std::nullopt;
        if (uint32 const* const result = value->GetIf<uint32>())
            return *result;
        if (int32 const* const result = value->GetIf<int32>(); result && *result >= 0)
            return static_cast<std::uint32_t>(*result);
        if (int64 const* const result = value->GetIf<int64>(); result && *result >= 0 && static_cast<std::uint64_t>(*result) <= std::numeric_limits<std::uint32_t>::max())
            return static_cast<std::uint32_t>(*result);
        return std::nullopt;
    };
    auto const fillText = [&row, &text](std::string_view property, std::string& destination)
    {
        std::optional<std::string> value = text(property);
        if (!value)
            return false;
        destination = std::move(*value);
        return true;
    };

    std::optional<bool> const applyNot = boolean("m_applyNOT");
    if (!applyNot)
        return std::nullopt;
    row.ApplyNot = *applyNot;
    if (row.Type == "ReqHasQuest")
    {
        if (!fillText("m_questName", row.QuestName))
            return std::nullopt;
    }
    else if (row.Type == "ReqHasGoal")
    {
        if (!fillText("m_questName", row.QuestName) || !fillText("m_goalName", row.GoalName))
            return std::nullopt;
        std::optional<std::uint32_t> const status = integer("m_requiredStatus");
        if (!status || *status > 2)
            return std::nullopt;
        row.RequiredStatus = static_cast<std::uint8_t>(*status);
    }
    else if (row.Type == "ReqHasEntry" || row.Type == "ReqEntryValue")
    {
        if (!fillText("m_entryName", row.EntryName) || !fillText("m_questName", row.QuestName))
            return std::nullopt;
        std::optional<bool> const isQuestRegistry = boolean("m_isQuestRegistry");
        if (!isQuestRegistry)
            return std::nullopt;
        row.IsQuestRegistry = *isQuestRegistry;
    }
    else if (row.Type == "ReqGlobalRegistryValue")
    {
        if (!fillText("m_entryName", row.EntryName))
            return std::nullopt;
    }
    else if (row.Type == "ReqMagicLevel")
    {
        if (!fillText("m_magicSchool", row.MagicSchool))
            return std::nullopt;
    }
    else if (row.Type == "ReqSchoolOfFocus")
    {
        if (!fillText("m_magicSchool", row.MagicSchool))
            return std::nullopt;
    }
    else if (row.Type == "ReqIsSchool")
    {
        if (!fillText("m_magicSchoolName", row.MagicSchool))
            return std::nullopt;
        std::optional<std::uint32_t> const target = integer("m_targetType");
        if (!target || *target > 1)
            return std::nullopt;
        row.TargetType = static_cast<std::uint8_t>(*target);
    }
    else if (row.Type == "ReqInZone")
    {
        if (!fillText("m_zoneName", row.ZoneName))
            return std::nullopt;
    }
    else if (row.Type == "ReqIsGender")
    {
        if (!fillText("m_playerGender", row.Gender))
            return std::nullopt;
    }
    else if (row.Type == "ReqHasBadge")
    {
        if (!fillText("m_badgeName", row.BadgeName))
            return std::nullopt;
    }
    else
        return std::nullopt;

    if (row.Type == "ReqEntryValue" || row.Type == "ReqGlobalRegistryValue" || row.Type == "ReqMagicLevel")
    {
        row.NumericValue = number("m_numericValue");
        row.OperatorType = integer("m_operatorType");
        if (!row.NumericValue || !row.OperatorType || *row.OperatorType > 4)
            return std::nullopt;
    }
    return row;
}

std::optional<bool> RequirementMgr::EvaluateList(
    RequirementStore const& store,
    std::string_view listId,
    RequirementContext const& context,
    std::set<std::string, std::less<>>& activeLists) const
{
    auto const list = store.Lists.find(listId);
    if (list == store.Lists.end() || !activeLists.insert(list->first).second)
        return std::nullopt;

    std::optional<bool> result = list->second.Row.Operator == "AND";
    auto combine = [&result, &list](std::optional<bool> value) { Combine(list->second.Row.Operator, result, value); };

    for (RequirementRow const& row : list->second.Requirements)
    {
        std::optional<bool> value = EvaluateRequirement(row, context);
        if (value && row.ApplyNot)
            value = !*value;
        combine(value);
    }
    for (std::string const& child : list->second.Children)
        combine(EvaluateList(store, child, context, activeLists));

    activeLists.erase(list->first);
    if (result && list->second.Row.ApplyNot)
        result = !*result;
    return result;
}

std::optional<bool> RequirementMgr::EvaluateRequirement(RequirementRow const& row, RequirementContext const& context) const
{
    std::optional<bool> result;
    if (row.Type == "ReqHasQuest")
        result = context.HasQuest(row.QuestName);
    else if (row.Type == "ReqHasGoal")
    {
        std::optional<RequirementGoalStatus> const status = context.GetGoalStatus(row.QuestName, row.GoalName);
        if (status && row.RequiredStatus)
        {
            if (*row.RequiredStatus == static_cast<std::uint8_t>(RequirementGoalStatus::DontCare))
                result = true;
            else
                result = static_cast<std::uint8_t>(*status) == *row.RequiredStatus;
        }
    }
    else if (row.Type == "ReqHasEntry")
        result = context.HasRegistryEntry(row.QuestName, row.EntryName, row.IsQuestRegistry);
    else if (row.Type == "ReqEntryValue")
    {
        std::optional<double> const value = context.GetRegistryValue(row.QuestName, row.EntryName, row.IsQuestRegistry);
        if (value)
            result = Compare(*value, row.NumericValue, row.OperatorType);
    }
    else if (row.Type == "ReqGlobalRegistryValue")
    {
        std::optional<double> const value = context.GetGlobalRegistryValue(row.EntryName);
        if (value)
            result = Compare(*value, row.NumericValue, row.OperatorType);
    }
    else if (row.Type == "ReqMagicLevel")
    {
        std::optional<double> const level = context.GetMagicLevel(row.MagicSchool);
        if (level)
            result = Compare(*level, row.NumericValue, row.OperatorType);
    }
    else if (row.Type == "ReqSchoolOfFocus")
        result = context.HasSchoolOfFocus(row.MagicSchool);
    else if (row.Type == "ReqIsSchool")
    {
        if (row.TargetType)
            result = context.IsSchool(row.MagicSchool, *row.TargetType);
    }
    else if (row.Type == "ReqInZone")
        result = context.IsInZone(row.ZoneName);
    else if (row.Type == "ReqIsGender")
        result = context.IsGender(row.Gender);
    else if (row.Type == "ReqHasBadge")
        result = context.HasBadge(row.BadgeName);
    else
    {
        result = sScriptMgr.EvaluateCondition(row, context);
        if (!result)
            LogUnknownTypeOnce(row.Type);
    }
    return result;
}

void RequirementMgr::LogUnknownTypeOnce(std::string_view type) const
{
    std::string const name(type);
    {
        std::lock_guard const lock(_unknownTypeMutex);
        if (!_warnedTypes.insert(name).second)
            return;
    }
    LOG_WARN(RequirementLog, "Unknown requirement type {} evaluates false unless a ConditionScript handles it", type);
}

std::uint64_t RequirementMgr::GetGeneration() const
{
    return _requirements.GetGeneration();
}

void RequirementMgr::Clear()
{
    _requirements.Replace(RequirementStore{});
    {
        std::lock_guard const lock(_sourceMutex);
        _rowSource = nullptr;
    }
    {
        std::lock_guard const lock(_unknownTypeMutex);
        _warnedTypes.clear();
    }
}
