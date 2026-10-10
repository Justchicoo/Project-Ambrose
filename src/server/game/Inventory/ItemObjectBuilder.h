/*
 * Project Ambrose by Imjustchico
 * Builds the game object an item in a backpack is shown to the client as: the CoreObject header its template's class gives, an object of the class that core type builds, the item's own global id and its template's id, and encodes it into the SerializedItem field of the message that carries it, in the form that field's ObjectFields entry gives; and fills the m_itemList of the player object's inventory behavior with the objects of the items its backpack holds, an item whose template the server no longer holds left out and named; and sets the m_numItemsAllowed of that behavior, the capacity the client shows and holds the backpack to, which the server chooses because no file of the install gives one; and fills the equipment behavior, the one with an m_itemList and no m_numItemsAllowed, with the objects of the items the wizard wears, a slot entry for each naming the item and the string id of its slot, and the public list other wizards see, each entry the worn item's template id, and builds and encodes that public entry on its own for the message that shows one item put on.
 */

#ifndef AMBROSE_ITEMOBJECTBUILDER_H
#define AMBROSE_ITEMOBJECTBUILDER_H

#include "CharacterItem.h"
#include "CoreObjectSerializer.h"
#include "ItemTemplateStore.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ItemObjectBuilder
{
public:
    static constexpr std::string_view SerializedItemField = "SerializedItem";
    static constexpr std::string_view ItemListProperty = "m_itemList";
    static constexpr std::string_view ItemsAllowedProperty = "m_numItemsAllowed";
    static constexpr std::string_view PatternProperty = "m_pattern";
    static constexpr std::string_view SlotListProperty = "m_slotList";
    static constexpr std::string_view PublicItemListProperty = "m_publicItemList";
    static constexpr std::string_view SlotInfoClass = "class EquippedSlotInfo";
    static constexpr std::string_view ItemInfoClass = "class EquippedItemInfo";
    static constexpr std::string_view SerializedInfoField = "SerializedInfo";

    ItemObjectBuilder() = delete;

    static PropertyObjectPtr Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, ItemTemplateRecord const& itemTemplate, CharacterItem const& item,
        std::string& problem);
    static bool FillBackpack(PropertyObject& player, CoreObjectTypeTable const& types, ItemTemplateStore const& templates, std::vector<CharacterItem> const& items,
        std::vector<uint64>& missing, std::string& problem);
    static bool SetItemsAllowed(PropertyObject& player, uint32 capacity, std::string& problem);
    static PropertyObject* FindBackpack(PropertyValue::List& behaviors);
    static bool FillEquipment(PropertyObject& player, CoreObjectTypeTable const& types, ItemTemplateStore const& templates, std::vector<CharacterEquippedItem> const& items,
        std::vector<uint64>& missing, std::string& problem);
    static PropertyObject* FindEquipment(PropertyValue::List& behaviors);
    static PropertyObjectPtr BuildPublicInfo(TypeCatalogPtr const& catalog, uint32 templateId, std::string& problem);
    static std::optional<std::string> EncodePublicInfo(std::string_view message, PropertyObject const& info, std::string& problem);
    static uint32 SlotNameId(std::string_view slot) noexcept;
    static std::optional<std::string> Encode(std::string_view message, PropertyObject const& object, CoreObjectTypeTable const& types, std::string& problem);
};

#endif
