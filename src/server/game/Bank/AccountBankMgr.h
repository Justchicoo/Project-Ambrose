/*
 * Project Ambrose by Imjustchico
 * Manages account's shared bank: storing items in shared bank slots, enforcing shared bank capacity,
 * and handling movement of items between personal bank and shared bank.
 */

#ifndef AMBROSE_ACCOUNTBANKMGR_H
#define AMBROSE_ACCOUNTBANKMGR_H

#include "AccountInfo.h"
#include "GameMessages.h"
#include "ItemInstance.h"
#include "DatabaseEnv.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <string_view>
#include <utility>
#include <vector>

class GameSession;
class QueryCallback;
#include "../ItemInstance.h"

struct SharedBankItem
{
    uint64 itemGuid = 0;   // item_instance guid
    uint16 slot = 0;       // shared bank slot
};

class AccountBankLists
{
public:
    bool AddItem(uint64 accountId, uint64 itemGuid, uint16 slot = 0);
    bool RemoveItem(uint64 accountId, uint64 itemGuid);
    bool HasItem(uint64 accountId, uint64 itemGuid) const;
    uint16 GetSlot(uint64 accountId, uint64 itemGuid) const;
    void SetSlot(uint64 accountId, uint64 itemGuid, uint16 slot);
    std::size_t ItemCount(uint64 accountId) const;
    bool CanAddItem(uint64 accountId, uint16 capacity) const;
    std::map<uint64, SharedBankItem> const& GetItems(uint64 accountId) const;

private:
    std::map<uint64, std::map<uint64, SharedBankItem>> _bankItems; // accountId -> (itemGuid -> SharedBankItem)
};

class AccountBankMgr
{
public:
    static AccountBankMgr& Instance();

    AccountBankMgr(AccountBankMgr const&) = delete;
    AccountBankMgr& operator=(AccountBankMgr const&) = delete;

    void SendSharedBankContents(GameSession& session);
    void MoveItemToSharedBank(GameSession& session, GameMessages::MoveBankToBank const& message);
    void MoveItemFromSharedBank(GameSession& session, GameMessages::MoveBankToInv const& message); // Note: reuse message? Actually, moving from shared bank to inventory might be a different message? We'll see.
    void DeleteItemFromSharedBank(GameSession& session, GameMessages::BankDelete const& message); // Maybe reuse? Or we need a specific message for shared bank delete.
    void AddStorageClient(GameSession& session, GameMessages::StorageClientAdd const& message);
    void RemoveStorageClient(GameSession& session, GameMessages::StorageClientRemove const& message);
    void HandleItemOverflowToSharedBank(GameSession& session, GameMessages::ItemOverflowToBank const& message);
    void SendSharedBankCount(GameSession& session, GameMessages::BankCount const& message);
    void UpdateSharedBankLimit(GameSession& session, GameMessages::UpdateBankLimit const& message);

    bool IsItemInSharedBank(uint64 accountId, uint64 itemGuid) const;
    uint16 GetItemSlotInSharedBank(uint64 accountId, uint64 itemGuid) const;
    std::size_t GetSharedBankItemCount(uint64 accountId) const;
    bool CanAddItemToSharedBank(uint64 accountId, uint16 sharedBankCapacity) const;

    void Clear();

private:
    AccountBankMgr() = default;

    std::map<uint64, AccountBankLists> _lists; // accountId -> AccountBankLists for that account's shared bank
    std::unordered_set<uint64> _loaded;        // accounts whose shared bank has been loaded
    std::set<uint64> _pendingLoads;            // accounts pending shared bank load

    AccountBankLists& Lists(uint64 accountId);
    void LoadSharedBank(GameSession& session, uint64 accountId, std::function<void(bool)> completion);
    void LoadItemInstance(GameSession& session, uint64 itemGuid, std::function<void(std::optional<ItemInstance>)> completion);
    void QueueQuery(GameSession& session, QueryCallback&& query);
    void QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work);
};

#define sAccountBankMgr AccountBankMgr::Instance()

#endif