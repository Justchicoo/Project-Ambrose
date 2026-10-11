/*
 * Project Ambrose by Imjustchico
 * A wizard's personal bank and the moves of the banking messages: a deposit takes an item the wizard holds out of its backpack, stores it in the lowest free slot under the live Bank.Capacity, and deletes the backpack row while the instance and its bank row stay; a withdrawal restores both rows; a move to the shared bank and a deletion go through the account's bank, and the confirms echo the ClientRequestID of the message that asked with Failure 0 for success; the bank is read from its rows as the wizard enters and the count it sends is what the store holds.
 */

#ifndef AMBROSE_BANKMGR_H
#define AMBROSE_BANKMGR_H

#include "CharacterItem.h"
#include "GameMessages.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class GameSession;
class QueryCallback;
class BankStore;

enum class BankMoveFailure : uint8
{
    NotOwned = 1,
    BankFull = 2,
    InventoryFull = 3,
    Locked = 4,
    NotInWorld = 5,
    InternalError = 6
};

class BankMgr
{
public:
    static BankMgr& Instance();

    BankMgr(BankMgr const&) = delete;
    BankMgr& operator=(BankMgr const&) = delete;

    void LoadBank(GameSession& session, uint64 characterId, std::function<void(bool)> completion);
    void SendBankContents(GameSession& session, std::function<void(bool)> completion = {});
    void MoveItemToBank(GameSession& session, GameMessages::MoveInvToBank const& message);
    void MoveItemToInventory(GameSession& session, GameMessages::MoveBankToInv const& message);
    void MoveItemToSharedBank(GameSession& session, GameMessages::MoveBankToBank const& message);
    void DeleteItemFromBank(GameSession& session, GameMessages::BankDelete const& message);
    void SendBankCount(GameSession& session);
    void HandleQuickSellRequestBank(GameSession& session, GameMessages::QuickSellRequestBank const& message);

    bool AddItem(uint64 characterId, CharacterItem const& item);
    bool IsItemInBank(uint64 characterId, uint64 itemGuid) const;
    uint32 GetBankItemCount(uint64 characterId) const;
    bool CanAddItemToBank(uint64 characterId, uint32 bankCapacity) const;

    void Clear();

private:
    BankMgr() = default;

    BankStore const* Store(uint64 characterId) const;
    BankStore* Store(uint64 characterId);
    void QueueQuery(GameSession& session, QueryCallback&& query);
    void QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work);

    std::unordered_map<uint64, BankStore> _banks;
    std::unordered_set<uint64> _loaded;
};

#define sBankMgr BankMgr::Instance()

#endif
