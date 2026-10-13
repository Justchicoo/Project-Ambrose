/*
 * Project Ambrose by Imjustchico
 * Reads a requirement object by property name: its type is its class name without the class prefix, and its m_applyNOT, quest, goal, registry entry, badge, required status, value, comparison, school, target, zone and gender are taken wherever its class has them; a RequirementList inside a list becomes a nested list, read no deeper than a fixed depth, its m_operator 1 being ROP_OR and any other ROP_AND as the type dump gives Requirement::Operator, and a requirement the decode could not name becomes a row of no known type, which the evaluator counts as unmet.
 */

#include "RequirementObjects.h"
#include "PropertyObject.h"

#include <cmath>
#include <cstddef>

namespace
{
    constexpr std::string_view ClassPrefix = "class ";
    constexpr std::size_t MaxDepth = 16;

    std::optional<int64> IntegerOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        if (!value)
            return std::nullopt;
        std::optional<int64> found;
        auto const take = [value, &found]<typename Stored>()
        {
            if (Stored const* const stored = value->GetIf<Stored>())
                found = static_cast<int64>(*stored);
        };
        take.template operator()<int8>();
        take.template operator()<uint8>();
        take.template operator()<int16>();
        take.template operator()<uint16>();
        take.template operator()<int32>();
        take.template operator()<uint32>();
        take.template operator()<int64>();
        take.template operator()<uint64>();
        return found;
    }

    std::string TextOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        std::string const* const text = value ? value->GetIf<std::string>() : nullptr;
        return text ? *text : std::string();
    }

    bool FlagOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        bool const* const flag = value ? value->GetIf<bool>() : nullptr;
        return flag && *flag;
    }

    std::optional<RequirementTree> TreeAt(PropertyObject const& list, std::size_t depth)
    {
        if (depth > MaxDepth)
            return std::nullopt;
        RequirementTree tree;
        tree.List.ApplyNot = FlagOf(list, "m_applyNOT");
        tree.List.Operator = RequirementObjects::OperatorName(IntegerOf(list, "m_operator").value_or(0));
        PropertyValue const* const held = list.Get("m_requirements");
        PropertyValue::List const* const entries = held ? held->GetList() : nullptr;
        if (!entries)
            return tree;
        for (PropertyValue const& entry : *entries)
        {
            PropertyObject const* const requirement = entry.AsObject();
            if (requirement && requirement->IsA(RequirementObjects::ListClass))
            {
                std::optional<RequirementTree> child = TreeAt(*requirement, depth + 1);
                if (child)
                    tree.Children.push_back(std::move(*child));
                else
                    tree.Requirements.push_back(RequirementObjects::RowOf(nullptr));
                continue;
            }
            tree.Requirements.push_back(RequirementObjects::RowOf(requirement));
        }
        return tree;
    }
}

std::string RequirementObjects::TypeOf(std::string_view className)
{
    if (className.starts_with(ClassPrefix))
        className.remove_prefix(ClassPrefix.size());
    return std::string(className);
}

std::string RequirementObjects::OperatorName(int64 listOperator)
{
    return listOperator == OperatorOr ? "OR" : "AND";
}

RequirementRow RequirementObjects::RowOf(PropertyObject const* requirement)
{
    RequirementRow row;
    if (!requirement)
    {
        row.Type = std::string(UnnamedRequirement);
        return row;
    }
    PropertyObject const& object = *requirement;
    row.Type = TypeOf(object.GetClass().Name);
    row.ApplyNot = FlagOf(object, "m_applyNOT");
    row.QuestName = TextOf(object, "m_questName");
    row.GoalName = TextOf(object, "m_goalName");
    row.EntryName = TextOf(object, "m_entryName");
    row.BadgeName = TextOf(object, "m_badgeName");
    row.IsQuestRegistry = FlagOf(object, "m_isQuestRegistry");
    if (std::optional<int64> const status = IntegerOf(object, "m_requiredStatus"); status && *status >= 0 && *status <= 255)
        row.RequiredStatus = static_cast<uint8>(*status);
    if (PropertyValue const* const value = object.Get("m_numericValue"))
    {
        if (float const* const single = value->GetIf<float>())
            row.NumericValue = double{ *single };
        else if (double const* const twice = value->GetIf<double>())
            row.NumericValue = *twice;
        if (row.NumericValue && !std::isfinite(*row.NumericValue))
            row.NumericValue.reset();
    }
    if (std::optional<int64> const comparison = IntegerOf(object, "m_operatorType"); comparison && *comparison >= 0)
        row.OperatorType = static_cast<uint32>(*comparison);
    row.MagicSchool = TextOf(object, "m_magicSchool");
    if (row.MagicSchool.empty())
        row.MagicSchool = TextOf(object, "m_magicSchoolName");
    if (std::optional<int64> const target = IntegerOf(object, "m_targetType"); target && *target >= 0 && *target <= 255)
        row.TargetType = static_cast<uint8>(*target);
    row.ZoneName = TextOf(object, "m_zoneName");
    row.Gender = TextOf(object, "m_playerGender");
    return row;
}

std::optional<RequirementTree> RequirementObjects::TreeOf(PropertyObject const* list)
{
    if (!list || !list->IsA(ListClass))
        return std::nullopt;
    return TreeAt(*list, 0);
}
