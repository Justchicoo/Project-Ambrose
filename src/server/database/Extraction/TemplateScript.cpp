/*
 * Project Ambrose by Imjustchico
 * Lays extracted templates out for the world tables in id order, replacing object_template first so its adjectives, behaviors and item rows are only ever written after it, an item template's own fields going to item_template, its requirement lists, each requirement and each equip effect to the tables beside it with each requirement and effect written as a BINd of its own beside typed columns for the fields its class has, and an item set bonus template's stacking, each tier's item count, description and requirement list, and each requirement and equip effect a tier holds, the same way, to item_set_bonus and the tables beside it, with NULL for a visual id or object type a template does not have and for the name of a behavior of a class nothing describes. Replacing every table whole makes a second run over the same install write the same rows.
 */

#include "TemplateScript.h"
#include "BindFile.h"
#include "PropertyObject.h"

#include <initializer_list>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace
{
    constexpr uint64 EquipList = 0;
    constexpr uint64 PurchaseList = 1;

    template<typename T>
    WorldSqlScript::Value Optional(std::optional<T> const& value)
    {
        if (!value)
            return std::monostate{};
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, double>)
            return *value;
        else
            return int64{ *value };
    }

    WorldSqlScript::Value Serialized(ItemTemplatePart const& part)
    {
        EncodeResult const encoded = BindFile::Write(part.Object.get());
        return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
    }

    WorldSqlScript::Row RequirementRow(WorldSqlScript::Row row, ItemTemplatePart const& part)
    {
        row.insert(row.end(), { uint64{ part.ClassHash }, part.ClassName, Optional(part.NumericValue), Optional(part.OperatorType), Optional(part.MagicSchool),
            Optional(part.Quantity), Optional(part.ItemTemplateId), Optional(part.Adjective), Serialized(part) });
        return row;
    }

    WorldSqlScript::Row EffectRow(WorldSqlScript::Row row, ItemTemplatePart const& part)
    {
        row.insert(row.end(), { uint64{ part.ClassHash }, part.ClassName, Optional(part.EffectName), Optional(part.LookupIndex), Optional(part.PipsGiven),
            Optional(part.PowerPipsGiven), Optional(part.SpellName), Optional(part.NumSpells), Optional(part.SpeedMultiplier), Optional(part.TriggerName), Serialized(part) });
        return row;
    }

    std::vector<std::string_view> RequirementColumns(std::initializer_list<std::string_view> keys)
    {
        std::vector<std::string_view> columns(keys);
        columns.insert(columns.end(), { "class_hash", "class_name", "numeric_value", "operator_type", "magic_school", "quantity", "item_template_id", "adjective", "data" });
        return columns;
    }

    std::vector<std::string_view> EffectColumns(std::initializer_list<std::string_view> keys)
    {
        std::vector<std::string_view> columns(keys);
        columns.insert(columns.end(), { "class_hash", "class_name", "effect_name", "lookup_index", "pips_given", "power_pips_given", "spell_name", "num_spells",
            "speed_multiplier", "trigger_name", "data" });
        return columns;
    }
}

