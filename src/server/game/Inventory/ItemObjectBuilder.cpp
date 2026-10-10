/*
 * Project Ambrose by Imjustchico
 * Makes an item's game object from the catalog's own class and defaults, refusing a template class the catalog or core_template_type does not know rather than guessing a core type, sets only its header, global id, template id and, where the class has one, the pattern word whose top bit the client reads as the lock, and encodes it through the message field ObjectFields declares for it, so the envelope and the CoreObject form are the field's, not this file's; the backpack's objects go to the player's behavior that has both an m_itemList and an m_numItemsAllowed, since the equipment behavior carries an m_itemList too, and a list the class refuses fails the fill whole, leaving the player object as it was; the capacity goes to whichever of them has an m_numItemsAllowed, ClientWizInventoryBehavior in the type dump. The worn items go to the behavior with an m_itemList and no m_numItemsAllowed, ClientWizEquipmentBehavior, as the same item objects the backpack holds, since the client loads both lists and MSG_EQUIPMENTBEHAVIOR_EQUIPITEM's SerializedItem with the reader MSG_INVENTORYBEHAVIOR_ADDITEM's uses; m_slotList and m_publicItemList are filled only where the class has them, a slot entry's m_itemSlotNameID is the KI string id of the slot's name, which no capture has confirmed yet, and a public entry carries only the item's template id, as EquippedItemInfo's one property is an unsigned int; the public entry is encoded on its own through the SerializedInfo field ObjectFields declares.
 */

#include "ItemObjectBuilder.h"
#include "ObjectFields.h"
#include "PlayerBackpack.h"
#include "ObjectSerializer.h"
#include "PropertyFiller.h"
#include "StringHash.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <span>

PropertyObjectPtr ItemObjectBuilder::Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, ItemTemplateRecord const& itemTemplate, CharacterItem const& item,
    std::string& problem)
{
    problem.clear();
    if (!catalog)
    {
        problem = "no type dump is loaded";
        return nullptr;
    }
    ClassInfo const* const templateClass = catalog->FindClass(itemTemplate.ClassName);
    if (!templateClass)
    {
        problem = fmt::format("the type dump has no template class {}", itemTemplate.ClassName);
        return nullptr;
    }
    std::optional<CoreObjectHeader> const header = types.HeaderFor(*templateClass, itemTemplate.TemplateId);
    if (!header)
    {
        problem = fmt::format("core_template_type gives the item template class {} no core type, so the client could not create item {} from it", itemTemplate.ClassName,
            itemTemplate.TemplateId);
        return nullptr;
    }
    std::span<CoreObjectType const> const built = types.GetTypes();
    auto const type = std::find_if(built.begin(), built.end(), [&header](CoreObjectType const& entry) { return entry.CoreType == header->Block; });
    if (type == built.end())
    {
        problem = fmt::format("core_object_type does not say which class core type {} builds", header->Block);
        return nullptr;
    }
    PropertyObjectPtr object = PropertyObject::Create(catalog, type->ClassName);
    if (!object)
    {
        problem = fmt::format("the type dump has no property class {}", type->ClassName);
        return nullptr;
    }
    object->SetCoreHeader(*header);
    PropertyFiller(*object, problem)
        .Set("m_globalID.m_full", item.Guid)
        .Set("m_permID", uint64{ 0 })
        .Set("m_templateID.m_full", uint64{ itemTemplate.TemplateId });
    if (!problem.empty())
        return nullptr;
    if (object->GetClass().FindProperty(PatternProperty))
        PropertyFiller(*object, problem).Set(PatternProperty, static_cast<int32>(PlayerBackpack::LockWord(item)));
    if (!problem.empty())
        return nullptr;
    return object;
}

