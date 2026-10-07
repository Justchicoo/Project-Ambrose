/*
 * Project Ambrose by Imjustchico
 * Builds the game object an item in a backpack is shown to the client as: the CoreObject header its template's class gives, an object of the class that core type builds, the item's own global id and its template's id, and encodes it into the SerializedItem field of the message that carries it, in the form that field's ObjectFields entry gives; and fills the m_itemList of the player object's inventory behavior with the objects of the items its backpack holds, an item whose template the server no longer holds left out and named; and sets the m_numItemsAllowed of that behavior, the capacity the client shows and holds the backpack to, which the server chooses because no file of the install gives one.
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

    ItemObjectBuilder() = delete;

    static PropertyObjectPtr Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, ItemTemplateRecord const& itemTemplate, CharacterItem const& item,
        std::string& problem);
    static bool FillBackpack(PropertyObject& player, CoreObjectTypeTable const& types, ItemTemplateStore const& templates, std::vector<CharacterItem> const& items,
        std::vector<uint64>& missing, std::string& problem);
    static bool SetItemsAllowed(PropertyObject& player, uint32 capacity, std::string& problem);
    static std::optional<std::string> Encode(std::string_view message, PropertyObject const& object, CoreObjectTypeTable const& types, std::string& problem);
};

#endif
