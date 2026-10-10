/*
 * Project Ambrose by Imjustchico
 * The slots a wizard wears items in, read from the equipment template its player template names: each slot's name, category and how many items it holds, the adjectives an item must all have, the ones it must have one of and the ones it must not have, and the requirement list a slot sets, as rows the requirement evaluator reads; and the same rows made from an item's equip requirements, so a slot and an item are judged by one evaluator.
 */

#ifndef AMBROSE_EQUIPMENTSLOTS_H
#define AMBROSE_EQUIPMENTSLOTS_H

#include "ItemTemplateRecord.h"
#include "RequirementMgr.h"
#include "Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PropertyObject;

struct EquipRequirements
{
    RequirementListRow List;
    std::vector<RequirementRow> Requirements;
};

struct EquipSlot
{
    std::string Name;
    std::string Category;
    uint32 MaxItems = 1;
    std::vector<std::string> AdjectivesAnd;
    std::vector<std::string> AdjectivesOr;
    std::vector<std::string> AdjectivesNot;
    std::optional<EquipRequirements> Requirements;

    bool Accepts(std::vector<std::string> const& adjectives) const;
};

class EquipmentSlots
{
public:
    static constexpr std::string_view SlotsProperty = "m_baseSlots";

    static std::optional<EquipmentSlots> Read(PropertyObject const& equipmentTemplate, std::string& problem);
    static EquipmentSlots FromSlots(std::vector<EquipSlot> slots);
    static std::optional<EquipRequirements> RequirementsOf(ItemRequirementList const& list);

    EquipSlot const* Find(std::string_view name) const noexcept;
    std::vector<EquipSlot> const& GetSlots() const noexcept { return _slots; }

private:
    std::vector<EquipSlot> _slots;
};

#endif