bool ItemObjectBuilder::FillBackpack(PropertyObject& player, CoreObjectTypeTable const& types, ItemTemplateStore const& templates, std::vector<CharacterItem> const& items,
    std::vector<uint64>& missing, std::string& problem)
{
    problem.clear();
    PropertyValue const* const held = player.Get("m_inactiveBehaviors");
    PropertyValue::List const* const current = held ? held->GetList() : nullptr;
    if (!current)
    {
        problem = fmt::format("{} has no behavior list", player.GetClass().Name);
        return false;
    }
    PropertyValue::List behaviors = *current;
    PropertyObject* const inventory = FindBackpack(behaviors);
    if (!inventory)
    {
        problem = fmt::format("no behavior of the player object carries {} and {}", ItemListProperty, ItemsAllowedProperty);
        return false;
    }
    PropertyValue::List list;
    list.reserve(items.size());
    for (CharacterItem const& item : items)
    {
        ItemTemplateRecord const* const itemTemplate = templates.Find(item.TemplateId);
        if (!itemTemplate)
        {
            missing.push_back(item.Guid);
            continue;
        }
        PropertyObjectPtr object = Build(player.GetCatalog(), types, *itemTemplate, item, problem);
        if (!object)
            return false;
        list.emplace_back(std::move(object));
    }
    PropertyFiller(*inventory, problem).Set(ItemListProperty, std::move(list));
    PropertyFiller(player, problem).Set("m_inactiveBehaviors", std::move(behaviors));
    return problem.empty();
}

PropertyObject* ItemObjectBuilder::FindBackpack(PropertyValue::List& behaviors)
{
    for (PropertyValue& entry : behaviors)
        if (PropertyObject* const behavior = entry.AsObject();
            behavior && behavior->GetClass().FindProperty(ItemListProperty) && behavior->GetClass().FindProperty(ItemsAllowedProperty))
            return behavior;
    return nullptr;
}

bool ItemObjectBuilder::SetItemsAllowed(PropertyObject& player, uint32 capacity, std::string& problem)
{
    problem.clear();
    PropertyValue const* const held = player.Get("m_inactiveBehaviors");
    PropertyValue::List const* const current = held ? held->GetList() : nullptr;
    if (!current)
    {
        problem = fmt::format("{} has no behavior list", player.GetClass().Name);
        return false;
    }
    PropertyValue::List behaviors = *current;
    PropertyObject* const inventory = FindBackpack(behaviors);
    if (!inventory)
    {
        problem = fmt::format("no behavior of the player object carries {}", ItemsAllowedProperty);
        return false;
    }
    PropertyFiller(*inventory, problem).Set(ItemsAllowedProperty, static_cast<int32>(std::min<uint32>(capacity, std::numeric_limits<int32>::max())));
    PropertyFiller(player, problem).Set("m_inactiveBehaviors", std::move(behaviors));
    return problem.empty();
}

std::optional<std::string> ItemObjectBuilder::Encode(std::string_view message, PropertyObject const& object, CoreObjectTypeTable const& types, std::string& problem)
{
    ObjectField const* const field = ObjectFields::Find(message, SerializedItemField);
    if (!field)
    {
        problem = fmt::format("no field describes the {} of {}", SerializedItemField, message);
        return std::nullopt;
    }
    EncodeResult const encoded = CoreObjectSerializer::EncodeField(*field, object, types);
    if (!encoded.Ok())
    {
        problem = fmt::format("the item does not encode: {}", encoded.Detail);
        return std::nullopt;
    }
    return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
}

