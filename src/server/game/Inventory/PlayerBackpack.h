/*
 * Project Ambrose by Imjustchico
 * A wizard's backpack while it plays: the items it holds in the order they arrived, read from its stored rows, and how many it may hold, the m_numItemsAllowed its player template's inventory behavior gives plus the extra slots the server grants; an add to a full backpack is refused before an id is spent, so nothing is made or stored, and an item is trashed only by the wizard that holds it, of the template the client names, and while it is not locked.
 */

#ifndef AMBROSE_PLAYERBACKPACK_H
#define AMBROSE_PLAYERBACKPACK_H

#include "CharacterItem.h"
#include "GuidGenerator.h"

#include <optional>
#include <string_view>
#include <vector>

class PropertyObject;

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

class PlayerBackpack
{
public:
    static constexpr std::string_view ItemsAllowedProperty = "m_numItemsAllowed";

    static PlayerBackpack FromStored(std::vector<CharacterItem> stored);
    static uint32 CapacityFor(int64 itemsAllowed, uint32 extraSlots) noexcept;
    static std::optional<int64> ReadItemsAllowed(PropertyObject const& playerTemplate);

    std::vector<CharacterItem> const& GetItems() const noexcept { return _items; }
    std::size_t Size() const noexcept { return _items.size(); }
    CharacterItem const* Find(uint64 itemGuid) const noexcept;

    BackpackAdd Add(uint32 templateId, uint32 quantity, uint32 capacity, GuidGenerator& guids, uint64 now);
    BackpackTrashResult CanTrash(uint64 itemGuid, uint32 templateId) const noexcept;
    std::optional<CharacterItem> Remove(uint64 itemGuid);

private:
    std::vector<CharacterItem> _items;
    uint32 _nextSlot = 0;
};

#endif
