/*
 * Project Ambrose by Imjustchico
 * A spell as the server uses it, read once from the SpellTemplate the user's install holds through its typed views: its name, which the client hashes into its template id, its school, type, accuracy, costs and flags, the pips its rank takes of each school, and its effects as a tree, since a random, conditional, variable or shadow effect holds the effects it chooses among and a conditional one holds each behind a requirement. An effect keeps its type, target and disposition as the numbers the client's enums give them, whose names the type dump holds, and a spell is described in lines naming them through the catalog in use.
 */

#ifndef AMBROSE_SPELLINFO_H
#define AMBROSE_SPELLINFO_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

class PropertyObject;
class TypeCatalog;

struct SpellPips
{
    uint8 Rank = 0;
    uint8 Balance = 0;
    uint8 Death = 0;
    uint8 Fire = 0;
    uint8 Ice = 0;
    uint8 Life = 0;
    uint8 Myth = 0;
    uint8 Storm = 0;
    uint8 Shadow = 0;
    bool X = false;
};

struct SpellEffectInfo
{
    std::string Class;
    int64 Type = 0;
    int32 Param = 0;
    int64 Disposition = 0;
    std::string DamageType;
    uint32 DamageTypeId = 0;
    int32 PipNum = 0;
    int32 ActNum = 0;
    int64 Target = 0;
    int32 NumRounds = 0;
    int32 ParamPerRound = 0;
    float HealModifier = 1.0f;
    uint32 SpellTemplateId = 0;
    uint32 EnchantmentSpellTemplateId = 0;
    int32 ArmorPiercing = 0;
    int32 ChancePerTarget = 0;
    int32 Rank = 0;
    bool Cloaked = false;
    bool Conditional = false;
    std::vector<SpellEffectInfo> Effects;

    static std::optional<SpellEffectInfo> Read(PropertyObject const& object, std::string& error);
};

struct SpellInfo
{
    static constexpr std::size_t MaxEffectDepth = 16;

    uint32 TemplateId = 0;
    std::string Class;
    std::string File;
    std::string Name;
    std::string DisplayKey;
    std::string DescriptionKey;
    std::string Base;
    std::string School;
    std::string SecondarySchool;
    std::string RequiredSchool;
    std::string TypeName;
    int32 Accuracy = 0;
    int32 TrainingCost = 0;
    int32 LevelRestriction = 0;
    int64 SourceType = 0;
    bool Treasure = false;
    bool PvP = false;
    bool PvE = false;
    SpellPips Pips;
    std::vector<SpellEffectInfo> Effects;

    static std::optional<SpellInfo> Read(PropertyObject const& object, uint32 templateId, std::string file, std::string& error);
    std::vector<std::string> Describe(TypeCatalog const* catalog) const;
    std::size_t CountEffects() const noexcept;
};

#endif
