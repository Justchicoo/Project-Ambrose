/*
 * Project Ambrose by Imjustchico
 * An account's shared bank, the one bank every wizard of the account sees: loaded once per account from the account's rows as its first wizard enters, holding items of whichever wizard deposited them, with deposits from a wizard's backpack, withdrawals into the withdrawing wizard's backpack, moves back into that wizard's bank, and deletions of the shared reagents and pet snacks, all under the live Bank.SharedCapacity, every confirm echoing the ClientRequestID of the message that asked with Failure 0 for success; the count it sends is what the store holds.
 */

#ifndef AMBROSE_ACCOUNTBANKMGR_H
#define AMBROSE_ACCOUNTBANKMGR_H

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

class AccountBankMgr
{
public:
    static AccountBankMgr& Instance();

    AccountBankMgr(AccountBankMgr const&) = delete;
    AccountBankMgr& operator=(AccountBankMgr const&) = delete;

    void LoadSharedBank(GameSession& session, uint64 accountId, std::function<void(bool)> completion);
    void SendSharedBankContents(GameSession& session);
    void MoveItemToSharedBank(GameSession& session, GameMessages::MoveInvToBank const& message);
    void MoveItemFromSharedBank(GameSession& session, GameMessages::MoveBankToInv const& message);
    void MoveItemToBank(GameSession& session, GameMessages::MoveBankToBank const& message);
    void DeleteItemFromSharedBank(GameSession& session, GameMessages::SharedBankDeleteReagentOrPetSnack const& message);
    void SendSharedBankCount(GameSession& session);

    bool AddItem(uint64 accountId, CharacterItem const& item);
    bool IsItemInSharedBank(uint64 accountId, uint64 itemGuid) const;
    uint32 GetSharedBankItemCount(uint64 accountId) const;
    bool CanAddItemToSharedBank(uint64 accountId, uint32 sharedBankCapacity) const;

    void Clear();

private:
    AccountBankMgr() = default;

    BankStore const* Store(uint64 accountId) const;
    BankStore* Store(uint64 accountId);
    void QueueQuery(GameSession& session, QueryCallback&& query);
    void QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work);

    std::unordered_map<uint64, BankStore> _banks;
    std::unordered_set<uint64> _loaded;
};

#define sAccountBankMgr AccountBankMgr::Instance()

#endif
