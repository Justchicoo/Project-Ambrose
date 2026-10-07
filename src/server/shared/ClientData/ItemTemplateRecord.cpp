/*
 * Project Ambrose by Imjustchico
 * Reads an item template through the game object and wizard item template views after checking every class its decode skipped: one inside its behaviors is counted, any other refuses the item. A color count is read by name as whichever integer the dump stores it as, and is absent when the class has no such property.
 */

#include "ItemTemplateRecord.h"
#include "ObjectSerializer.h"
#include "ObjectViews.h"
#include "PropertyObject.h"

#include <fmt/format.h>

#include <utility>

namespace
{
    std::optional<int64> IntegerOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        if (!value)
            return std::nullopt;
        std::optional<int64> found;
        auto const take = [value, &found]<typename Stored>()
        {
            if (Stored const* const stored = value->GetIf<Stored>())
                found = static_cast<int64>(*stored);
        };
        take.template operator()<int8>();
        take.template operator()<uint8>();
        take.template operator()<int16>();
        take.template operator()<uint16>();
        take.template operator()<int32>();
        take.template operator()<uint32>();
        take.template operator()<int64>();
        return found;
    }
}

bool ItemTemplateRecord::IsItem(PropertyObject const& object) noexcept
{
    return object.IsA(ItemClass);
}

std::optional<ItemTemplateRecord> ItemTemplateRecord::Read(PropertyObject const& object, uint32 templateId, std::string file, std::vector<DecodeIssue> const& issues,
    std::string& error)
{
    std::optional<GameObjectTemplateView> const game = GameObjectTemplateView::From(object);
    std::optional<WizItemTemplateView> const item = WizItemTemplateView::From(object);
    if (!game || !item)
    {
        error = fmt::format("{} is a {}, which does not read as a {}", file, object.GetClass().Name, ItemClass);
        return std::nullopt;
    }

    ItemTemplateRecord record;
    std::string const behaviors = fmt::format("{}.m_behaviors[", object.GetClass().Name);
    for (DecodeIssue const& issue : issues)
    {
        if (issue.Kind != DecodeIssueKind::UnknownClass)
            continue;
        if (issue.Path.starts_with(behaviors))
        {
            ++record.UnknownBehaviors;
            continue;
        }
        error = fmt::format("{} holds an object of class hash {} at {}, which the type dump does not list", file, issue.Hash, issue.Path);
        return std::nullopt;
    }

    record.TemplateId = templateId;
    record.ClassName = object.GetClass().Name;
    record.File = std::move(file);
    record.ObjectName = game->GetObjectName();
    record.DisplayKey = game->GetDisplayName();
    record.ObjectType = game->GetObjectType();
    for (PropertyValue const& adjective : game->GetAdjectiveList())
        if (std::string const* const text = adjective.GetIf<std::string>())
            record.Adjectives.push_back(*text);
    record.School = item->GetSchool();
    record.BaseCost = item->GetBaseCost();
    record.Rank = item->GetRank();
    record.ItemLimit = item->GetItemLimit();
    record.ItemSetBonusTemplateId = item->GetItemSetBonusTemplateId();
    record.NumPrimaryColors = IntegerOf(object, "m_numPrimaryColors");
    record.NumSecondaryColors = IntegerOf(object, "m_numSecondaryColors");
    return record;
}

std::size_t ItemTemplateRecord::GetMemoryUsage() const noexcept
{
    std::size_t bytes = sizeof(ItemTemplateRecord) + ClassName.capacity() + File.capacity() + ObjectName.capacity() + DisplayKey.capacity() + School.capacity()
        + Adjectives.capacity() * sizeof(std::string);
    for (std::string const& adjective : Adjectives)
        bytes += adjective.capacity();
    return bytes;
}
