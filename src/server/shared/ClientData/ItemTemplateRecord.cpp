/*
 * Project Ambrose by Imjustchico
 * Reads an item template through the game object and wizard item template views after checking every class its decode skipped: one inside its behaviors is counted, any other refuses the item. Each requirement and equip effect is kept as a copy of the object it decoded to, so the record outlives the decode, and its memory is counted as that object's values; the fields the item tables give columns to, an effect's name, stat lookup, pips, spell and speed and a requirement's value, comparison, school, quantity, item and adjective, are read from it by name wherever its class has them. An item set bonus template is checked the same way and read by name, each bonus it grants kept as a copy of its object. A color count, like every such field, is read by name as whichever integer the dump stores it as, and is absent when the class has no such property.
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
        take.template operator()<uint64>();
        return found;
    }

    std::optional<double> RealOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        if (!value)
            return std::nullopt;
        if (float const* const stored = value->GetIf<float>())
            return double{ *stored };
        if (double const* const stored = value->GetIf<double>())
            return *stored;
        return std::nullopt;
    }

    std::optional<std::string> TextOf(PropertyObject const& object, std::string_view name)
    {
        PropertyValue const* const value = object.Get(name);
        std::string const* const text = value ? value->GetIf<std::string>() : nullptr;
        return text ? std::optional<std::string>(*text) : std::nullopt;
    }

    ItemTemplatePart PartOf(PropertyObject const& object)
    {
        ItemTemplatePart part;
        part.ClassName = object.GetClass().Name;
        part.ClassHash = object.GetClass().Hash;
        part.Object = std::shared_ptr<PropertyObject const>(object.Clone());
        part.EffectName = TextOf(object, "m_effectName");
        part.LookupIndex = IntegerOf(object, "m_lookupIndex");
        part.PipsGiven = IntegerOf(object, "m_pipsGiven");
        part.PowerPipsGiven = IntegerOf(object, "m_powerPipsGiven");
        part.SpellName = TextOf(object, "m_spellName");
        part.NumSpells = IntegerOf(object, "m_numSpells");
        part.SpeedMultiplier = IntegerOf(object, "m_speedMultiplier");
        part.NumericValue = RealOf(object, "m_numericValue");
        part.OperatorType = IntegerOf(object, "m_operatorType");
        part.MagicSchool = TextOf(object, "m_magicSchool");
        if (!part.MagicSchool)
            part.MagicSchool = TextOf(object, "m_magicSchoolName");
        part.Quantity = IntegerOf(object, "m_quantity");
        part.ItemTemplateId = IntegerOf(object, "m_templateID");
        part.Adjective = TextOf(object, "m_adjective");
        return part;
    }

    std::vector<ItemTemplatePart> PartsOf(PropertyValue::List const& list)
    {
        std::vector<ItemTemplatePart> parts;
        parts.reserve(list.size());
        for (PropertyValue const& value : list)
            if (PropertyObject const* const object = value.AsObject())
                parts.push_back(PartOf(*object));
        return parts;
    }

    std::optional<ItemRequirementList> RequirementsOf(PropertyObject const* object)
    {
        std::optional<RequirementListView> const list = RequirementListView::From(object);
        if (!list)
            return std::nullopt;
        return ItemRequirementList{ list->AppliesNot(), list->GetOperator(), PartsOf(list->GetRequirements()) };
    }

    std::size_t PartMemory(std::vector<ItemTemplatePart> const& parts)
    {
        std::size_t bytes = parts.capacity() * sizeof(ItemTemplatePart);
        for (ItemTemplatePart const& part : parts)
            bytes += part.ClassName.capacity() + (part.Object ? sizeof(PropertyObject) + part.Object->GetClass().Properties.size() * sizeof(PropertyValue) : 0);
        return bytes;
    }

    bool CountIssues(PropertyObject const& object, std::string_view file, std::vector<DecodeIssue> const& issues, std::size_t& unknownBehaviors, std::string& error)
    {
        std::string const behaviors = fmt::format("{}.m_behaviors[", object.GetClass().Name);
        for (DecodeIssue const& issue : issues)
        {
            if (issue.Kind != DecodeIssueKind::UnknownClass)
                continue;
            if (issue.Path.starts_with(behaviors))
            {
                ++unknownBehaviors;
                continue;
            }
            error = fmt::format("{} holds an object of class hash {} at {}, which the type dump does not list", file, issue.Hash, issue.Path);
            return false;
        }
        return true;
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
    if (!CountIssues(object, file, issues, record.UnknownBehaviors, error))
        return std::nullopt;

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
    record.EquipRequirements = RequirementsOf(item->GetEquipRequirements());
    record.PurchaseRequirements = RequirementsOf(item->GetPurchaseRequirements());
    record.EquipEffects = PartsOf(item->GetEquipEffects());
    return record;
}

std::size_t ItemTemplateRecord::GetMemoryUsage() const noexcept
{
    std::size_t bytes = sizeof(ItemTemplateRecord) + ClassName.capacity() + File.capacity() + ObjectName.capacity() + DisplayKey.capacity() + School.capacity()
        + Adjectives.capacity() * sizeof(std::string);
    for (std::string const& adjective : Adjectives)
        bytes += adjective.capacity();
    for (std::optional<ItemRequirementList> const* list : { &EquipRequirements, &PurchaseRequirements })
        if (*list)
            bytes += PartMemory((*list)->Requirements);
    return bytes + PartMemory(EquipEffects);
}

bool ItemSetBonusRecord::IsSetBonus(PropertyObject const& object) noexcept
{
    return object.IsA(SetBonusClass);
}

std::optional<ItemSetBonusRecord> ItemSetBonusRecord::Read(PropertyObject const& object, uint32 templateId, std::string file, std::vector<DecodeIssue> const& issues,
    std::string& error)
{
    ItemSetBonusRecord record;
    if (!CountIssues(object, file, issues, record.UnknownBehaviors, error))
        return std::nullopt;
    record.TemplateId = templateId;
    record.File = std::move(file);
    record.ObjectName = TextOf(object, "m_objectName").value_or(std::string());
    record.DisplayKey = TextOf(object, "m_displayName").value_or(std::string());
    if (PropertyValue const* const stacking = object.Get("m_noStacking"))
        if (bool const* const value = stacking->GetIf<bool>())
            record.NoStacking = *value;
    if (PropertyValue const* const bonuses = object.Get("m_itemSetBonusDataList"))
        if (PropertyValue::List const* const list = bonuses->GetIf<PropertyValue::List>())
            record.Bonuses = PartsOf(*list);
    return record;
}
