/*
 * Project Ambrose by Imjustchico
 * Answers the bank messages for the personal bank: loading it as the wizard enters, and on each move taking the item from or giving it back to the backpack, handing it to or taking it from the shared bank, deleting it, counting it, relaying the live limit and selling it, each storing or restoring the item's rows and telling the client with the confirm of the message that asked.
 */

#include "BankMgr.h"

#include "AccountBankMgr.h"
#include "BankStore.h"
#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "GameSession.h"
#include "Log.h"
#include "QueryCallback.h"
#include "Settings.h"

#include <chrono>
#include <fmt/format.h>

namespace
{
    constexpr char const* BankLog = "server.bank";

    uint64 NowSeconds()
    {
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    std::vector<CharacterItem> ReadBankItems(PreparedQueryResult result)
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

    std::string_view MoveFailureName(BankMoveFailure failure)
    {
        switch (failure)
        {
            case BankMoveFailure::NotOwned:
                return "the wizard does not hold that item";
            case BankMoveFailure::BankFull:
                return "the bank is full";
            case BankMoveFailure::InventoryFull:
                return "the backpack is full";
            case BankMoveFailure::Locked:
                return "the item is locked";
            case BankMoveFailure::NotInWorld:
                return "the wizard is not in the world";
            default:
                return "the bank could not store the change";
        }
    }
}

BankMgr& BankMgr::Instance()
{
    static BankMgr instance;
    return instance;
}

void BankMgr::LoadBank(GameSession& session, uint64 characterId, std::function<void(bool)> completion)
{
    if (_loaded.contains(characterId))
    {
        completion(true);
        return;
    }
    if (!CharacterDatabase.IsOpen())
    {
        LOG_WARN(BankLog, "Wizard {} holds an empty bank this session, since the characters database is not open", characterId);
        _banks[characterId] = BankStore{};
        _loaded.insert(characterId);
        completion(true);
        return;
    }
    CharacterRepository::Statement statement = CharacterRepository::PrepareLoadBank(characterId);
    if (!statement)
    {
        LOG_ERROR(BankLog, "Could not prepare loading the bank of wizard {}", characterId);
        completion(false);
        return;
    }
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(statement), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, characterId, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        std::vector<CharacterItem> stored = ReadBankItems(std::move(result));
        QueueWorldWork(*owner, [this, characterId, stored = std::move(stored), completion = std::move(completion)](GameSession&) mutable
        {
            if (!_loaded.contains(characterId))
            {
                _banks[characterId] = BankStore::FromStored(std::move(stored));
                _loaded.insert(characterId);
            }
            completion(true);
        });
    }));
}

void BankMgr::SendBankContents(GameSession& session, std::function<void(bool)> completion)
{
    uint64 const characterId = session.GetCharacterId();
    if (characterId == 0)
    {
        if (completion)
            completion(false);
        return;
    }
    LoadBank(session, characterId, [this, characterId, &session, completion = std::move(completion)](bool loaded) mutable
    {
        if (!loaded || !session.IsOpen())
        {
            if (completion)
                completion(false);
            return;
        }
        SendBankCount(session);
        if (completion)
            completion(true);
    });
}

