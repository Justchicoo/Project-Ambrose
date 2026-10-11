/*
 * Project Ambrose by Imjustchico
 * Answers the bank messages for the shared bank of an account: loading it as the account's first wizard enters, taking a withdrawal back into the withdrawing wizard's backpack, deleting the shared reagent or pet snack its client names, counting it and relaying the live limit, each storing or restoring the item's rows and telling the client with the confirm of the message that asked.
 */

#include "AccountBankMgr.h"

#include "BankMgr.h"
#include "BankStore.h"
#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "GameSession.h"
#include "Log.h"
#include "QueryCallback.h"
#include "Settings.h"

#include <fmt/format.h>

namespace
{
    constexpr char const* BankLog = "server.bank";

    std::vector<CharacterItem> ReadSharedBankItems(PreparedQueryResult result)
    {
        std::vector<CharacterItem> items;
        if (!result)
            return items;
        do
        {
            Field const* const row = result->Fetch();
            if (row[0].Get<uint32>() == 0)
                continue;
            CharacterItem item;
            item.Guid = row[1].Get<uint64>();
            item.TemplateId = row[2].Get<uint32>();
            item.Quantity = row[3].Get<uint32>();
            item.PrimaryColor = row[4].Get<uint8>();
            item.SecondaryColor = row[5].Get<uint8>();
            item.Pattern = row[6].Get<uint8>();
            item.Locked = row[7].Get<uint8>() != 0;
            item.Flags = row[8].Get<uint32>();
            item.Created = row[9].Get<uint64>();
            item.Slot = row[10].Get<uint32>();
            items.push_back(item);
        } while (result->NextRow());
        return items;
    }
}

AccountBankMgr& AccountBankMgr::Instance()
{
    static AccountBankMgr instance;
    return instance;
}

void AccountBankMgr::LoadSharedBank(GameSession& session, uint64 accountId, std::function<void(bool)> completion)
{
    if (_loaded.contains(accountId))
    {
        completion(true);
        return;
    }
    if (!CharacterDatabase.IsOpen())
    {
        LOG_WARN(BankLog, "Account {} holds an empty shared bank this session, since the characters database is not open", accountId);
        _banks[accountId] = BankStore{};
        _loaded.insert(accountId);
        completion(true);
        return;
    }
    CharacterRepository::Statement statement = CharacterRepository::PrepareLoadSharedBank(accountId);
    if (!statement)
    {
        LOG_ERROR(BankLog, "Could not prepare loading the shared bank of account {}", accountId);
        completion(false);
        return;
    }
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(statement), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, accountId, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        std::vector<CharacterItem> stored = ReadSharedBankItems(std::move(result));
        QueueWorldWork(*owner, [this, accountId, stored = std::move(stored), completion = std::move(completion)](GameSession&) mutable
        {
            if (!_loaded.contains(accountId))
            {
                _banks[accountId] = BankStore::FromStored(std::move(stored));
                _loaded.insert(accountId);
            }
            completion(true);
        });
    }));
}

void AccountBankMgr::SendSharedBankContents(GameSession& session)
{
    uint64 const accountId = session.GetAccountId();
    if (accountId == 0)
        return;
    LoadSharedBank(session, accountId, [this, &session](bool loaded)
    {
        if (!loaded || !session.IsOpen())
            return;
        SendSharedBankCount(session);
    });
}

bool AccountBankMgr::AddItem(uint64 accountId, CharacterItem const& item)
{
    if (accountId == 0 || item.Guid == 0)
        return false;
    BankStore* const store = Store(accountId);
    uint32 const capacity = sSettings.Get<uint32>("Bank.SharedCapacity");
    if (!store || !store->CanAdd(capacity))
        return false;
    CharacterItem const shared = store->Put(item, capacity);
    CharacterRepository::Statement statement;
    if (CharacterDatabase.IsOpen())
        statement = CharacterRepository::PrepareShareItem(accountId, item.Guid, shared.Slot);
    if (!statement)
    {
        LOG_ERROR(BankLog, "Could not write item {} to the shared bank of account {}, since the characters database is not open", item.Guid, accountId);
        store->PutBack(shared);
        return false;
    }
    CharacterDatabase.Execute(std::move(statement));
    return true;
}

