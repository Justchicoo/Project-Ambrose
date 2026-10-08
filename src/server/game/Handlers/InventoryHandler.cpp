/*
 * Project Ambrose by Imjustchico
 * Changes a wizard's backpack on the world thread: an item is added only while the backpack has room for it under the capacity read at that moment, and an add to a full one sends MSG_ITEMDROP naming the template and stores nothing; an added item is stored with its instance and backpack row together and shown with GAME MSG_INVENTORYBEHAVIOR_ADDITEM; an item taken away or trashed is deleted only from the wizard that owns it and shown gone with MSG_INVENTORYBEHAVIOR_REMOVEITEM; MSG_TRASHINVENTORYITEM for an item the wizard does not hold, of another template, or locked is refused and logged; MSG_REQUESTTOGGLELOCKITEM locks or unlocks an item the wizard holds, stores it, and answers with the item's pattern word carrying the lock in its top bit; and MSG_LOOT shows the wizard the items it was given.
 */

#include "CharacterRepository.h"
#include "GameSession.h"
#include "ItemMgr.h"
#include "ItemObjectBuilder.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "ObjectSchemaMgr.h"
#include "Settings.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <chrono>
#include <string>

namespace
{
    constexpr char const* InventoryLog = "server.gamesession";
    constexpr uint32 ItemDropBackpackFull = 0;