void BankMgr::MoveItemToBank(GameSession& session, GameMessages::MoveInvToBank const& message)
{
    if (message.UseShared != 0)
    {
        sAccountBankMgr.MoveItemToSharedBank(session, message);
        return;
    }
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
        LOG_WARN(BankLog, "Session {} refused wizard {}'s deposit of item {}, since the wizard does not hold that item", session.GetSessionId(), session.GetCharacterId(), message.GlobalID);
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::NotOwned);
        session.SendDmlMessage(confirm);
        return;
    }
    if (held->Locked)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s deposit of item {}, since the item is locked", session.GetSessionId(), session.GetCharacterId(), message.GlobalID);
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::Locked);
        session.SendDmlMessage(confirm);
        return;
    }
    uint32 const capacity = sSettings.Get<uint32>("Bank.Capacity");
    if (!CanAddItemToBank(session.GetCharacterId(), capacity))
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s deposit of item {}, since the bank holds {} of {} item(s)", session.GetSessionId(), session.GetCharacterId(), message.GlobalID,
            GetBankItemCount(session.GetCharacterId()), capacity);
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::BankFull);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const banked = Store(session.GetCharacterId())->Put(*held, capacity);
    if (!session._backpack->Remove(message.GlobalID))
    {
        Store(session.GetCharacterId())->Remove(message.GlobalID);
        GameMessages::InvToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterRepository::Statement statement;
    if (CharacterDatabase.IsOpen())
        statement = CharacterRepository::PrepareBankItem(session.GetCharacterId(), banked.Guid, banked.Slot);
    if (statement)
        CharacterDatabase.Execute(std::move(statement));
    else
        LOG_ERROR(BankLog, "Could not write item {} of wizard {} to the bank, since the characters database is not open", banked.Guid, session.GetCharacterId());
    CharacterRepository::Statement unhold;
    if (CharacterDatabase.IsOpen())
        unhold = CharacterRepository::PrepareUnholdItem(session.GetCharacterId(), message.GlobalID);
    if (unhold)
        CharacterDatabase.Execute(std::move(unhold));
    else
        LOG_ERROR(BankLog, "Could not take item {} of wizard {} out of the backpack, since the characters database is not open", message.GlobalID, session.GetCharacterId());
    session.SendItemRemoved(message.GlobalID);
    GameMessages::StorageClientAdd added;
    added.GlobalID = banked.Guid;
    added.SharedBank = 0;
    session.SendDmlMessage(added);
    GameMessages::InvToBankConfirm confirm;
    confirm.ClientRequestID = message.ClientRequestID;
    confirm.OriginalGlobalID = message.GlobalID;
    confirm.GlobalID = banked.Guid;
    confirm.UseShared = message.UseShared;
    confirm.Failure = 0;
    session.SendDmlMessage(confirm);
    LOG_INFO(BankLog, "Session {} banked item {} of wizard {} in slot {}, and the bank holds {} of {} item(s)", session.GetSessionId(), session.GetCharacterId(), message.GlobalID,
        banked.Slot, GetBankItemCount(session.GetCharacterId()), capacity);
}

void BankMgr::MoveItemToInventory(GameSession& session, GameMessages::MoveBankToInv const& message)
{
    if (message.UseShared != 0)
    {
        sAccountBankMgr.MoveItemFromSharedBank(session, message);
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
    BankStore* const store = Store(session.GetCharacterId());
    CharacterItem const* const stored = store ? store->Find(message.GlobalID) : nullptr;
    if (!stored)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s withdrawal of item {}, since the bank does not hold that item", session.GetSessionId(), session.GetCharacterId(), message.GlobalID);
        GameMessages::BankToInvConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.GlobalID;
        confirm.GlobalID = message.GlobalID;
        confirm.Failure = static_cast<int8>(BankMoveFailure::NotOwned);
        session.SendDmlMessage(confirm);
        return;
    }
    uint32 const capacity = session.GetBackpackCapacity();
    if (session.GetBackpack()->Size() >= capacity)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s withdrawal of item {}, since the backpack holds {} of {} item(s)", session.GetSessionId(), session.GetCharacterId(), message.GlobalID,
            session.GetBackpack()->Size(), capacity);
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
    CharacterItem const restored = session._backpack->Put(withdrawn);
    CharacterRepository::Statement statement;
    if (CharacterDatabase.IsOpen())
        statement = CharacterRepository::PrepareUnbankItem(session.GetCharacterId(), message.GlobalID);
    if (statement)
        CharacterDatabase.Execute(std::move(statement));
    else
        LOG_ERROR(BankLog, "Could not take item {} of wizard {} out of the bank, since the characters database is not open", message.GlobalID, session.GetCharacterId());
    CharacterRepository::Statement restore;
    if (CharacterDatabase.IsOpen())
        restore = CharacterRepository::PrepareRestoreItem(session.GetCharacterId(), restored);
    if (restore)
        CharacterDatabase.Execute(std::move(restore));
    else
        LOG_ERROR(BankLog, "Could not write item {} of wizard {} back to the backpack, since the characters database is not open", message.GlobalID, session.GetCharacterId());
    session.SendItemAdded(restored);
    GameMessages::StorageClientRemove removed;
    removed.GlobalID = withdrawn.Guid;
    removed.SharedBank = 0;
    removed.ItemID = withdrawn.Guid;
    session.SendDmlMessage(removed);
    GameMessages::BankToInvConfirm confirm;
    confirm.ClientRequestID = message.ClientRequestID;
    confirm.OriginalGlobalID = message.GlobalID;
    confirm.GlobalID = restored.Guid;
    confirm.Failure = 0;
    session.SendDmlMessage(confirm);
    LOG_INFO(BankLog, "Session {} withdrew item {} of wizard {} into backpack slot {}, and the bank holds {} item(s)", session.GetSessionId(), session.GetCharacterId(), message.GlobalID,
        restored.Slot, GetBankItemCount(session.GetCharacterId()));
}

