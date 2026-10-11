/*
 * Project Ambrose by Imjustchico
 * Manages character's personal bank: storing items in bank slots, enforcing bank capacity,
 * and handling movement of items between inventory and bank.
 */

#include "BankMgr.h"
#include "CharacterDatabase.h"
#include "CharacterSummary.h"
#include "GameMessages.h"
#include "../ItemInstance.h"
#include "DatabaseEnv.h"
#include "GameSession.h"
#include "Log.h"
#include "QueryCallback.h"
#include "World.h"

#include <chrono>
#include <functional>
#include <iostream>

// BankLists implementation
bool BankLists::AddItem(uint64 characterId, uint64 itemGuid, uint16 slot) {
    auto& charMap = _bankItems[characterId];
    if (charMap.find(itemGuid) != charMap.end()) {
        return false; // already exists
    }
    charMap[itemGuid] = {itemGuid, slot};
    return true;
}

bool BankLists::RemoveItem(uint64 characterId, uint64 itemGuid) {
    auto it = _bankItems.find(characterId);
    if (it == _bankItems.end()) return false;
    auto& charMap = it->second;
    auto itemIt = charMap.find(itemGuid);
    if (itemIt == charMap.end()) return false;
    charMap.erase(itemIt);
    if (charMap.empty()) {
        _bankItems.erase(it);
    }
    return true;
}

bool BankLists::HasItem(uint64 characterId, uint64 itemGuid) const {
    auto it = _bankItems.find(characterId);
    if (it == _bankItems.end()) return false;
    return it->second.find(itemGuid) != it->second.end();
}

uint16 BankLists::GetSlot(uint64 characterId, uint64 itemGuid) const {
    auto it = _bankItems.find(characterId);
    if (it == _bankItems.end()) return 0;
    auto itemIt = it->second.find(itemGuid);
    if (itemIt == it->second.end()) return 0;
    return itemIt->second.slot;
}

void BankLists::SetSlot(uint64 characterId, uint64 itemGuid, uint16 slot) {
    auto it = _bankItems.find(characterId);
    if (it != _bankItems.end()) {
        auto itemIt = it->second.find(itemGuid);
        if (itemIt != it->second.end()) {
            itemIt->second.slot = slot;
        }
    }
}

std::size_t BankLists::ItemCount(uint64 characterId) const {
    auto it = _bankItems.find(characterId);
    if (it == _bankItems.end()) return 0;
    return it->second.size();
}

bool BankLists::CanAddItem(uint64 characterId, uint16 capacity) const {
    auto it = _bankItems.find(characterId);
    std::size_t currentCount = (it == _bankItems.end()) ? 0 : it->second.size();
    return currentCount < capacity;
}

std::map<uint64, BankItem> const& BankLists::GetItems(uint64 characterId) const {
    static std::map<uint64, BankItem> empty;
    auto it = _bankItems.find(characterId);
    if (it == _bankItems.end()) return empty;
    return it->second;
}

// BankMgr implementation
BankMgr& BankMgr::Instance() {
    static BankMgr instance;
    return instance;
}

void BankMgr::SendBankContents(GameSession& session) {
    // TODO: Implement sending bank contents to client
    // For now, we just load the bank and then send a packet with the items.
    // We need to construct the appropriate GameMessages.
}

void BankMgr::MoveItemToBank(GameSession& session, GameMessages::MoveInvToBank const& message) {
    // TODO: Implement moving item from inventory to bank
    // Check if the item exists in inventory, then move it to bank slot.
    // Enforce capacity.
}

void BankMgr::MoveItemToInventory(GameSession& session, GameMessages::MoveBankToInv const& message) {
    // TODO: Implement moving item from bank to inventory
}

void BankMgr::MoveItemToSharedBank(GameSession& session, GameMessages::MoveBankToBank const& message) {
    // TODO: Implement moving item from personal bank to shared bank
    // This would involve the AccountBankMgr (to be implemented).
}

void BankMgr::DeleteItemFromBank(GameSession& session, GameMessages::BankDelete const& message) {
    // TODO: Implement deleting item from bank
}

void BankMgr::AddStorageClient(GameSession& session, GameMessages::StorageClientAdd const& message) {
    // TODO: Implement adding a storage client (maybe for shared bank access?)
}

void BankMgr::RemoveStorageClient(GameSession& session, GameMessages::StorageClientRemove const& message) {
    // TODO: Implement removing a storage client
}

void BankMgr::HandleItemOverflowToBank(GameSession& session, GameMessages::ItemOverflowToBank const& message) {
    // TODO: Handle item overflow to bank (when inventory is full, item goes to bank if space)
}

void BankMgr::SendBankCount(GameSession& session, GameMessages::BankCount const& message) {
    // TODO: Send bank count to client (maybe the number of items or used slots)
}

