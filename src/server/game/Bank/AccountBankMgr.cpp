/*
 * Project Ambrose by Imjustchico
 * Manages account's shared bank: storing items in shared bank slots, enforcing shared bank capacity,
 * and handling movement of items between personal bank and shared bank.
 */

#include "AccountBankMgr.h"
#include "AccountInfo.h"
#include "CharacterDatabase.h"
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

// AccountBankLists implementation
bool AccountBankLists::AddItem(uint64 accountId, uint64 itemGuid, uint16 slot) {
    auto& accMap = _bankItems[accountId];
    if (accMap.find(itemGuid) != accMap.end()) {
        return false; // already exists
    }
    accMap[itemGuid] = {itemGuid, slot};
    return true;
}

bool AccountBankLists::RemoveItem(uint64 accountId, uint64 itemGuid) {
    auto it = _bankItems.find(accountId);
    if (it == _bankItems.end()) return false;
    auto& accMap = it->second;
    auto itemIt = accMap.find(itemGuid);
    if (itemIt == accMap.end()) return false;
    accMap.erase(itemIt);
    if (accMap.empty()) {
        _bankItems.erase(it);
    }
    return true;
}

bool AccountBankLists::HasItem(uint64 accountId, uint64 itemGuid) const {
    auto it = _bankItems.find(accountId);
    if (it == _bankItems.end()) return false;
    return it->second.find(itemGuid) != it->second.end();
}

uint16 AccountBankLists::GetSlot(uint64 accountId, uint64 itemGuid) const {
    auto it = _bankItems.find(accountId);
    if (it == _bankItems.end()) return 0;
    auto itemIt = it->second.find(itemGuid);
    if (itemIt == it->second.end()) return 0;
    return itemIt->second.slot;
}

void AccountBankLists::SetSlot(uint64 accountId, uint64 itemGuid, uint16 slot) {
    auto it = _bankItems.find(accountId);
    if (it != _bankItems.end()) {
        auto itemIt = it->second.find(itemGuid);
        if (itemIt != it->second.end()) {
            itemIt->second.slot = slot;
        }
    }
}

std::size_t AccountBankLists::ItemCount(uint64 accountId) const {
    auto it = _bankItems.find(accountId);
    if (it == _bankItems.end()) return 0;
    return it->second.size();
}

bool AccountBankLists::CanAddItem(uint64 accountId, uint16 capacity) const {
    auto it = _bankItems.find(accountId);
    std::size_t currentCount = (it == _bankItems.end()) ? 0 : it->second.size();
    return currentCount < capacity;
}

std::map<uint64, SharedBankItem> const& AccountBankLists::GetItems(uint64 accountId) const {
    static std::map<uint64, SharedBankItem> empty;
    auto it = _bankItems.find(accountId);
    if (it == _bankItems.end()) return empty;
    return it->second;
}

// AccountBankMgr implementation
AccountBankMgr& AccountBankMgr::Instance() {
    static AccountBankMgr instance;
    return instance;
}

void AccountBankMgr::SendSharedBankContents(GameSession& session) {
    // TODO: Implement sending shared bank contents to client
}

void AccountBankMgr::MoveItemToSharedBank(GameSession& session, GameMessages::MoveBankToBank const& message) {
    // TODO: Implement moving item from personal bank to shared bank
    // This would involve getting the character's account ID, then moving the item from personal bank to shared bank.
}

void AccountBankMgr::MoveItemFromSharedBank(GameSession& session, GameMessages::MoveBankToInv const& message) {
    // TODO: Implement moving item from shared bank to inventory
}

void AccountBankMgr::DeleteItemFromSharedBank(GameSession& session, GameMessages::BankDelete const& message) {
    // TODO: Implement deleting item from shared bank
}

void AccountBankMgr::AddStorageClient(GameSession& session, GameMessages::StorageClientAdd const& message) {
    // TODO: Implement adding a storage client (maybe for shared bank access?)
}

