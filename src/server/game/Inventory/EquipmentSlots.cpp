/*
 * Project Ambrose by Imjustchico
 * Reads an EquipmentTemplate's m_baseSlots by name, each EquipSlot's name, category, item count and AND, OR and NOT adjective lists, matched against an item's adjectives whatever their case, and its m_slotRequirements list, read as rows the way every requirement object is.
 */

#include "EquipmentSlots.h"
#include "PropertyObject.h"
#include "RequirementObjects.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace
{
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
            rules.List.Operator = RequirementObjects::OperatorName(IntegerOf(*requirementList, "m_operator").value_or(0));
            PropertyValue const* const rows = requirementList->Get("m_requirements");
            if (PropertyValue::List const* const entries = rows ? rows->GetList() : nullptr)
                for (PropertyValue const& requirement : *entries)
                    rules.Requirements.push_back(RequirementObjects::RowOf(requirement.AsObject()));
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
    rules.List.Operator = RequirementObjects::OperatorName(list.Operator);
    for (ItemTemplatePart const& part : list.Requirements)
    {
        RequirementRow row = RequirementObjects::RowOf(part.Object.get());
        if (!part.Object)
            row.Type = RequirementObjects::TypeOf(part.ClassName);
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