WorldSqlScript TemplateScript::Build(TemplateExtraction const& extraction)
{
    auto const optional = [](auto const& value)
    {
        return value ? WorldSqlScript::Value{ static_cast<int64>(*value) } : WorldSqlScript::Value{ std::monostate{} };
    };
    std::vector<WorldSqlScript::Row> templates;
    std::vector<WorldSqlScript::Row> adjectives;
    std::vector<WorldSqlScript::Row> behaviors;
    std::vector<WorldSqlScript::Row> items;
    std::vector<WorldSqlScript::Row> requirementLists;
    std::vector<WorldSqlScript::Row> requirements;
    std::vector<WorldSqlScript::Row> effects;
    std::vector<WorldSqlScript::Row> setBonuses;
    std::vector<WorldSqlScript::Row> setTiers;
    std::vector<WorldSqlScript::Row> setRequirements;
    std::vector<WorldSqlScript::Row> setEffects;
    templates.reserve(extraction.Templates.size());
    adjectives.reserve(extraction.GetAdjectiveCount());
    behaviors.reserve(extraction.GetBehaviorCount());
    for (ExtractedTemplate const& found : extraction.Templates)
    {
        uint64 const id = found.TemplateId;
        templates.push_back({ id, found.ClassName, uint64{ found.ClassHash }, found.Archive, found.Path, found.ObjectName, found.DisplayKey, found.DescriptionKey, found.Icon,
            optional(found.VisualId), optional(found.ObjectType), found.LootTable });
        for (std::size_t position = 0; position < found.Adjectives.size(); ++position)
            adjectives.push_back({ id, uint64{ position }, found.Adjectives[position] });
        for (std::size_t position = 0; position < found.Behaviors.size(); ++position)
        {
            ExtractedBehavior const& behavior = found.Behaviors[position];
            behaviors.push_back({ id, uint64{ position }, uint64{ behavior.ClassHash },
                behavior.Name ? WorldSqlScript::Value{ *behavior.Name } : WorldSqlScript::Value{ std::monostate{} } });
        }
        if (ItemTemplateRecord const* const item = found.Item ? &*found.Item : nullptr)
        {
            items.push_back({ id, item->School, double{ item->BaseCost }, int64{ item->Rank }, int64{ item->ItemLimit }, uint64{ item->ItemSetBonusTemplateId },
                item->NumPrimaryColors ? WorldSqlScript::Value{ *item->NumPrimaryColors } : WorldSqlScript::Value{ std::monostate{} },
                item->NumSecondaryColors ? WorldSqlScript::Value{ *item->NumSecondaryColors } : WorldSqlScript::Value{ std::monostate{} } });
            for (auto const& [list, kind] : { std::pair{ &item->EquipRequirements, EquipList }, std::pair{ &item->PurchaseRequirements, PurchaseList } })
            {
                if (!*list)
                    continue;
                requirementLists.push_back({ id, kind, uint64{ (*list)->ApplyNot }, (*list)->Operator });
                for (std::size_t position = 0; position < (*list)->Requirements.size(); ++position)
                    requirements.push_back(RequirementRow({ id, kind, uint64{ position } }, (*list)->Requirements[position]));
            }
            for (std::size_t position = 0; position < item->EquipEffects.size(); ++position)
                effects.push_back(EffectRow({ id, uint64{ position } }, item->EquipEffects[position]));
        }
        if (ItemSetBonusRecord const* const set = found.SetBonus ? &*found.SetBonus : nullptr)
        {
            setBonuses.push_back({ id, set->NoStacking ? WorldSqlScript::Value{ uint64{ *set->NoStacking } } : WorldSqlScript::Value{ std::monostate{} },
                uint64{ set->Tiers.size() } });
            for (std::size_t tier = 0; tier < set->Tiers.size(); ++tier)
            {
                ItemSetBonusTier const& bonus = set->Tiers[tier];
                setTiers.push_back({ id, uint64{ tier }, int64{ bonus.NumItemsToEquip }, bonus.Description,
                    bonus.Requirements ? WorldSqlScript::Value{ uint64{ bonus.Requirements->ApplyNot } } : WorldSqlScript::Value{ std::monostate{} },
                    bonus.Requirements ? WorldSqlScript::Value{ bonus.Requirements->Operator } : WorldSqlScript::Value{ std::monostate{} } });
                if (bonus.Requirements)
                    for (std::size_t position = 0; position < bonus.Requirements->Requirements.size(); ++position)
                        setRequirements.push_back(RequirementRow({ id, uint64{ tier }, uint64{ position } }, bonus.Requirements->Requirements[position]));
                for (std::size_t position = 0; position < bonus.Effects.size(); ++position)
                    setEffects.push_back(EffectRow({ id, uint64{ tier }, uint64{ position } }, bonus.Effects[position]));
            }
        }
    }

    std::vector<std::string_view> const tables = GetTables();
    WorldSqlScript script;
    script.ReplaceTable(tables[0], { "template_id", "class_name", "class_hash", "archive", "source_path", "object_name", "display_key", "description_key", "icon", "visual_id",
        "object_type", "loot_table" }, templates);
    script.ReplaceTable(tables[1], { "template_id", "position", "adjective" }, adjectives);
    script.ReplaceTable(tables[2], { "template_id", "position", "class_hash", "behavior_name" }, behaviors);
    script.ReplaceTable(tables[3], { "template_id", "school", "base_cost", "item_rank", "item_limit", "item_set_bonus_template_id", "num_primary_colors", "num_secondary_colors" },
        items);
    script.ReplaceTable(tables[4], { "template_id", "list", "apply_not", "operator" }, requirementLists);
    script.ReplaceTable(tables[5], RequirementColumns({ "template_id", "list", "position" }), requirements);
    script.ReplaceTable(tables[6], EffectColumns({ "template_id", "position" }), effects);
    script.ReplaceTable(tables[7], { "template_id", "no_stacking", "tier_count" }, setBonuses);
    script.ReplaceTable(tables[8], { "template_id", "tier", "num_items_to_equip", "description_key", "apply_not", "operator" }, setTiers);
    script.ReplaceTable(tables[9], RequirementColumns({ "template_id", "tier", "position" }), setRequirements);
    script.ReplaceTable(tables[10], EffectColumns({ "template_id", "tier", "position" }), setEffects);
    return script;
}

std::vector<std::string_view> TemplateScript::GetTables()
{
    return { "object_template", "object_template_adjective", "object_template_behavior", "item_template", "item_template_requirement_list",
        "item_template_requirement", "item_template_effect", "item_set_bonus", "item_set_bonus_tier", "item_set_bonus_requirement", "item_set_bonus_effect" };
}