void AccountBankMgr::RemoveStorageClient(GameSession& session, GameMessages::StorageClientRemove const& message) {
    // TODO: Implement removing a storage client
}

void AccountBankMgr::HandleItemOverflowToSharedBank(GameSession& session, GameMessages::ItemOverflowToBank const& message) {
    // TODO: Handle item overflow to shared bank (when personal bank is full, item goes to shared bank if space?)
}

void AccountBankMgr::SendSharedBankCount(GameSession& session, GameMessages::BankCount const& message) {
    // TODO: Send shared bank count to client
}

void AccountBankMgr::UpdateSharedBankLimit(GameSession& session, GameMessages::UpdateBankLimit const& message) {
    // TODO: Update shared bank limit from live settings and notify client if needed
}

bool AccountBankMgr::IsItemInSharedBank(uint64 accountId, uint64 itemGuid) const {
    auto it = _bankItems.find(accountId);
    if (it == _bankItems.end()) return false;
    return it->second.find(itemGuid) != it->second.end();
}

uint16 AccountBankMgr::GetItemSlotInSharedBank(uint64 accountId, uint64 itemGuid) const {
    auto it = _bankItems.find(accountId);
    if (it == _bankItems.end()) return 0;
    auto itemIt = it->second.find(itemGuid);
    if (itemIt == it->second.end()) return 0;
    return itemIt->second.slot;
}

std::size_t AccountBankMgr::GetSharedBankItemCount(uint64 accountId) const {
    auto it = _bankItems.find(accountId);
    if (it == _bankItems.end()) return 0;
    return it->second.size();
}

bool AccountBankMgr::CanAddItemToSharedBank(uint64 accountId, uint16 sharedBankCapacity) const {
    auto it = _bankItems.find(accountId);
    std::size_t currentCount = (it == _bankItems.end()) ? 0 : it->second.size();
    return currentCount < sharedBankCapacity;
}

void AccountBankMgr::Clear() {
    _bankItems.clear();
    _loaded.clear();
    _pendingLoads.clear();
}

AccountBankLists& AccountBankMgr::Lists(uint64 accountId) {
    return _bankItems[accountId];
}

void AccountBankMgr::LoadSharedBank(GameSession& session, uint64 accountId, std::function<void(bool)> completion) {
    if (_loaded.contains(accountId)) {
        completion(true);
        return;
    }

    auto statement = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACCOUNT_SHARED_BANK);
    if (!statement || !CharacterDatabase.IsOpen()) {
        LOG_ERROR("server.bank", "Could not prepare loading the shared bank for account {}", accountId);
        completion(false);
        return;
    }

    statement->SetData(0, accountId);
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(statement), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, accountId, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        std::map<uint64, SharedBankItem> sharedBankItems;
        if (result && result->GetRowCount() > 0) {
            PreparedResultSet const& row = *result;
            do {
                bool hasItem = row[0].Get<bool>();
                uint64 itemGuid = row[1].Get<uint64>();
                uint16 slot = row[2].Get<uint16>();

                if (hasItem && itemGuid != 0) {
                    sharedBankItems[itemGuid] = {itemGuid, slot};
                }
            } while (result->NextRow());
        }

        QueueWorldWork(*owner, [this, accountId, sharedBankItems = std::move(sharedBankItems), completion = std::move(completion)](GameSession&) mutable
        {
            if (!_loaded.contains(accountId)) {
                auto& accMap = _bankItems[accountId];
                accMap = std::move(sharedBankItems);
                _loaded.insert(accountId);
            }
            completion(true);
        });
    });
}

void AccountBankMgr::LoadItemInstance(GameSession& session, uint64 itemGuid, std::function<void(std::optional<ItemInstance>)> completion) {
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

void AccountBankMgr::QueueQuery(GameSession& session, QueryCallback&& query) {
    session._queryCallbacks.AddCallback(std::move(query));
    session._queryCallbacks.ProcessReadyCallbacks();
}

void AccountBankMgr::QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work) {
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