void AccountBankMgr::MoveItemToSharedBank(GameSession& session, GameMessages::MoveInvToBank const& message)
{
    uint64 const accountId = session.GetAccountId();
    if (!session.GetBackpack())
    {
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::NotInWorld);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const* const held = session.GetBackpack()->Find(message.GlobalID);
    if (!held)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s deposit of item {} to the shared bank, since the wizard does not hold that item", session.GetSessionId(), session.GetCharacterId(), message.GlobalID);
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::NotOwned);
        session.SendDmlMessage(confirm);
        return;
    }
    uint32 const capacity = sSettings.Get<uint32>("Bank.SharedCapacity");
    if (!CanAddItemToSharedBank(accountId, capacity))
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s deposit of item {} to the shared bank, since the shared bank holds {} of {} item(s)", session.GetSessionId(), session.GetCharacterId(),
            message.GlobalID, GetSharedBankItemCount(accountId), capacity);
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::BankFull);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const shared = *held;
    if (!session._backpack->Remove(message.GlobalID))
    {
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    if (!AddItem(accountId, shared))
    {
        session._backpack->Put(shared);
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterRepository::Statement unhold;
    if (CharacterDatabase.IsOpen())
        unhold = CharacterRepository::PrepareUnholdItem(session.GetCharacterId(), message.GlobalID);
    if (unhold)
        CharacterDatabase.Execute(std::move(unhold));
    else
        LOG_ERROR(BankLog, "Could not take item {} of wizard {} out of the backpack, since the characters database is not open", message.GlobalID, session.GetCharacterId());
    session.SendItemRemoved(message.GlobalID);
    GameMessages::StorageClientAdd added;
    added.GlobalID = shared.Guid;
    added.SharedBank = 1;
    session.SendDmlMessage(added);
    GameMessages::InvToBankConfirm confirm;
    confirm.ClientRequestID = message.ClientRequestID;
    confirm.OriginalGlobalID = message.GlobalID;
    confirm.GlobalID = shared.Guid;
    confirm.UseShared = message.UseShared;
    confirm.Failure = 0;
    session.SendDmlMessage(confirm);
    LOG_INFO(BankLog, "Session {} deposited item {} of wizard {} to the shared bank of account {} in slot {}", session.GetSessionId(), session.GetCharacterId(), message.GlobalID, accountId, shared.Slot);
}

void AccountBankMgr::MoveItemFromSharedBank(GameSession& session, GameMessages::MoveBankToInv const& message)
{
    uint64 const accountId = session.GetAccountId();
    BankStore* const store = Store(accountId);
    CharacterItem const* const stored = store ? store->Find(message.GlobalID) : nullptr;
    if (!stored)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s withdrawal of item {} from the shared bank, since the shared bank does not hold that item", session.GetSessionId(),
            session.GetCharacterId(), message.GlobalID);
        GameMessages::BankToInvConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.Failure = static_cast<int8>(BankMoveFailure::NotOwned);
        session.SendDmlMessage(confirm);
        return;
    }
    if (!session.GetBackpack())
    {
        GameMessages::BankToInvConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.Failure = static_cast<int8>(BankMoveFailure::NotInWorld);
        session.SendDmlMessage(confirm);
        return;
    }
    uint32 const capacity = session.GetBackpackCapacity();
    if (session.GetBackpack()->Size() >= capacity)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s withdrawal of item {} from the shared bank, since the backpack holds {} of {} item(s)", session.GetSessionId(),
            session.GetCharacterId(), message.GlobalID, session.GetBackpack()->Size(), capacity);
        GameMessages::BankToInvConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.Failure = static_cast<int8>(BankMoveFailure::InventoryFull);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const withdrawn = *stored;
    if (!store->Remove(message.GlobalID))
        return;
    CharacterRepository::Statement unshare;
    if (CharacterDatabase.IsOpen())
        unshare = CharacterRepository::PrepareUnshareItem(accountId, message.GlobalID);
    if (!unshare)
    {
        store->PutBack(withdrawn);
        LOG_ERROR(BankLog, "Could not take item {} out of the shared bank of account {}, since the characters database is not open", message.GlobalID, accountId);
        GameMessages::BankToInvConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.Failure = static_cast<int8>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const restored = session._backpack->Put(withdrawn);
    CharacterRepository::Statement restore;
    if (CharacterDatabase.IsOpen())
        restore = CharacterRepository::PrepareRestoreItem(session.GetCharacterId(), restored);
    if (!restore)
    {
        session._backpack->Remove(message.GlobalID);
        store->PutBack(withdrawn);
        LOG_ERROR(BankLog, "Could not write item {} into wizard {}'s backpack, since the characters database is not open", message.GlobalID, session.GetCharacterId());
        GameMessages::BankToInvConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.Failure = static_cast<int8>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterDatabase.Execute(std::move(unshare));
    CharacterDatabase.Execute(std::move(restore));
    CharacterRepository::Statement transfer;
    if (CharacterDatabase.IsOpen())
        transfer = CharacterRepository::PrepareTransferItem(session.GetCharacterId(), message.GlobalID);
    if (transfer)
        CharacterDatabase.Execute(std::move(transfer));
    else
        LOG_ERROR(BankLog, "Could not give wizard {} the item {}, since the characters database is not open", session.GetCharacterId(), message.GlobalID);
    session.SendItemAdded(restored);
    GameMessages::StorageClientRemove removed;
    removed.GlobalID = withdrawn.Guid;
    removed.SharedBank = 1;
    removed.ItemID = withdrawn.Guid;
    session.SendDmlMessage(removed);
    GameMessages::BankToInvConfirm confirm;
    confirm.ClientRequestID = message.ClientRequestID;
    confirm.OriginalGlobalID = message.GlobalID;
    confirm.GlobalID = restored.Guid;
    confirm.Failure = 0;
    session.SendDmlMessage(confirm);
    LOG_INFO(BankLog, "Session {} withdrew item {} of the shared bank of account {} into wizard {}'s backpack slot {}", session.GetSessionId(), message.GlobalID, accountId,
        session.GetCharacterId(), restored.Slot);
}

void AccountBankMgr::MoveItemToBank(GameSession& session, GameMessages::MoveBankToBank const& message)
{
    uint64 const accountId = session.GetAccountId();
    BankStore* const store = Store(accountId);
    CharacterItem const* const stored = store ? store->Find(message.BankSharedID) : nullptr;
    if (!stored)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s move of item {} from the shared bank, since the shared bank does not hold that item", session.GetSessionId(), session.GetCharacterId(),
            message.BankSharedID);
        GameMessages::BankToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.BankSharedID;
        confirm.GlobalID = message.BankSharedID;
        confirm.UseShared = 0;
        confirm.Error = static_cast<uint32>(BankMoveFailure::NotOwned);
        session.SendDmlMessage(confirm);
        return;
    }
    uint32 const capacity = sSettings.Get<uint32>("Bank.Capacity");
    if (!sBankMgr.CanAddItemToBank(session.GetCharacterId(), capacity))
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s move of item {} from the shared bank, since the bank holds {} of {} item(s)", session.GetSessionId(), session.GetCharacterId(),
            message.BankSharedID, sBankMgr.GetBankItemCount(session.GetCharacterId()), capacity);
        GameMessages::BankToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.BankSharedID;
        confirm.GlobalID = message.BankSharedID;
        confirm.UseShared = 0;
        confirm.Error = static_cast<uint32>(BankMoveFailure::BankFull);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const banked = *stored;
    if (!store->Remove(message.BankSharedID))
        return;
    CharacterRepository::Statement unshare;
    if (CharacterDatabase.IsOpen())
        unshare = CharacterRepository::PrepareUnshareItem(accountId, message.BankSharedID);
    if (!unshare)
    {
        store->PutBack(banked);
        LOG_ERROR(BankLog, "Could not take item {} out of the shared bank of account {}, since the characters database is not open", message.BankSharedID, accountId);
        GameMessages::BankToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.BankSharedID;
        confirm.GlobalID = message.BankSharedID;
        confirm.UseShared = 0;
        confirm.Error = static_cast<uint32>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    if (!sBankMgr.AddItem(session.GetCharacterId(), banked))
    {
        store->PutBack(banked);
        GameMessages::BankToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.BankSharedID;
        confirm.GlobalID = message.BankSharedID;
        confirm.UseShared = 0;
        confirm.Error = static_cast<uint32>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterDatabase.Execute(std::move(unshare));
    GameMessages::BankToBankConfirm confirm;
    confirm.ClientRequestID = message.ClientRequestID;
    confirm.OriginalGlobalID = message.BankSharedID;
    confirm.GlobalID = banked.Guid;
    confirm.UseShared = 0;
    confirm.Error = 0;
    session.SendDmlMessage(confirm);
    LOG_INFO(BankLog, "Session {} moved item {} of the shared bank of account {} to wizard {}'s bank", session.GetSessionId(), message.BankSharedID, accountId, session.GetCharacterId());
}

void AccountBankMgr::DeleteItemFromSharedBank(GameSession& session, GameMessages::SharedBankDeleteReagentOrPetSnack const& message)
{
    uint64 const accountId = session.GetAccountId();
    BankStore* const store = Store(accountId);
    CharacterItem const* const stored = store ? store->Find(message.GlobalID) : nullptr;
    if (!stored)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s deletion of item {}, since the shared bank does not hold that item", session.GetSessionId(), session.GetCharacterId(), message.GlobalID);
        GameMessages::SharedBankDeleteReagentOrPetSnackConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.GlobalID = message.GlobalID;
        confirm.Failure = static_cast<int8>(BankMoveFailure::NotOwned);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const deleted = *stored;
    if (!store->Remove(message.GlobalID))
        return;
    CharacterRepository::Statement unshare;
    if (CharacterDatabase.IsOpen())
        unshare = CharacterRepository::PrepareUnshareItem(accountId, message.GlobalID);
    if (!unshare)
    {
        store->PutBack(deleted);
        LOG_ERROR(BankLog, "Could not take item {} out of the shared bank of account {}, since the characters database is not open", message.GlobalID, accountId);
        GameMessages::SharedBankDeleteReagentOrPetSnackConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.GlobalID = message.GlobalID;
        confirm.Failure = static_cast<int8>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterDatabase.Execute(std::move(unshare));
    GameMessages::SharedBankDeleteReagentOrPetSnackConfirm confirm;
    confirm.ClientRequestID = message.ClientRequestID;
    confirm.GlobalID = message.GlobalID;
    confirm.Failure = 0;
    session.SendDmlMessage(confirm);
    LOG_INFO(BankLog, "Session {} deleted item {} of template {} from the shared bank of account {}", session.GetSessionId(), deleted.Guid, deleted.TemplateId, accountId);
}

void AccountBankMgr::SendSharedBankCount(GameSession& session)
{
    GameMessages::BankCount response;
    response.Count = static_cast<uint32>(GetSharedBankItemCount(session.GetAccountId()));
    session.SendDmlMessage(response);
}

bool AccountBankMgr::IsItemInSharedBank(uint64 accountId, uint64 itemGuid) const
{
    auto const* const store = Store(accountId);
    return store && store->Holds(itemGuid);
}

uint32 AccountBankMgr::GetSharedBankItemCount(uint64 accountId) const
{
    auto const* const store = Store(accountId);
    return store ? static_cast<uint32>(store->Size()) : 0;
}

bool AccountBankMgr::CanAddItemToSharedBank(uint64 accountId, uint32 sharedBankCapacity) const
{
    auto const* const store = Store(accountId);
    return store && store->CanAdd(sharedBankCapacity);
}

void AccountBankMgr::Clear()
{
    _banks.clear();
    _loaded.clear();
}

BankStore const* AccountBankMgr::Store(uint64 accountId) const
{
    auto const it = _banks.find(accountId);
    return it == _banks.end() ? nullptr : &it->second;
}

BankStore* AccountBankMgr::Store(uint64 accountId)
{
    auto it = _banks.find(accountId);
    return it == _banks.end() ? nullptr : &it->second;
}

void AccountBankMgr::QueueQuery(GameSession& session, QueryCallback&& query)
{
    session._queryCallbacks.AddCallback(std::move(query));
    session._queryCallbacks.ProcessReadyCallbacks();
}

void AccountBankMgr::QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work)
{
    std::shared_ptr<GameSession> const owner = session.SharedSelf();
    if (!owner->IsOpen())
        return;
    if (!owner->QueueInbound([owner, work = std::move(work)]() mutable
    {
        if (owner->IsOpen())
            work(*owner);
    }))
    {
        if (owner->IsOpen())
            LOG_ERROR(BankLog, "Could not queue database work for session {}", owner->GetSessionId());
    }
}
