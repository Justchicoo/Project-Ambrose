/*
 * Project Ambrose by Imjustchico
 * Reads an EquipmentTemplate's m_baseSlots by name, each EquipSlot's name, category, item count and AND, OR and NOT adjective lists, matched against an item's adjectives whatever their case, and its m_slotRequirements list, where a requirement the decode could not name becomes a row of no known type, which the evaluator counts as unmet; a requirement's type is its class name without the class prefix, its m_applyNOT, value, comparison and school are read where its class has them, and a list's operator 1 is ROP_OR and any other ROP_AND, as the type dump gives Requirement::Operator.
 */

#include "EquipmentSlots.h"
#include "PropertyObject.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace
{
    constexpr std::string_view ClassPrefix = "class ";
    constexpr std::string_view UnnamedRequirement = "unnamed requirement";
    constexpr int64 OperatorOr = 1;

    std::string TypeOf(std::string_view className)
    {
        if (className.starts_with(ClassPrefix))
            className.remove_prefix(ClassPrefix.size());
        return std::string(className);
    }

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

    std::vector<std::string> TextsOf(PropertyObject const& object, std::string_view name)
    {
        std::vector<std::string> texts;
        PropertyValue const* const value = object.Get(name);
        PropertyValue::List const* const list = value ? value->GetList() : nullptr;
        if (!list)
            return texts;
        for (PropertyValue const& entry : *list)
            if (std::string const* const text = entry.GetIf<std::string>())
                texts.push_back(*text);
        return texts;
    }

    bool Holds(std::vector<std::string> const& adjectives, std::string_view wanted)
    {
        return std::any_of(adjectives.begin(), adjectives.end(), [wanted](std::string const& adjective) { return Ambrose::EqualsIgnoreCase(adjective, wanted); });
    }

    RequirementRow RowOf(PropertyObject const* requirement)
    {
        RequirementRow row;
        if (!requirement)
        {
            row.Type = std::string(UnnamedRequirement);
            return row;
        }
        row.Type = TypeOf(requirement->GetClass().Name);
        row.ApplyNot = FlagOf(*requirement, "m_applyNOT");
        row.MagicSchool = TextOf(*requirement, "m_magicSchool");
        if (PropertyValue const* const value = requirement->Get("m_numericValue"))
        {
            if (float const* const single = value->GetIf<float>())
                row.NumericValue = double{ *single };
            else if (double const* const twice = value->GetIf<double>())
                row.NumericValue = *twice;
        }
        if (std::optional<int64> const comparison = IntegerOf(*requirement, "m_operatorType"); comparison && *comparison >= 0)
            row.OperatorType = static_cast<uint32>(*comparison);
        return row;
    }

    std::string OperatorName(int64 listOperator)
    {
        return listOperator == OperatorOr ? "OR" : "AND";
    }
}

bool EquipSlot::Accepts(std::vector<std::string> const& adjectives) const
{
    for (std::string const& wanted : AdjectivesAnd)
        if (!Holds(adjectives, wanted))
            return false;
    if (!AdjectivesOr.empty() && std::none_of(AdjectivesOr.begin(), AdjectivesOr.end(), [&adjectives](std::string const& wanted) { return Holds(adjectives, wanted); }))
        return false;
    return std::none_of(AdjectivesNot.begin(), AdjectivesNot.end(), [&adjectives](std::string const& refused) { return Holds(adjectives, refused); });
}

std::optional<EquipmentSlots> EquipmentSlots::Read(PropertyObject const& equipmentTemplate, std::string& problem)
{
    problem.clear();
    PropertyValue const* const held = equipmentTemplate.Get(SlotsProperty);
    PropertyValue::List const* const list = held ? held->GetList() : nullptr;
    if (!list)
    {
        problem = fmt::format("{} has no {}", equipmentTemplate.GetClass().Name, SlotsProperty);
        return std::nullopt;
    }
    std::vector<EquipSlot> slots;
    slots.reserve(list->size());
    for (PropertyValue const& entry : *list)
    {
        PropertyObject const* const object = entry.AsObject();
        if (!object)
            continue;
        EquipSlot slot;
        slot.Name = TextOf(*object, "m_slotName");
        if (slot.Name.empty())
        {
            problem = fmt::format("slot {} of {} has no name", slots.size(), equipmentTemplate.GetClass().Name);
            return std::nullopt;
        }
        slot.Category = TextOf(*object, "m_slotCategory");
        slot.MaxItems = static_cast<uint32>(std::clamp<int64>(IntegerOf(*object, "m_maxItemCount").value_or(1), 1, 255));
        slot.AdjectivesAnd = TextsOf(*object, "m_adjectivesAND");
        slot.AdjectivesOr = TextsOf(*object, "m_adjectivesOR");
        slot.AdjectivesNot = TextsOf(*object, "m_adjectivesNOT");
        PropertyValue const* const requirements = object->Get("m_slotRequirements");
        if (PropertyObject const* const requirementList = requirements ? requirements->AsObject() : nullptr)
        {
            EquipRequirements rules;
            rules.List.ApplyNot = FlagOf(*requirementList, "m_applyNOT");
            rules.List.Operator = OperatorName(IntegerOf(*requirementList, "m_operator").value_or(0));
            PropertyValue const* const rows = requirementList->Get("m_requirements");
            if (PropertyValue::List const* const entries = rows ? rows->GetList() : nullptr)
                for (PropertyValue const& requirement : *entries)
                    rules.Requirements.push_back(RowOf(requirement.AsObject()));
            if (!rules.Requirements.empty())
                slot.Requirements = std::move(rules);
        }
        slots.push_back(std::move(slot));
    }
    return FromSlots(std::move(slots));
}

EquipmentSlots EquipmentSlots::FromSlots(std::vector<EquipSlot> slots)
{
    EquipmentSlots found;
    found._slots = std::move(slots);
    return found;
}

std::optional<EquipRequirements> EquipmentSlots::RequirementsOf(ItemRequirementList const& list)
{
    if (list.Requirements.empty())
        return std::nullopt;
    EquipRequirements rules;
    rules.List.ApplyNot = list.ApplyNot;
    rules.List.Operator = OperatorName(list.Operator);
    for (ItemTemplatePart const& part : list.Requirements)
    {
        RequirementRow row = RowOf(part.Object.get());
        if (!part.Object)
            row.Type = TypeOf(part.ClassName);
        if (part.MagicSchool && row.MagicSchool.empty())
            row.MagicSchool = *part.MagicSchool;
        if (part.NumericValue && !row.NumericValue)
            row.NumericValue = part.NumericValue;
        if (part.OperatorType && !row.OperatorType && *part.OperatorType >= 0)
            row.OperatorType = static_cast<uint32>(*part.OperatorType);
        rules.Requirements.push_back(std::move(row));
    }
    return rules;
}

EquipSlot const* EquipmentSlots::Find(std::string_view name) const noexcept
{
    auto const found = std::find_if(_slots.begin(), _slots.end(), [name](EquipSlot const& slot) { return Ambrose::EqualsIgnoreCase(slot.Name, name); });
    return found == _slots.end() ? nullptr : &*found;
}