void BankMgr::MoveItemToSharedBank(GameSession& session, GameMessages::MoveBankToBank const& message)
{
    if (message.BankSharedID != 0)
    {
        sAccountBankMgr.MoveItemToBank(session, message);
        return;
    }
    auto const accountId = session.GetAccountId();
    BankStore* const store = Store(session.GetCharacterId());
    CharacterItem const* const stored = store ? store->Find(message.BankID) : nullptr;
    if (!stored)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s move of item {} to the shared bank, since the bank does not hold that item", session.GetSessionId(), session.GetCharacterId(), message.BankID);
        GameMessages::BankToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.BankID;
        confirm.GlobalID = message.BankID;
        confirm.UseShared = 1;
        confirm.Error = static_cast<uint32>(BankMoveFailure::NotOwned);
        session.SendDmlMessage(confirm);
        return;
    }
    uint32 const capacity = sSettings.Get<uint32>("Bank.SharedCapacity");
    if (!sAccountBankMgr.CanAddItemToSharedBank(accountId, capacity))
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s move of item {} to the shared bank, since the shared bank holds {} of {} item(s)", session.GetSessionId(), session.GetCharacterId(),
            message.BankID, sAccountBankMgr.GetSharedBankItemCount(accountId), capacity);
        GameMessages::BankToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.BankID;
        confirm.GlobalID = message.BankID;
        confirm.UseShared = 1;
        confirm.Error = static_cast<uint32>(BankMoveFailure::BankFull);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const shared = *stored;
    if (!store->Remove(message.BankID))
        return;
    if (!sAccountBankMgr.AddItem(accountId, shared))
    {
        store->PutBack(shared);
        GameMessages::BankToBankConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.OriginalGlobalID = message.BankID;
        confirm.GlobalID = message.BankID;
        confirm.UseShared = 1;
        confirm.Error = static_cast<uint32>(BankMoveFailure::InternalError);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterRepository::Statement statement;
    if (CharacterDatabase.IsOpen())
        statement = CharacterRepository::PrepareUnbankItem(session.GetCharacterId(), message.BankID);
    if (statement)
        CharacterDatabase.Execute(std::move(statement));
    else
        LOG_ERROR(BankLog, "Could not take item {} of wizard {} out of the bank, since the characters database is not open", message.BankID, session.GetCharacterId());
    GameMessages::BankToBankConfirm confirm;
    confirm.ClientRequestID = message.ClientRequestID;
    confirm.OriginalGlobalID = message.BankID;
    confirm.GlobalID = shared.Guid;
    confirm.UseShared = 1;
    confirm.Error = 0;
    session.SendDmlMessage(confirm);
    LOG_INFO(BankLog, "Session {} moved wizard {}'s item {} to the shared bank of account {} in slot {}", session.GetSessionId(), session.GetCharacterId(), message.BankID, accountId, shared.Slot);
}

