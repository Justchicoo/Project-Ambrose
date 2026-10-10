/*
 * Project Ambrose by Imjustchico
 * Changes what a wizard wears on the world thread: GAME MSG_EQUIPITEM with IsEquip 1 puts the backpack item it names on in the slot it names, and with IsEquip 0 takes the worn item it names off, judged by the player's equipment slots, the item templates and the wizard's school and level; a change is stored in one transaction, an item put on is shown leaving the backpack with MSG_INVENTORYBEHAVIOR_REMOVEITEM and arriving in its slot with MSG_EQUIPMENTBEHAVIOR_EQUIPITEM, an item taken off or given back by a full slot is shown leaving with MSG_EQUIPMENTBEHAVIOR_UNEQUIPITEM and arriving with MSG_INVENTORYBEHAVIOR_ADDITEM, the public change is queued for the wizards that see it and the object newcomers are shown is encoded again, and PlayerScript hears each item put on and taken off. A refused equip is logged and answered with MSG_EQUIPMENTBEHAVIOR_EQUIPITEM marked not valid, which the client's handler drops without touching its lists, so the client keeps the items where the server has them; a refused unequip is logged, and one refused for a full backpack is answered with MSG_ITEMDROP as a full backpack's add is.
 */

#include "CharacterRepository.h"
#include "GameSession.h"
#include "ItemMgr.h"
#include "ItemObjectBuilder.h"
#include "Log.h"
#include "ObjectFields.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "PlayerLevelMgr.h"
#include "ScriptMgr.h"
#include "StringUtil.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <string>

namespace
{
    constexpr char const* EquipmentLog = "server.gamesession";
    constexpr int32 Valid = 1;
    constexpr int32 NotValid = 0;
    constexpr uint32 ItemDropBackpackFull = 0;
}

EquipResult GameSession::EquipItem(uint64 itemGuid, std::string_view slotName)
{
    if (!_backpack || !_equipment || !_player)
        return EquipResult::NotInWorld;
    std::shared_ptr<ItemTemplateStore const> const templates = sItemMgr.GetItems();
    std::shared_ptr<ObjectTemplate const> const equipmentTemplate = sObjectTemplateMgr.GetPlayerEquipment();
    std::string problem = "the player's template names no equipment template";
    std::optional<EquipmentSlots> const slots = equipmentTemplate && equipmentTemplate->Object ? EquipmentSlots::Read(*equipmentTemplate->Object, problem) : std::nullopt;
    if (!slots || !templates)
    {
        LOG_WARN(EquipmentLog, "Session {} refused wizard {}'s request to equip item {} in slot {}, since {}", GetSessionId(), _worldGuid, itemGuid, Ambrose::ForLog(slotName),
            templates ? problem : std::string("no item templates are loaded"));
        RefuseEquip(itemGuid, slotName);
        return slots ? EquipResult::NoSuchTemplate : EquipResult::NoSuchSlot;
    }
    PlayerStats const& stats = _player->GetStats();
    std::shared_ptr<PlayerLevelSet const> const levels = sPlayerLevelMgr.GetLevels();
    MagicSchool const* const school = levels ? levels->FindSchool(stats.GetSchoolId()) : nullptr;
    EquipWizard const wizard(school ? school->Name : std::string(), stats.GetLevel());
    EquipChange const change = _equipment->Equip(*_backpack, GetBackpackCapacity(), *slots, *templates, wizard, itemGuid, slotName);
    if (change.Result != EquipResult::Equipped || !change.Worn)
    {
        LOG_WARN(EquipmentLog, "Session {} refused wizard {}'s request to equip item {} in slot {}, since {}", GetSessionId(), _worldGuid, itemGuid, Ambrose::ForLog(slotName),
            PlayerEquipment::ResultName(change.Result));
        RefuseEquip(itemGuid, slotName);
        return change.Result;
    }
    CharacterEquippedItem const& worn = *change.Worn;
    SaveEquip(worn.Item, worn.Slot, change.Returned);
    if (change.Returned)
    {
        SendUnequipped(change.Returned->Guid);
        SendItemAdded(*change.Returned);
        _equipmentChanges.push_back({ std::nullopt, static_cast<uint8>(std::min<std::size_t>(change.ReturnedIndex, std::numeric_limits<uint8>::max())) });
        sScriptMgr.OnUnequip(*_player, change.Returned->Guid, change.Returned->TemplateId, worn.Slot);
        LOG_INFO(EquipmentLog, "Session {} took item {} of template {} off wizard {}'s slot {} and put it back in its backpack", GetSessionId(), change.Returned->Guid,
            change.Returned->TemplateId, _worldGuid, worn.Slot);
    }
    SendItemRemoved(worn.Item.Guid);
    SendEquipped(worn);
    QueuePublicEquip(worn.Item.TemplateId);
    RefreshPublicObject();
    sScriptMgr.OnEquip(*_player, worn.Item.Guid, worn.Item.TemplateId, worn.Slot);
    LOG_INFO(EquipmentLog, "Session {} put item {} of template {} on wizard {}'s slot {}, and it wears {} item(s)", GetSessionId(), worn.Item.Guid, worn.Item.TemplateId,
        _worldGuid, worn.Slot, _equipment->Size());
    return change.Result;
}