    uint64 NowSeconds()
    {
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    std::string_view TrashResultName(BackpackTrashResult result)
    {
        switch (result)
        {
            case BackpackTrashResult::Trashed:
                return "trashed";
            case BackpackTrashResult::NotOwned:
                return "it does not hold that item";
            case BackpackTrashResult::WrongTemplate:
                return "the item it holds under that id is of another template";
            case BackpackTrashResult::Locked:
                return "the item is locked";
            default:
                return "it is not in the world";
        }
    }
}

uint32 GameSession::GetBackpackCapacity() const
{
    return PlayerBackpack::CapacityFor(_itemsAllowed, sSettings.Get<uint32>("Inventory.ExtraSlots"));
}

BackpackAdd GameSession::AddItem(ItemTemplateRecord const& itemTemplate, uint32 quantity)
{
    if (!_backpack)
        return { BackpackAddResult::NotInWorld, std::nullopt };
    if (itemTemplate.TemplateId == 0)
        return { BackpackAddResult::NoSuchTemplate, std::nullopt };
    uint32 const capacity = GetBackpackCapacity();
    BackpackAdd added = _backpack->Add(itemTemplate.TemplateId, quantity, capacity, ObjectGuid::ItemGuids(), NowSeconds());
    if (added.Result == BackpackAddResult::Full)
    {
        GameMessages::ItemDrop drop;
        drop.TemplateId = itemTemplate.TemplateId;
        drop.ErrorId = ItemDropBackpackFull;
        SendDmlMessage(drop);
        LOG_INFO(InventoryLog, "Session {} could not give wizard {} item template {}, since its backpack holds {} of {} item(s)", GetSessionId(), _worldGuid, itemTemplate.TemplateId,
            _backpack->Size(), capacity);
        return added;
    }
    if (!added.Item)
    {
        LOG_ERROR(InventoryLog, "Session {} could not give wizard {} item template {}, since no item id is left", GetSessionId(), _worldGuid, itemTemplate.TemplateId);
        return added;
    }
    SaveNewItem(*added.Item);
    SendItemAdded(itemTemplate, *added.Item);
    LOG_INFO(InventoryLog, "Session {} gave wizard {} item {} of template {}, and its backpack holds {} of {} item(s)", GetSessionId(), _worldGuid, added.Item->Guid,
        itemTemplate.TemplateId, _backpack->Size(), capacity);
    return added;
}

std::optional<CharacterItem> GameSession::RemoveItem(uint64 itemGuid)
{
    if (!_backpack)
        return std::nullopt;
    std::optional<CharacterItem> removed = _backpack->Remove(itemGuid);
    if (!removed)
        return std::nullopt;
    DeleteStoredItem(itemGuid);
    SendItemRemoved(itemGuid);
    LOG_INFO(InventoryLog, "Session {} took item {} of template {} from wizard {}, and its backpack holds {} item(s)", GetSessionId(), itemGuid, removed->TemplateId, _worldGuid,
        _backpack->Size());
    return removed;
}

BackpackTrashResult GameSession::TrashItem(uint64 itemGuid, uint32 templateId)
{
    if (!_backpack)
        return BackpackTrashResult::NotInWorld;
    BackpackTrashResult const verdict = _backpack->CanTrash(itemGuid, templateId);
    if (verdict != BackpackTrashResult::Trashed)
    {
        LOG_WARN(InventoryLog, "Session {} refused wizard {}'s request to trash item {} of template {}, since {}", GetSessionId(), _worldGuid, itemGuid, templateId,
            TrashResultName(verdict));
        return verdict;
    }
    RemoveItem(itemGuid);
    return verdict;
}

void GameSession::HandleTrashInventoryItem(GameMessages::TrashInventoryItem& message)
{
    TrashItem(message.GlobalId, static_cast<uint32>(message.TemplateId));
}

BackpackLockResult GameSession::ToggleItemLock(uint64 itemGuid)
{
    if (!_backpack)
        return BackpackLockResult::NotInWorld;
    BackpackLockResult const verdict = _backpack->ToggleLock(itemGuid);
    CharacterItem const* const item = _backpack->Find(itemGuid);
    if (!item)
    {
        LOG_WARN(InventoryLog, "Session {} refused wizard {}'s request to lock or unlock item {}, since it does not hold that item", GetSessionId(), _worldGuid, itemGuid);
        return verdict;
    }
    SaveItemLock(*item);
    GameMessages::RequestToggleLockItem locked;
    locked.ItemId = itemGuid;
    locked.GlobalId = _worldGuid;
    locked.IsLocked = PlayerBackpack::LockWord(*item);
    SendDmlMessage(locked);
    LOG_INFO(InventoryLog, "Session {} {} item {} of wizard {}", GetSessionId(), item->Locked ? "locked" : "unlocked", itemGuid, _worldGuid);
    return verdict;
}

void GameSession::HandleRequestToggleLockItem(GameMessages::RequestToggleLockItem& message)
{
    LOG_DEBUG(InventoryLog, "Session {} asks to toggle the lock of item {} for {}, sending IsLocked {:#x}", GetSessionId(), message.ItemId, message.GlobalId, message.IsLocked);
    ToggleItemLock(message.ItemId);
}

void GameSession::HandleItemLock(GameMessages::ItemLock& message)
{
    if (!_player)
        return;
    bool const enabled = message.Enabled != 0;
    if (_player->SetShowItemLock(enabled))
        SaveStatsIfDirty();
    LOG_INFO(InventoryLog, "Session {} turned wizard {}'s backpack item lock {}", GetSessionId(), _worldGuid, enabled ? "on" : "off");
}

bool GameSession::ShowLoot(std::vector<LootItem> const& items)
{
    std::string problem;
    PropertyObjectPtr const list = LootListBuilder::Build(sTypeRegistry.GetCatalog(), items, problem);
    std::optional<std::string> const encoded = list ? LootListBuilder::Encode(GameMessages::Loot::Tag, *list, problem) : std::nullopt;
    if (!encoded)
    {
        LOG_ERROR(InventoryLog, "Session {} shows wizard {} no loot, since {}", GetSessionId(), _worldGuid, problem);
        return false;
    }
    GameMessages::Loot loot;
    loot.GlobalId = _worldGuid;
    loot.LootList = *encoded;
    return SendDmlMessage(loot);
}

void GameSession::SaveNewItem(CharacterItem const& item)
{
    CharacterRepository::CreateTransaction transaction = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareAddItem(_worldGuid, item) : nullptr;
    if (!transaction)
    {
        LOG_ERROR(InventoryLog, "Session {} could not write item {} of wizard {}'s backpack, since the characters database is not open", GetSessionId(), item.Guid, _worldGuid);
        return;
    }
    CharacterDatabase.CommitTransaction(std::move(transaction));
}

void GameSession::DeleteStoredItem(uint64 itemGuid)
{
    CharacterRepository::Statement statement = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareTrashItem(_worldGuid, itemGuid) : nullptr;
    if (!statement)
    {
        LOG_ERROR(InventoryLog, "Session {} could not delete item {} of wizard {}'s backpack, since the characters database is not open", GetSessionId(), itemGuid, _worldGuid);
        return;
    }
    CharacterDatabase.Execute(std::move(statement));
}

void GameSession::SaveItemLock(CharacterItem const& item)
{
    CharacterRepository::Statement statement = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareLockItem(_worldGuid, item.Guid, item.Locked) : nullptr;
    if (!statement)
    {
        LOG_ERROR(InventoryLog, "Session {} could not store the lock of item {} of wizard {}'s backpack, since the characters database is not open", GetSessionId(), item.Guid,
            _worldGuid);
        return;
    }
    CharacterDatabase.Execute(std::move(statement));
}

void GameSession::SendItemAdded(ItemTemplateRecord const& itemTemplate, CharacterItem const& item)
{
    CoreObjectTypeTablePtr const types = sObjectSchemaMgr.GetCoreObjectTypes();
    std::string problem;
    PropertyObjectPtr const object = types ? ItemObjectBuilder::Build(sTypeRegistry.GetCatalog(), *types, itemTemplate, item, problem) : nullptr;
    if (!types)
        problem = "the core object types are not loaded";
    std::optional<std::string> const encoded = object ? ItemObjectBuilder::Encode(GameMessages::InventoryBehaviorAddItem::Tag, *object, *types, problem) : std::nullopt;
    if (!encoded)
    {
        LOG_ERROR(InventoryLog, "Session {} stored item {} for wizard {} but cannot show it until the wizard enters again, since {}", GetSessionId(), item.Guid, _worldGuid, problem);
        return;
    }
    GameMessages::InventoryBehaviorAddItem added;
    added.GlobalId = _worldGuid;
    added.SerializedItem = *encoded;
    SendDmlMessage(added);
}

void GameSession::SendItemRemoved(uint64 itemGuid)
{
    GameMessages::InventoryBehaviorRemoveItem removed;
    removed.GlobalId = _worldGuid;
    removed.ItemId = itemGuid;
    SendDmlMessage(removed);
}
