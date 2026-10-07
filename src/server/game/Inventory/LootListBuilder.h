/*
 * Project Ambrose by Imjustchico
 * Builds the LootInfoList MSG_LOOT shows a wizard, one ItemLootInfo for each item it was given with the item's template id and how many, and encodes it into the message's LootList field, for every system that hands out loot: commands now, combat and quests later.
 */

#ifndef AMBROSE_LOOTLISTBUILDER_H
#define AMBROSE_LOOTLISTBUILDER_H

#include "PropertyObject.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct LootItem
{
    uint32 TemplateId = 0;
    uint32 Quantity = 1;
};

class LootListBuilder
{
public:
    static constexpr std::string_view ListClass = "class LootInfoList";
    static constexpr std::string_view ItemClass = "class ItemLootInfo";
    static constexpr std::string_view LootField = "LootList";

    LootListBuilder() = delete;

    static PropertyObjectPtr Build(TypeCatalogPtr const& catalog, std::vector<LootItem> const& items, std::string& problem);
    static std::optional<std::string> Encode(std::string_view message, PropertyObject const& list, std::string& problem);
};

#endif