UnequipResult GameSession::UnequipItem(uint64 itemGuid)
{
    if (!_backpack || !_equipment || !_player)
        return UnequipResult::NotInWorld;
    UnequipChange const change = _equipment->Unequip(*_backpack, GetBackpackCapacity(), itemGuid);
    if (change.Result != UnequipResult::Unequipped || !change.TakenOff || !change.Returned)
    {
        LOG_WARN(EquipmentLog, "Session {} refused wizard {}'s request to unequip item {}, since {}", GetSessionId(), _worldGuid, itemGuid, PlayerEquipment::ResultName(change.Result));
        CharacterEquippedItem const* const worn = change.Result == UnequipResult::BackpackFull ? _equipment->Find(itemGuid) : nullptr;
        if (worn)
        {
            GameMessages::ItemDrop drop;
            drop.TemplateId = worn->Item.TemplateId;
            drop.ErrorId = ItemDropBackpackFull;
            SendDmlMessage(drop);
        }
        return change.Result;
    }
    SaveUnequip(*change.Returned);
    SendUnequipped(itemGuid);
    SendItemAdded(*change.Returned);
    _equipmentChanges.push_back({ std::nullopt, static_cast<uint8>(std::min<std::size_t>(change.Index, std::numeric_limits<uint8>::max())) });
    RefreshPublicObject();
    sScriptMgr.OnUnequip(*_player, itemGuid, change.Returned->TemplateId, change.TakenOff->Slot);
    LOG_INFO(EquipmentLog, "Session {} took item {} of template {} off wizard {}'s slot {}, and it wears {} item(s)", GetSessionId(), itemGuid, change.Returned->TemplateId,
        _worldGuid, change.TakenOff->Slot, _equipment->Size());
    return change.Result;
}

void GameSession::HandleEquipItem(GameMessages::EquipItem& message)
{
    LOG_DEBUG(EquipmentLog, "Session {} asks to {} item {} in slot {}", GetSessionId(), message.IsEquip != 0 ? "equip" : "unequip", message.ItemId,
        Ambrose::ForLog(message.SlotName));
    if (message.IsEquip != 0)
        EquipItem(message.ItemId, message.SlotName);
    else
        UnequipItem(message.ItemId);
}

void GameSession::ShowEquipmentChangeOf(uint64 worldGuid, PublicEquipmentChange const& change)
{
    if (change.SerializedInfo)
    {
        GameMessages::EquipmentBehaviorPublicEquipItem shown;
        shown.GlobalId = worldGuid;
        shown.SerializedInfo = *change.SerializedInfo;
        SendDmlMessage(shown);
        return;
    }
    GameMessages::EquipmentBehaviorPublicUnequipItem hidden;
    hidden.GlobalId = worldGuid;
    hidden.IndexToRemove = change.IndexToRemove;
    SendDmlMessage(hidden);
}

void GameSession::SaveEquip(CharacterItem const& item, std::string const& slot, std::optional<CharacterItem> const& returned)
{
    CharacterRepository::CreateTransaction transaction = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareEquipItem(_worldGuid, item, slot, returned) : nullptr;
    if (!transaction)
    {
        LOG_ERROR(EquipmentLog, "Session {} could not store item {} worn in wizard {}'s slot {}, since the characters database is not open", GetSessionId(), item.Guid, _worldGuid,
            slot);
        return;
    }
    CharacterDatabase.CommitTransaction(std::move(transaction));
}

void GameSession::SaveUnequip(CharacterItem const& item)
{
    CharacterRepository::CreateTransaction transaction = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareUnequipItem(_worldGuid, item) : nullptr;
    if (!transaction)
    {
        LOG_ERROR(EquipmentLog, "Session {} could not store item {} taken off wizard {}, since the characters database is not open", GetSessionId(), item.Guid, _worldGuid);
        return;
    }
    CharacterDatabase.CommitTransaction(std::move(transaction));
}