void BankMgr::DeleteItemFromBank(GameSession& session, GameMessages::BankDelete const& message)
{
    if (message.UseShared != 0)
    {
        GameMessages::SharedBankDeleteReagentOrPetSnack shared;
        shared.ClientRequestID = message.ClientRequestID;
        shared.GlobalID = message.GlobalID;
        shared.Quantity = 1;
        sAccountBankMgr.DeleteItemFromSharedBank(session, shared);
        return;
    }
    BankStore* const store = Store(session.GetCharacterId());
    CharacterItem const* const stored = store ? store->Find(message.GlobalID) : nullptr;
    if (!stored)
    {
        LOG_WARN(BankLog, "Session {} refused wizard {}'s deletion of item {}, since the bank does not hold that item", session.GetSessionId(), session.GetCharacterId(), message.GlobalID);
        GameMessages::BankDeleteConfirm confirm;
        confirm.ClientRequestID = message.ClientRequestID;
        confirm.GlobalID = message.GlobalID;
        confirm.UseShared = message.UseShared;
        confirm.Failure = static_cast<int8>(BankMoveFailure::NotOwned);
        session.SendDmlMessage(confirm);
        return;
    }
    CharacterItem const deleted = *stored;
    if (!store->Remove(message.GlobalID))
        return;
    CharacterRepository::Statement statement;
    if (CharacterDatabase.IsOpen())
        statement = CharacterRepository::PrepareUnbankItem(session.GetCharacterId(), message.GlobalID);
    if (statement)
        CharacterDatabase.Execute(std::move(statement));
    else
        LOG_ERROR(BankLog, "Could not take item {} of wizard {} out of the bank, since the characters database is not open", message.GlobalID, session.GetCharacterId());
    CharacterRepository::Statement trash;
    if (CharacterDatabase.IsOpen())
        trash = CharacterRepository::PrepareTrashItem(session.GetCharacterId(), message.GlobalID);
    if (trash)
        CharacterDatabase.Execute(std::move(trash));
    else
        LOG_ERROR(BankLog, "Could not delete item {} of wizard {}, since the characters database is not open", message.GlobalID, session.GetCharacterId());
    GameMessages::BankDeleteConfirm confirm;
    confirm.ClientRequestID = message.ClientRequestID;
    confirm.GlobalID = message.GlobalID;
    confirm.UseShared = message.UseShared;
    confirm.Failure = 0;
    session.SendDmlMessage(confirm);
    LOG_INFO(BankLog, "Session {} deleted item {} of template {} from wizard {}'s bank", session.GetSessionId(), deleted.Guid, deleted.TemplateId, session.GetCharacterId());
}

void BankMgr::SendBankCount(GameSession& session)
{
    GameMessages::BankCount response;
    response.Count = static_cast<uint32>(GetBankItemCount(session.GetCharacterId()));
    session.SendDmlMessage(response);
}

void BankMgr::HandleQuickSellRequestBank(GameSession& session, GameMessages::QuickSellRequestBank const& message)
{
    LOG_WARN(BankLog, "Session {} asked for a quick sell from the bank, which carries no item to name one; the bank sells nothing yet", session.GetSessionId());
}

bool BankMgr::AddItem(uint64 characterId, CharacterItem const& item)
{
    if (characterId == 0 || item.Guid == 0)
        return false;
    BankStore* const store = Store(characterId);
    uint32 const capacity = sSettings.Get<uint32>("Bank.Capacity");
    if (!store || !store->CanAdd(capacity))
        return false;
    CharacterItem const banked = store->Put(item, capacity);
    CharacterRepository::Statement statement;
    if (CharacterDatabase.IsOpen())
        statement = CharacterRepository::PrepareBankItem(characterId, item.Guid, banked.Slot);
    if (!statement)
    {
        LOG_ERROR(BankLog, "Could not write item {} to the bank of wizard {}, since the characters database is not open", item.Guid, characterId);
        store->PutBack(banked);
        return false;
    }
    CharacterDatabase.Execute(std::move(statement));
    return true;
}

bool BankMgr::IsItemInBank(uint64 characterId, uint64 itemGuid) const
{
    auto const* const store = Store(characterId);
    return store && store->Holds(itemGuid);
}

uint32 BankMgr::GetBankItemCount(uint64 characterId) const
{
    auto const* const store = Store(characterId);
    return store ? static_cast<uint32>(store->Size()) : 0;
}

bool BankMgr::CanAddItemToBank(uint64 characterId, uint32 bankCapacity) const
{
    auto const* const store = Store(characterId);
    return store && store->CanAdd(bankCapacity);
}

void BankMgr::Clear()
{
    _banks.clear();
    _loaded.clear();
}

BankStore const* BankMgr::Store(uint64 characterId) const
{
    auto const it = _banks.find(characterId);
    return it == _banks.end() ? nullptr : &it->second;
}

BankStore* BankMgr::Store(uint64 characterId)
{
    auto it = _banks.find(characterId);
    return it == _banks.end() ? nullptr : &it->second;
}

void BankMgr::QueueQuery(GameSession& session, QueryCallback&& query)
{
    session._queryCallbacks.AddCallback(std::move(query));
    session._queryCallbacks.ProcessReadyCallbacks();
}

void BankMgr::QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work)
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
