/*
 * Project Ambrose by Imjustchico
 * Manages character's personal bank: storing items in bank slots, enforcing bank capacity,
 * and handling movement of items between inventory and bank.
 */

#ifndef AMBROSE_BANKMGR_H
#define AMBROSE_BANKMGR_H

#include "CharacterSummary.h"
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

struct BankItem
{
    uint64 itemGuid = 0;   // item_instance guid
    uint16 slot = 0;       // bank slot
};

class BankLists
{
public:
    bool AddItem(uint64 characterId, uint64 itemGuid, uint16 slot = 0);
    bool RemoveItem(uint64 characterId, uint64 itemGuid);
    bool HasItem(uint64 characterId, uint64 itemGuid) const;
    uint16 GetSlot(uint64 characterId, uint64 itemGuid) const;
    void SetSlot(uint64 characterId, uint64 itemGuid, uint16 slot);
    std::size_t ItemCount(uint64 characterId) const;
    bool CanAddItem(uint64 characterId, uint16 capacity) const;
    std::map<uint64, BankItem> const& GetItems(uint64 characterId) const;

private:
    std::map<uint64, std::map<uint64, BankItem>> _bankItems; // characterId -> (itemGuid -> BankItem)
};

class BankMgr
{
public:
    static BankMgr& Instance();

    BankMgr(BankMgr const&) = delete;
    BankMgr& operator=(BankMgr const&) = delete;

    void SendBankContents(GameSession& session);
    void MoveItemToBank(GameSession& session, GameMessages::MoveInvToBank const& message);
    void MoveItemToInventory(GameSession& session, GameMessages::MoveBankToInv const& message);
    void MoveItemToSharedBank(GameSession& session, GameMessages::MoveBankToBank const& message);
    void DeleteItemFromBank(GameSession& session, GameMessages::BankDelete const& message);
    void AddStorageClient(GameSession& session, GameMessages::StorageClientAdd const& message);
    void RemoveStorageClient(GameSession& session, GameMessages::StorageClientRemove const& message);
    void HandleItemOverflowToBank(GameSession& session, GameMessages::ItemOverflowToBank const& message);
    void SendBankCount(GameSession& session, GameMessages::BankCount const& message);
    void UpdateBankLimit(GameSession& session, GameMessages::UpdateBankLimit const& message);
    void HandleQuickSellRequestBank(GameSession& session, GameMessages::QuickSellRequestBank const& message);

    bool IsItemInBank(uint64 characterId, uint64 itemGuid) const;
    uint16 GetItemSlotInBank(uint64 characterId, uint64 itemGuid) const;
    std::size_t GetBankItemCount(uint64 characterId) const;
    bool CanAddItemToBank(uint64 characterId, uint16 bankCapacity) const;

    void Clear();

private:
    BankMgr() = default;

    std::map<uint64, BankLists> _lists; // characterId -> BankLists for that character's personal bank
    std::unordered_set<uint64> _loaded; // characters whose bank has been loaded
    std::set<uint64> _pendingLoads;     // characters pending bank load

    BankLists& Lists(uint64 characterId);
    void LoadBank(GameSession& session, uint64 characterId, std::function<void(bool)> completion);
    void LoadItemInstance(GameSession& session, uint64 itemGuid, std::function<void(std::optional<ItemInstance>)> completion);
    void QueueQuery(GameSession& session, QueryCallback&& query);
    void QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work);
};

#define sBankMgr BankMgr::Instance()

#endif