bool ItemObjectBuilder::FillEquipment(PropertyObject& player, CoreObjectTypeTable const& types, ItemTemplateStore const& templates, std::vector<CharacterEquippedItem> const& items,
    std::vector<uint64>& missing, std::string& problem)
{
    problem.clear();
    PropertyValue const* const held = player.Get("m_inactiveBehaviors");
    PropertyValue::List const* const current = held ? held->GetList() : nullptr;
    if (!current)
    {
        problem = fmt::format("{} has no behavior list", player.GetClass().Name);
        return false;
    }
    PropertyValue::List behaviors = *current;
    PropertyObject* const equipment = FindEquipment(behaviors);
    if (!equipment)
    {
        problem = fmt::format("no behavior of the player object carries {} without {}", ItemListProperty, ItemsAllowedProperty);
        return false;
    }
    bool const hasSlots = equipment->GetClass().FindProperty(SlotListProperty) != nullptr;
    bool const hasPublic = equipment->GetClass().FindProperty(PublicItemListProperty) != nullptr;
    PropertyValue::List list;
    PropertyValue::List slots;
    PropertyValue::List shown;
    list.reserve(items.size());
    for (CharacterEquippedItem const& worn : items)
    {
        ItemTemplateRecord const* const itemTemplate = templates.Find(worn.Item.TemplateId);
        if (!itemTemplate)
        {
            missing.push_back(worn.Item.Guid);
            continue;
        }
        PropertyObjectPtr object = Build(player.GetCatalog(), types, *itemTemplate, worn.Item, problem);
        if (!object)
            return false;
        list.emplace_back(std::move(object));
        if (hasSlots)
        {
            PropertyObjectPtr slot = PropertyObject::Create(player.GetCatalog(), SlotInfoClass);
            if (!slot)
            {
                problem = fmt::format("the type dump has no {}", SlotInfoClass);
                return false;
            }
            PropertyFiller(*slot, problem).Set("m_itemID", worn.Item.Guid).Set("m_itemSlotNameID", SlotNameId(worn.Slot));
            if (!problem.empty())
                return false;
            slots.emplace_back(std::move(slot));
        }
        if (hasPublic)
        {
            PropertyObjectPtr info = BuildPublicInfo(player.GetCatalog(), worn.Item.TemplateId, problem);
            if (!info)
                return false;
            shown.emplace_back(std::move(info));
        }
    }
    PropertyFiller filler(*equipment, problem);
    filler.Set(ItemListProperty, std::move(list));
    if (hasSlots)
        filler.Set(SlotListProperty, std::move(slots));
    if (hasPublic)
        filler.Set(PublicItemListProperty, std::move(shown));
    if (!problem.empty())
        return false;
    PropertyFiller(player, problem).Set("m_inactiveBehaviors", std::move(behaviors));
    return problem.empty();
}

PropertyObject* ItemObjectBuilder::FindEquipment(PropertyValue::List& behaviors)
{
    for (PropertyValue& entry : behaviors)
        if (PropertyObject* const behavior = entry.AsObject();
            behavior && behavior->GetClass().FindProperty(ItemListProperty) && !behavior->GetClass().FindProperty(ItemsAllowedProperty))
            return behavior;
    return nullptr;
}

PropertyObjectPtr ItemObjectBuilder::BuildPublicInfo(TypeCatalogPtr const& catalog, uint32 templateId, std::string& problem)
{
    PropertyObjectPtr info = catalog ? PropertyObject::Create(catalog, ItemInfoClass) : nullptr;
    if (!info)
    {
        problem = fmt::format("the type dump has no {}", ItemInfoClass);
        return nullptr;
    }
    PropertyFiller(*info, problem).Set("m_itemID", templateId);
    if (!problem.empty())
        return nullptr;
    return info;
}

std::optional<std::string> ItemObjectBuilder::EncodePublicInfo(std::string_view message, PropertyObject const& info, std::string& problem)
{
    ObjectField const* const field = ObjectFields::Find(message, SerializedInfoField);
    if (!field)
    {
        problem = fmt::format("no field describes the {} of {}", SerializedInfoField, message);
        return std::nullopt;
    }
    EncodeResult const encoded = ObjectSerializer::EncodeField(*field, &info);
    if (!encoded.Ok())
    {
        problem = fmt::format("the equipped item's public entry does not encode: {}", encoded.Detail);
        return std::nullopt;
    }
    return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
}

uint32 ItemObjectBuilder::SlotNameId(std::string_view slot) noexcept
{
    return StringHash::StringId(slot);
}
