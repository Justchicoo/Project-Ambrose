/*
 * Project Ambrose by Imjustchico
 * The items one bank holds, a wizard's personal or an account's shared alike: stored in the order their rows arrive, an item added only while the bank has room under the capacity its caller reads at that moment, the item it holds found by its id, and one taken away only to be returned whole; the slot of a taken item rests for its next arrival.
 */

#ifndef AMBROSE_BANKSTORE_H
#define AMBROSE_BANKSTORE_H

#include "CharacterItem.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

class BankStore
{
public:
    BankStore() = default;

    static BankStore FromStored(std::vector<CharacterItem> stored);

    std::vector<CharacterItem> const& GetItems() const noexcept { return _items; }
    std::size_t Size() const noexcept { return _items.size(); }
    CharacterItem const* Find(uint64 itemGuid) const noexcept;
    bool Holds(uint64 itemGuid) const noexcept { return Find(itemGuid) != nullptr; }
    bool CanAdd(uint32 capacity) const noexcept;
    std::optional<uint32> FreeSlot(uint32 capacity) const noexcept;
    CharacterItem Put(CharacterItem item, uint32 capacity);
    CharacterItem PutBack(CharacterItem item);
    std::optional<CharacterItem> Remove(uint64 itemGuid);
    void Clear() noexcept { _items.clear(); }

private:
    std::vector<CharacterItem> _items;
    std::map<uint32, uint64> _slots;
};

#endif
