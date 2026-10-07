/*
 * Project Ambrose by Imjustchico
 * The typed record of one item template the user's install holds, any template whose class is or derives from WizItemTemplate: its id, class and file, its object name, display key, object type and adjectives as a game object template carries them, and its school, base cost, rank, item limit, set bonus and color counts, and its equip and purchase requirement lists and equip effects, each requirement and effect kept as the object it decoded to with its class and the fields the item tables give columns to wherever its class has them. A template whose fields hold an object of a class the type dump does not list is refused, naming the class hash and where it sits, while one in its behaviors keeps its place and is counted, as every template's behaviors are; and the record of an item set bonus template, its names, whether it stacks and each tier of bonuses with how many of the set's items it needs, its description key, its requirements and the equip effects it grants, kept as the objects they decoded to, refused the same way.
 */

#ifndef AMBROSE_ITEMTEMPLATERECORD_H
#define AMBROSE_ITEMTEMPLATERECORD_H

#include "Types.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PropertyObject;
struct DecodeIssue;

struct ItemTemplatePart
{
    std::string ClassName;
    uint32 ClassHash = 0;
    std::shared_ptr<PropertyObject const> Object;
    std::optional<std::string> EffectName;
    std::optional<int64> LookupIndex;
    std::optional<int64> PipsGiven;
    std::optional<int64> PowerPipsGiven;
    std::optional<std::string> SpellName;
    std::optional<int64> NumSpells;
    std::optional<std::string> TriggerName;
    std::optional<int64> SpeedMultiplier;
    std::optional<double> NumericValue;
    std::optional<int64> OperatorType;
    std::optional<std::string> MagicSchool;
    std::optional<int64> Quantity;
    std::optional<int64> ItemTemplateId;
    std::optional<std::string> Adjective;
};

struct ItemRequirementList
{
    bool ApplyNot = false;
    int64 Operator = 0;
    std::vector<ItemTemplatePart> Requirements;
};

struct ItemTemplateRecord
{
    static constexpr std::string_view ItemClass = "class WizItemTemplate";

    uint32 TemplateId = 0;
    std::string ClassName;
    std::string File;
    std::string ObjectName;
    std::string DisplayKey;
    std::optional<int64> ObjectType;
    std::vector<std::string> Adjectives;
    std::string School;
    float BaseCost = 0.0f;
    int32 Rank = 0;
    int32 ItemLimit = 0;
    uint32 ItemSetBonusTemplateId = 0;
    std::optional<int64> NumPrimaryColors;
    std::optional<int64> NumSecondaryColors;
    std::optional<ItemRequirementList> EquipRequirements;
    std::optional<ItemRequirementList> PurchaseRequirements;
    std::vector<ItemTemplatePart> EquipEffects;
    std::size_t UnknownBehaviors = 0;

    static bool IsItem(PropertyObject const& object) noexcept;
    static std::optional<ItemTemplateRecord> Read(PropertyObject const& object, uint32 templateId, std::string file, std::vector<DecodeIssue> const& issues, std::string& error);

    std::size_t GetMemoryUsage() const noexcept;
};

struct ItemSetBonusTier
{
    int32 NumItemsToEquip = 0;
    std::string Description;
    std::optional<ItemRequirementList> Requirements;
    std::vector<ItemTemplatePart> Effects;
};

struct ItemSetBonusRecord
{
    static constexpr std::string_view SetBonusClass = "class ItemSetBonusTemplate";

    uint32 TemplateId = 0;
    std::string File;
    std::string ObjectName;
    std::string DisplayKey;
    std::optional<bool> NoStacking;
    std::vector<ItemSetBonusTier> Tiers;
    std::size_t UnknownBehaviors = 0;

    static bool IsSetBonus(PropertyObject const& object) noexcept;
    static std::optional<ItemSetBonusRecord> Read(PropertyObject const& object, uint32 templateId, std::string file, std::vector<DecodeIssue> const& issues, std::string& error);
};

#endif
