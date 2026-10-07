/*
 * Project Ambrose by Imjustchico
 * Makes the loot list from the catalog's own classes and defaults: each item's m_lootType is the option of ItemLootInfo's loot type enum whose name ends in ITEM, read from the type dump rather than written as a number, and a dump with no such option refuses the build, naming it, rather than sending a loot line of the wrong kind.
 */

#include "LootListBuilder.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "PropertyFiller.h"
#include "TypeInfo.h"

#include <fmt/format.h>

#include <cctype>
#include <utility>

namespace
{
    std::optional<int64> ItemLootType(ClassInfo const& itemClass)
    {
        PropertyInfo const* const property = itemClass.FindProperty("m_lootType");
        if (!property)
            return std::nullopt;
        for (EnumOption const& option : property->Options)
        {
            std::string_view const name = option.Name;
            if (name.size() < 4)
                continue;
            std::string_view const tail = name.substr(name.size() - 4);
            bool matches = true;
            for (std::size_t index = 0; index < 4; ++index)
                matches = matches && std::toupper(static_cast<unsigned char>(tail[index])) == "ITEM"[index];
            if (matches)
                return option.Value;
        }
        return std::nullopt;
    }
}

PropertyObjectPtr LootListBuilder::Build(TypeCatalogPtr const& catalog, std::vector<LootItem> const& items, std::string& problem)
{
    problem.clear();
    if (!catalog)
    {
        problem = "no type dump is loaded";
        return nullptr;
    }
    PropertyObjectPtr list = PropertyObject::Create(catalog, ListClass);
    ClassInfo const* const itemClass = catalog->FindClass(ItemClass);
    if (!list || !itemClass)
    {
        problem = fmt::format("the type dump has no {}", !list ? ListClass : ItemClass);
        return nullptr;
    }
    std::optional<int64> const lootType = ItemLootType(*itemClass);
    if (!lootType)
    {
        problem = fmt::format("{}'s m_lootType has no option that names an item", ItemClass);
        return nullptr;
    }
    PropertyValue::List loot;
    loot.reserve(items.size());
    for (LootItem const& item : items)
    {
        PropertyObjectPtr entry = PropertyObject::Create(catalog, ItemClass);
        if (!entry)
        {
            problem = fmt::format("the type dump has no {}", ItemClass);
            return nullptr;
        }
        PropertyFiller(*entry, problem)
            .Set("m_lootType", int64{ *lootType })
            .Set("m_itemID", uint64{ item.TemplateId })
            .Set("m_numItems", static_cast<int32>(item.Quantity));
        if (!problem.empty())
            return nullptr;
        loot.emplace_back(std::move(entry));
    }
    PropertyFiller(*list, problem).Set("m_loot", std::move(loot));
    if (!problem.empty())
        return nullptr;
    return list;
}

std::optional<std::string> LootListBuilder::Encode(std::string_view message, PropertyObject const& list, std::string& problem)
{
    ObjectField const* const field = ObjectFields::Find(message, LootField);
    if (!field)
    {
        problem = fmt::format("no field describes the {} of {}", LootField, message);
        return std::nullopt;
    }
    EncodeResult const encoded = ObjectSerializer::EncodeField(*field, &list);
    if (!encoded.Ok())
    {
        problem = fmt::format("the loot list does not encode: {}", encoded.Detail);
        return std::nullopt;
    }
    return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
}
