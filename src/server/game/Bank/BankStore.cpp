/*
 * Project Ambrose by Imjustchico
 * Keeps a bank's items in arrival order with the slot each occupies, so a deposit takes the lowest free slot, a withdrawal frees its slot, and both see the same map of slots to items.
 */

#include "BankStore.h"

#include <limits>

BankStore BankStore::FromStored(std::vector<CharacterItem> stored)
{
    BankStore store;
    for (CharacterItem& item : stored)
    {
        if (item.Guid == 0)
            continue;
        store._slots[item.Slot] = item.Guid;
        store._items.push_back(item);
    }
    return store;
}

CharacterItem const* BankStore::Find(uint64 itemGuid) const noexcept
{
    if (itemGuid == 0)
        return nullptr;
    for (CharacterItem const& item : _items)
        if (item.Guid == itemGuid)
            return &item;
    return nullptr;
}

bool BankStore::CanAdd(uint32 capacity) const noexcept
{
    return _items.size() < capacity;
}

std::optional<uint32> BankStore::FreeSlot(uint32 capacity) const noexcept
{
    if (!CanAdd(capacity))
        return std::nullopt;
    uint32 slot = 0;
    for (auto const& [occupied, guid] : _slots)
    {
        if (occupied > slot)
            break;
        slot = occupied + 1;
    }
    return slot;
}

CharacterItem BankStore::Put(CharacterItem item, uint32 capacity)
{
    if (item.Guid == 0 || !CanAdd(capacity))
        return item;
    if (auto const free = FreeSlot(capacity))
    {
        item.Slot = *free;
        _slots[item.Slot] = item.Guid;
        _items.push_back(item);
    }
    return item;
}

CharacterItem BankStore::PutBack(CharacterItem item)
{
    if (item.Guid == 0)
        return item;
    _slots[item.Slot] = item.Guid;
    _items.push_back(item);
    return item;
}

std::optional<CharacterItem> BankStore::Remove(uint64 itemGuid)
{
    for (auto it = _items.begin(); it != _items.end(); ++it)
    {
        if (it->Guid != itemGuid)
            continue;
        CharacterItem removed = *it;
        _slots.erase(removed.Slot);
        _items.erase(it);
        return removed;
    }
    return std::nullopt;
}
