/*
 * Project Ambrose by Imjustchico
 * A wizard's backpack while it plays: the items it holds in the order they arrived, read from its stored rows, and how many it may hold, the slots the server gives every backpack plus the extra slots it grants; an add to a full backpack is refused before an id is spent, so nothing is made or stored, and an item is trashed only by the wizard that holds it, of the template the client names, and while it is not locked; the wizard that holds an item locks and unlocks it, and the client reads the lock as the top bit of the item's pattern word; an item taken off returns to the backpack as its newest arrival.
 */

#ifndef AMBROSE_PLAYERBACKPACK_H
#define AMBROSE_PLAYERBACKPACK_H

#include "CharacterItem.h"
#include "GuidGenerator.h"

#include <optional>
#include <string_view>
#include <vector>

enum class BackpackAddResult : uint8
{
    Added,
    Full,
    NoItemId,
    NoSuchTemplate,
    NotInWorld
};

struct BackpackAdd
{
    BackpackAddResult Result = BackpackAddResult::Full;
    std::optional<CharacterItem> Item;
};

enum class BackpackTrashResult : uint8
{
    Trashed,
    NotOwned,
    WrongTemplate,
    Locked,
    NotInWorld
};

enum class BackpackLockResult : uint8
{
    Locked,
    Unlocked,
    NotOwned,
    NotInWorld
};

class PlayerBackpack
{
public:
    static constexpr uint32 LockBit = 0x80000000u;

    static PlayerBackpack FromStored(std::vector<CharacterItem> stored);
    static uint32 CapacityFor(int64 itemsAllowed, uint32 extraSlots) noexcept;

    std::vector<CharacterItem> const& GetItems() const noexcept { return _items; }
    std::size_t Size() const noexcept { return _items.size(); }
    CharacterItem const* Find(uint64 itemGuid) const noexcept;

    BackpackAdd Add(uint32 templateId, uint32 quantity, uint32 capacity, GuidGenerator& guids, uint64 now);
    BackpackTrashResult CanTrash(uint64 itemGuid, uint32 templateId) const noexcept;
    std::optional<CharacterItem> Remove(uint64 itemGuid);
    CharacterItem Put(CharacterItem item);
    BackpackLockResult ToggleLock(uint64 itemGuid) noexcept;
    static uint32 LockWord(CharacterItem const& item) noexcept;

private:
    std::vector<CharacterItem> _items;
    uint32 _nextSlot = 0;
};

#endif