void GameSession::SendItemAdded(CharacterItem const& item)
{
    std::shared_ptr<ItemTemplateStore const> const templates = sItemMgr.GetItems();
    ItemTemplateRecord const* const itemTemplate = templates ? templates->Find(item.TemplateId) : nullptr;
    if (!itemTemplate)
    {
        LOG_ERROR(EquipmentLog, "Session {} put item {} back in wizard {}'s backpack but cannot show it until the wizard enters again, since the items this server holds do not "
            "name template {}", GetSessionId(), item.Guid, _worldGuid, item.TemplateId);
        return;
    }
    SendItemAdded(*itemTemplate, item);
}

void GameSession::SendEquipped(CharacterEquippedItem const& worn)
{
    CoreObjectTypeTablePtr const types = sObjectSchemaMgr.GetCoreObjectTypes();
    std::shared_ptr<ItemTemplateStore const> const templates = sItemMgr.GetItems();
    ItemTemplateRecord const* const itemTemplate = templates ? templates->Find(worn.Item.TemplateId) : nullptr;
    std::string problem = !types ? std::string("the core object types are not loaded") : std::string("the items this server holds do not name its template");
    PropertyObjectPtr const object = types && itemTemplate ? ItemObjectBuilder::Build(sTypeRegistry.GetCatalog(), *types, *itemTemplate, worn.Item, problem) : nullptr;
    std::optional<std::string> const encoded = object ? ItemObjectBuilder::Encode(GameMessages::EquipmentBehaviorEquipItem::Tag, *object, *types, problem) : std::nullopt;
    if (!encoded)
    {
        LOG_ERROR(EquipmentLog, "Session {} stored item {} worn by wizard {} but cannot show it until the wizard enters again, since {}", GetSessionId(), worn.Item.Guid, _worldGuid,
            problem);
        return;
    }
    GameMessages::EquipmentBehaviorEquipItem equipped;
    equipped.GlobalId = _worldGuid;
    equipped.SlotName = worn.Slot;
    equipped.IsValid = Valid;
    equipped.SerializedItem = *encoded;
    SendDmlMessage(equipped);
}

void GameSession::SendUnequipped(uint64 itemGuid)
{
    GameMessages::EquipmentBehaviorUnequipItem unequipped;
    unequipped.GlobalId = _worldGuid;
    unequipped.ItemId = itemGuid;
    SendDmlMessage(unequipped);
}

void GameSession::RefuseEquip(uint64 itemGuid, std::string_view slotName)
{
    GameMessages::EquipmentBehaviorEquipItem refused;
    refused.GlobalId = _worldGuid;
    refused.SlotName = std::string(slotName);
    refused.IsValid = NotValid;
    SendDmlMessage(refused);
    LOG_DEBUG(EquipmentLog, "Session {} told its client item {} stays where it was", GetSessionId(), itemGuid);
}

void GameSession::QueuePublicEquip(uint32 templateId)
{
    std::string problem;
    PropertyObjectPtr const info = ItemObjectBuilder::BuildPublicInfo(sTypeRegistry.GetCatalog(), templateId, problem);
    std::optional<std::string> const encoded = info ? ItemObjectBuilder::EncodePublicInfo(GameMessages::EquipmentBehaviorPublicEquipItem::Tag, *info, problem) : std::nullopt;
    if (!encoded)
    {
        LOG_WARN(EquipmentLog, "Session {} cannot show the wizards around wizard {} that it wears template {}, since {}", GetSessionId(), _worldGuid, templateId, problem);
        return;
    }
    _equipmentChanges.push_back({ *encoded, 0 });
}

void GameSession::RefreshPublicObject()
{
    CoreObjectTypeTablePtr const types = sObjectSchemaMgr.GetCoreObjectTypes();
    std::shared_ptr<ItemTemplateStore const> const templates = sItemMgr.GetItems();
    if (!_playerObject || !types || !templates || !_equipment || _publicObject.empty())
        return;
    std::vector<uint64> unheld;
    std::string problem;
    if (!ItemObjectBuilder::FillEquipment(*_playerObject, *types, *templates, _equipment->GetItems(), unheld, problem))
    {
        LOG_WARN(EquipmentLog, "Session {} shows wizards arriving near wizard {} what it wore as it entered, since {}", GetSessionId(), _worldGuid, problem);
        return;
    }
    ObjectField const* const field = ObjectFields::Find("MSG_NEWOBJECT", "Data");
    SerializerOptions options;
    options.Mask = SerializerOptions::PublicMask;
    EncodeResult shown = field ? CoreObjectSerializer::EncodeField(*field, *_playerObject, *types, options) : EncodeResult{};
    if (!field || !shown.Ok())
    {
        LOG_WARN(EquipmentLog, "Session {} shows wizards arriving near wizard {} what it wore as it entered, since its object does not encode: {}", GetSessionId(), _worldGuid,
            field ? shown.Detail : std::string("MSG_NEWOBJECT's Data is not declared"));
        return;
    }
    _publicObject = std::move(shown.Bytes);
}
