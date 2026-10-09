/*
 * Project Ambrose by Imjustchico
 * Keeps a wizard's backpack in arrival order, works out its capacity, which never goes below zero or past the 32-bit range, and adds, checks, removes, locks and unlocks items by the rules its header gives.
 */

#include "PlayerBackpack.h"

#include <algorithm>
#include <limits>
#include <utility>

PlayerBackpack PlayerBackpack::FromStored(std::vector<CharacterItem> stored)
{
    std::stable_sort(stored.begin(), stored.end(), [](CharacterItem const& left, CharacterItem const& right) { return left.Slot < right.Slot; });
    PlayerBackpack backpack;
    backpack._items = std::move(stored);
    for (CharacterItem const& item : backpack._items)
        backpack._nextSlot = std::max(backpack._nextSlot, item.Slot == std::numeric_limits<uint32>::max() ? item.Slot : item.Slot + 1);
    return backpack;
}

uint32 PlayerBackpack::CapacityFor(int64 itemsAllowed, uint32 extraSlots) noexcept
{
    int64 const total = std::max<int64>(itemsAllowed, 0) + extraSlots;
    return static_cast<uint32>(std::min<int64>(total, std::numeric_limits<uint32>::max()));
}

CharacterItem const* PlayerBackpack::Find(uint64 itemGuid) const noexcept
{
    auto const found = std::find_if(_items.begin(), _items.end(), [itemGuid](CharacterItem const& item) { return item.Guid == itemGuid; });
    return found == _items.end() ? nullptr : &*found;
}

BackpackAdd PlayerBackpack::Add(uint32 templateId, uint32 quantity, uint32 capacity, GuidGenerator& guids, uint64 now)
{
    if (_items.size() >= capacity)
        return { BackpackAddResult::Full, std::nullopt };
    std::optional<uint64> const guid = guids.Generate();
    if (!guid)
        return { BackpackAddResult::NoItemId, std::nullopt };
    CharacterItem item;
    item.Guid = *guid;
    item.TemplateId = templateId;
    item.Quantity = std::max<uint32>(quantity, 1);
    item.Created = now;
    item.Slot = _nextSlot;
    if (_nextSlot != std::numeric_limits<uint32>::max())
        ++_nextSlot;
    _items.push_back(item);
    return { BackpackAddResult::Added, item };
}

BackpackTrashResult PlayerBackpack::CanTrash(uint64 itemGuid, uint32 templateId) const noexcept
{
    CharacterItem const* const item = Find(itemGuid);
    if (!item)
        return BackpackTrashResult::NotOwned;
    if (templateId != 0 && item->TemplateId != templateId)
        return BackpackTrashResult::WrongTemplate;
    if (item->Locked)
        return BackpackTrashResult::Locked;
    return BackpackTrashResult::Trashed;
}

std::optional<CharacterItem> PlayerBackpack::Remove(uint64 itemGuid)
{
    auto const found = std::find_if(_items.begin(), _items.end(), [itemGuid](CharacterItem const& item) { return item.Guid == itemGuid; });
    if (found == _items.end())
        return std::nullopt;
    CharacterItem removed = *found;
    _items.erase(found);
    return removed;
}

BackpackLockResult PlayerBackpack::ToggleLock(uint64 itemGuid) noexcept
{
    auto const found = std::find_if(_items.begin(), _items.end(), [itemGuid](CharacterItem const& item) { return item.Guid == itemGuid; });
    if (found == _items.end())
        return BackpackLockResult::NotOwned;
    found->Locked = !found->Locked;
    return found->Locked ? BackpackLockResult::Locked : BackpackLockResult::Unlocked;
}

uint32 PlayerBackpack::LockWord(CharacterItem const& item) noexcept
{
    return (item.Locked ? LockBit : 0u) | item.Pattern;
}
