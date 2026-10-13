/*
 * Project Ambrose by Imjustchico
 * Keeps a wizard's worn items in the order it put them on and equips and unequips by the rules its header gives, checking ownership, the slot, the item's template, the slot's adjectives, then the slot's and the item's requirements through the requirement evaluator, and moving nothing until every check has passed; a wizard's magic level is its level whatever school a requirement names, since ReqMagicLevel's school is not sent, and its school of focus is matched by name whatever its case.
 */

#include "PlayerEquipment.h"
#include "StringUtil.h"

#include <algorithm>
#include <utility>

std::optional<double> EquipWizard::GetMagicLevel(std::string_view) const
{
    return static_cast<double>(_level);
}

std::optional<bool> EquipWizard::HasSchoolOfFocus(std::string_view school) const
{
    return Ambrose::EqualsIgnoreCase(school, _school);
}

PlayerEquipment PlayerEquipment::FromStored(std::vector<CharacterEquippedItem> stored)
{
    PlayerEquipment equipment;
    equipment._items = std::move(stored);
    return equipment;
}

CharacterEquippedItem const* PlayerEquipment::Find(uint64 itemGuid) const noexcept
{
    std::optional<std::size_t> const index = IndexOf(itemGuid);
    return index ? &_items[*index] : nullptr;
}

std::optional<std::size_t> PlayerEquipment::IndexOf(uint64 itemGuid) const noexcept
{
    auto const found = std::find_if(_items.begin(), _items.end(), [itemGuid](CharacterEquippedItem const& worn) { return worn.Item.Guid == itemGuid; });
    if (found == _items.end())
        return std::nullopt;
    return static_cast<std::size_t>(found - _items.begin());
}

std::size_t PlayerEquipment::CountIn(std::string_view slot) const noexcept
{
    return static_cast<std::size_t>(std::count_if(_items.begin(), _items.end(), [slot](CharacterEquippedItem const& worn) { return Ambrose::EqualsIgnoreCase(worn.Slot, slot); }));
}

EquipChange PlayerEquipment::Equip(PlayerBackpack& backpack, uint32 capacity, EquipmentSlots const& slots, ItemTemplateStore const& templates, RequirementContext const& wizard,
    uint64 itemGuid, std::string_view slotName)
{
    EquipChange change;
    CharacterItem const* const held = backpack.Find(itemGuid);
    if (!held)
        return change;
    EquipSlot const* const slot = slots.Find(slotName);
    if (!slot)
    {
        change.Result = EquipResult::NoSuchSlot;
        return change;
    }
    ItemTemplateRecord const* const itemTemplate = templates.Find(held->TemplateId);
    if (!itemTemplate)
    {
        change.Result = EquipResult::NoSuchTemplate;
        return change;
    }
    if (!slot->Accepts(itemTemplate->Adjectives))
    {
        change.Result = EquipResult::WrongSlot;
        return change;
    }
    if (slot->Requirements && !sRequirementMgr.EvaluateRequirements(slot->Requirements->List, slot->Requirements->Requirements, wizard))
    {
        change.Result = EquipResult::SlotRequirementsNotMet;
        return change;
    }
    if (itemTemplate->EquipRequirements)
    {
        std::optional<EquipRequirements> const rules = EquipmentSlots::RequirementsOf(*itemTemplate->EquipRequirements);
        if (rules && !sRequirementMgr.EvaluateRequirements(rules->List, rules->Requirements, wizard))
        {
            change.Result = EquipResult::RequirementsNotMet;
            return change;
        }
    }
    std::optional<std::size_t> returning;
    if (CountIn(slot->Name) >= slot->MaxItems)
    {
        auto const oldest = std::find_if(_items.begin(), _items.end(), [slot](CharacterEquippedItem const& worn) { return Ambrose::EqualsIgnoreCase(worn.Slot, slot->Name); });
        returning = static_cast<std::size_t>(oldest - _items.begin());
        if (backpack.Size() > capacity)
        {
            change.Result = EquipResult::BackpackFull;
            return change;
        }
    }

    std::optional<CharacterItem> moved = backpack.Remove(itemGuid);
    if (returning)
    {
        CharacterEquippedItem const old = _items[*returning];
        _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(*returning));
        change.Returned = backpack.Put(old.Item);
        change.ReturnedIndex = *returning;
    }
    CharacterEquippedItem worn{ std::move(*moved), slot->Name };
    _items.push_back(worn);
    change.Worn = std::move(worn);
    change.Result = EquipResult::Equipped;
    return change;
}

UnequipChange PlayerEquipment::Unequip(PlayerBackpack& backpack, uint32 capacity, uint64 itemGuid)
{
    UnequipChange change;
    std::optional<std::size_t> const index = IndexOf(itemGuid);
    if (!index)
        return change;
    if (backpack.Size() >= capacity)
    {
        change.Result = UnequipResult::BackpackFull;
        return change;
    }
    CharacterEquippedItem const worn = _items[*index];
    _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(*index));
    change.TakenOff = worn;
    change.Returned = backpack.Put(worn.Item);
    change.Index = *index;
    change.Result = UnequipResult::Unequipped;
    return change;
}

std::string_view PlayerEquipment::ResultName(EquipResult result) noexcept
{
    switch (result)
    {
        case EquipResult::Equipped:
            return "equipped";
        case EquipResult::NotOwned:
            return "it does not hold that item in its backpack";
        case EquipResult::NoSuchSlot:
            return "its equipment has no slot of that name";
        case EquipResult::NoSuchTemplate:
            return "the items this server holds do not name the item's template";
        case EquipResult::WrongSlot:
            return "the item's adjectives do not fit that slot";
        case EquipResult::SlotRequirementsNotMet:
            return "the wizard does not meet the slot's requirements";
        case EquipResult::RequirementsNotMet:
            return "the wizard does not meet the item's equip requirements";
        case EquipResult::BackpackFull:
            return "its backpack has no room for the item the slot gives back";
        default:
            return "it is not in the world";
    }
}

std::string_view PlayerEquipment::ResultName(UnequipResult result) noexcept
{
    switch (result)
    {
        case UnequipResult::Unequipped:
            return "unequipped";
        case UnequipResult::NotWorn:
            return "it does not wear that item";
        case UnequipResult::BackpackFull:
            return "its backpack has no room for it";
        default:
            return "it is not in the world";
    }
}
