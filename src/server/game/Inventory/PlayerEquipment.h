/*
 * Project Ambrose by Imjustchico
 * The items a wizard wears while it plays, each with the slot it is worn in, in the order it put them on, and the rules that move an item between them and its backpack: an item is equipped only from the wizard's own backpack, into a slot its template's adjectives fit, when the slot's requirements and the item's equip requirements hold for the wizard's school and level, and a slot already holding as many items as it allows gives its oldest back to the backpack, which must have room for it; an unequipped item goes back to a backpack with room for it.
 */

#ifndef AMBROSE_PLAYEREQUIPMENT_H
#define AMBROSE_PLAYEREQUIPMENT_H

#include "CharacterItem.h"
#include "EquipmentSlots.h"
#include "ItemTemplateStore.h"
#include "PlayerBackpack.h"
#include "RequirementMgr.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class EquipResult : uint8
{
    Equipped,
    NotOwned,
    NoSuchSlot,
    NoSuchTemplate,
    WrongSlot,
    SlotRequirementsNotMet,
    RequirementsNotMet,
    BackpackFull,
    NotInWorld
};

enum class UnequipResult : uint8
{
    Unequipped,
    NotWorn,
    BackpackFull,
    NotInWorld
};

struct EquipChange
{
    EquipResult Result = EquipResult::NotOwned;
    std::optional<CharacterEquippedItem> Worn;
    std::optional<CharacterItem> Returned;
    std::size_t ReturnedIndex = 0;
};

struct UnequipChange
{
    UnequipResult Result = UnequipResult::NotWorn;
    std::optional<CharacterEquippedItem> TakenOff;
    std::optional<CharacterItem> Returned;
    std::size_t Index = 0;
};

class EquipWizard : public RequirementContext
{
public:
    EquipWizard(std::string school, int32 level) : _school(std::move(school)), _level(level) {}

    std::optional<double> GetMagicLevel(std::string_view school) const override;
    std::optional<bool> HasSchoolOfFocus(std::string_view school) const override;

private:
    std::string _school;
    int32 _level = 0;
};

class PlayerEquipment
{
public:
    static PlayerEquipment FromStored(std::vector<CharacterEquippedItem> stored);

    std::vector<CharacterEquippedItem> const& GetItems() const noexcept { return _items; }
    std::size_t Size() const noexcept { return _items.size(); }
    CharacterEquippedItem const* Find(uint64 itemGuid) const noexcept;
    std::optional<std::size_t> IndexOf(uint64 itemGuid) const noexcept;
    std::size_t CountIn(std::string_view slot) const noexcept;

    EquipChange Equip(PlayerBackpack& backpack, uint32 capacity, EquipmentSlots const& slots, ItemTemplateStore const& templates, RequirementContext const& wizard,
        uint64 itemGuid, std::string_view slotName);
    UnequipChange Unequip(PlayerBackpack& backpack, uint32 capacity, uint64 itemGuid);

    static std::string_view ResultName(EquipResult result) noexcept;
    static std::string_view ResultName(UnequipResult result) noexcept;

private:
    std::vector<CharacterEquippedItem> _items;
};

#endif