void BankMgr::UpdateBankLimit(GameSession& session, GameMessages::UpdateBankLimit const& message) {
    // TODO: Update bank limit from live settings and notify client if needed
}

void BankMgr::HandleQuickSellRequestBank(GameSession& session, GameMessages::QuickSellRequestBank const& message) {
    // TODO: Handle quick sell request from bank
}

bool BankMgr::IsItemInBank(uint64 characterId, uint64 itemGuid) const {
    auto it = _bankItems.find(characterId);
    if (it == _bankItems.end()) return false;
    return it->second.find(itemGuid) != it->second.end();
}

uint16 BankMgr::GetItemSlotInBank(uint64 characterId, uint64 itemGuid) const {
    auto it = _bankItems.find(characterId);
    if (it == _bankItems.end()) return 0;
    auto itemIt = it->second.find(itemGuid);
    if (itemIt == it->second.end()) return 0;
    return itemIt->second.slot;
}

std::size_t BankMgr::GetBankItemCount(uint64 characterId) const {
    auto it = _bankItems.find(characterId);
    if (it == _bankItems.end()) return 0;
    return it->second.size();
}

bool BankMgr::CanAddItemToBank(uint64 characterId, uint16 bankCapacity) const {
    auto it = _bankItems.find(characterId);
    std::size_t currentCount = (it == _bankItems.end()) ? 0 : it->second.size();
    return currentCount < bankCapacity;
}

void BankMgr::Clear() {
    _bankItems.clear();
    _loaded.clear();
    _pendingLoads.clear();
}

BankLists& BankMgr::Lists(uint64 characterId) {
    return _bankItems[characterId];
}

void BankMgr::LoadBank(GameSession& session, uint64 characterId, std::function<void(bool)> completion) {
    if (_loaded.contains(characterId)) {
        completion(true);
        return;
    }

    auto statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BANK);
    if (!statement || !CharacterDatabase.IsOpen()) {
        LOG_ERROR("server.bank", "Could not prepare loading the bank for wizard {}", characterId);
        completion(false);
        return;
    }

    statement->SetData(0, characterId);
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(statement), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, characterId, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        std::map<uint64, BankItem> bankItems;
        if (result && result->GetRowCount() > 0) {
            PreparedResultSet const& row = *result;
            do {
                bool hasItem = row[0].Get<bool>();
                uint64 itemGuid = row[1].Get<uint64>();
                uint16 slot = row[2].Get<uint16>();

                if (hasItem && itemGuid != 0) {
                    bankItems[itemGuid] = {itemGuid, slot};
                }
            } while (result->NextRow());
        }

        QueueWorldWork(*owner, [this, characterId, bankItems = std::move(bankItems), completion = std::move(completion)](GameSession&) mutable
        {
            if (!_loaded.contains(characterId)) {
                auto& charMap = _bankItems[characterId];
                charMap = std::move(bankItems);
                _loaded.insert(characterId);
            }
            completion(true);
        });
    });
}

void BankMgr::LoadItemInstance(GameSession& session, uint64 itemGuid, std::function<void(std::optional<ItemInstance>)> completion) {
    auto statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_INSTANCE_BY_GUID);
    if (!statement || !CharacterDatabase.IsOpen()) {
        LOG_ERROR("server.bank", "Could not prepare loading item instance {}", itemGuid);
        completion(std::nullopt);
        return;
    }

    statement->SetData(0, itemGuid);
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(statement), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        std::optional<ItemInstance> item;
        if (result && result->GetRowCount() > 0) {
            PreparedResultSet const& row = *result;
            item = ItemInstance{
                row[0].Get<uint64>(),   // guid
                row[1].Get<uint64>(),   // owner
                row[2].Get<uint32>(),   // template
                row[3].Get<uint32>(),   // quantity
                row[4].Get<uint32>(),   // primary_color
                row[5].Get<uint32>(),   // secondary_color
                row[6].Get<uint32>(),   // pattern
                row[7].Get<bool>(),     // locked
                row[8].Get<uint32>(),   // flags
                row[9].Get<uint64>()    // created
            };
        }
        QueueWorldWork(*owner, [completion = std::move(completion), item = std::move(item)](GameSession&) mutable
        {
            completion(item);
        });
    });
}

void BankMgr::QueueQuery(GameSession& session, QueryCallback&& query) {
    session._queryCallbacks.AddCallback(std::move(query));
    session._queryCallbacks.ProcessReadyCallbacks();
}

void BankMgr::QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work) {
    std::shared_ptr<GameSession> owner = session.SharedSelf();
    if (!owner->IsOpen())
        return;
    if (!owner->QueueInbound([owner, work = std::move(work)]() mutable
    {
        if (owner->IsOpen())
            work(*owner);
    }))
    {
        if (owner->IsOpen())
            LOG_ERROR("server.bank", "Could not queue database work for session {}", owner->GetSessionId());
    }
}