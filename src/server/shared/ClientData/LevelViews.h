/*
 * Project Ambrose by Imjustchico
 * The typed views the level extractor reads the client's own level, school and stat files through: MagicXPConfig with its encounter experience factors, its shared level table, its per-school tables, the level cap and the level each mob rank stands for, each MagicLevelInfo row, each MobRankLevel, each school's ClassInfo, each MagicSchoolTemplate, and the crit, block and pip conversion bands of WizStatisticEffectConfig.
 */

#ifndef AMBROSE_LEVELVIEWS_H
#define AMBROSE_LEVELVIEWS_H

#include "TypedView.h"

#include <array>
#include <cstddef>
#include <string>

class MagicXPConfigView : public TypedView<MagicXPConfigView>
{
public:
    enum Field : std::size_t { EncounterXPFactors, LevelTable, SchoolTables, MaxSchoolLevel, MobRanks, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(EncounterXPFactors, "float", "m_encounterXPFactors"),
        ViewField::Of<PropertyValue::List>(LevelTable, "class MagicLevelInfo*", "m_levelInfo"),
        ViewField::Of<PropertyValue::List>(SchoolTables, "class ClassInfo*", "m_classInfo"),
        ViewField::Of<int32>(MaxSchoolLevel, "int", "m_maxSchoolLevel"),
        ViewField::Of<PropertyValue::List>(MobRanks, "class MobRankLevel*", "m_levelsConfig"),
    } };
    static constexpr ViewDefinition Definition{ "MagicXPConfigView", "class MagicXPConfig", Fields };

    decltype(auto) GetEncounterXPFactors() const noexcept { return Read<EncounterXPFactors>(); }
    decltype(auto) GetLevelTable() const noexcept { return Read<LevelTable>(); }
    decltype(auto) GetSchoolTables() const noexcept { return Read<SchoolTables>(); }
    decltype(auto) GetMaxSchoolLevel() const noexcept { return Read<MaxSchoolLevel>(); }
    decltype(auto) GetMobRanks() const noexcept { return Read<MobRanks>(); }

    AMBROSE_TYPED_VIEW(MagicXPConfigView)
};

class MagicLevelInfoView : public TypedView<MagicLevelInfoView>
{
public:
    enum Field : std::size_t
    {
        Level, XpToLevel, Hitpoints, Mana, Gold, LevelName, PipChance, TrainingPoints, CraftingSlots, PetEnergy, PipConversionAll, PipConversionFire, PipConversionIce,
        PipConversionStorm, PipConversionLife, PipConversionMyth, PipConversionDeath, PipConversionBalance, ShadowPipRating, Archmastery, FieldCount
    };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<int32>(Level, "int", "m_level"),
        ViewField::Of<int32>(XpToLevel, "int", "m_xpToLevel"),
        ViewField::Of<int32>(Hitpoints, "int", "m_hitpoints"),
        ViewField::Of<int32>(Mana, "int", "m_mana"),
        ViewField::Of<int32>(Gold, "int", "m_gold"),
        ViewField::Of<std::string>(LevelName, "std::string", "m_levelName"),
        ViewField::Of<float>(PipChance, "float", "m_pipChance"),
        ViewField::Of<int32>(TrainingPoints, "int", "m_trainingPoints"),
        ViewField::Of<int32>(CraftingSlots, "int", "m_craftingSlots"),
        ViewField::Of<int32>(PetEnergy, "int", "m_petEnergy"),
        ViewField::Of<int32>(PipConversionAll, "int", "m_pipConversionRatingAllSchools"),
        ViewField::Of<int32>(PipConversionFire, "int", "m_pipConversionRatingFire"),
        ViewField::Of<int32>(PipConversionIce, "int", "m_pipConversionRatingIce"),
        ViewField::Of<int32>(PipConversionStorm, "int", "m_pipConversionRatingStorm"),
        ViewField::Of<int32>(PipConversionLife, "int", "m_pipConversionRatingLife"),
        ViewField::Of<int32>(PipConversionMyth, "int", "m_pipConversionRatingMyth"),
        ViewField::Of<int32>(PipConversionDeath, "int", "m_pipConversionRatingDeath"),
        ViewField::Of<int32>(PipConversionBalance, "int", "m_pipConversionRatingBalance"),
        ViewField::Of<float>(ShadowPipRating, "float", "m_shadowPipRating"),
        ViewField::Of<float>(Archmastery, "float", "m_archmastery"),
    } };
    static constexpr ViewDefinition Definition{ "MagicLevelInfoView", "class MagicLevelInfo", Fields };

    decltype(auto) GetLevel() const noexcept { return Read<Level>(); }
    decltype(auto) GetXpToLevel() const noexcept { return Read<XpToLevel>(); }
    decltype(auto) GetHitpoints() const noexcept { return Read<Hitpoints>(); }
    decltype(auto) GetMana() const noexcept { return Read<Mana>(); }
    decltype(auto) GetGold() const noexcept { return Read<Gold>(); }
    decltype(auto) GetLevelName() const noexcept { return Read<LevelName>(); }
    decltype(auto) GetPipChance() const noexcept { return Read<PipChance>(); }
    decltype(auto) GetTrainingPoints() const noexcept { return Read<TrainingPoints>(); }
    decltype(auto) GetCraftingSlots() const noexcept { return Read<CraftingSlots>(); }
    decltype(auto) GetPetEnergy() const noexcept { return Read<PetEnergy>(); }
    decltype(auto) GetPipConversionAll() const noexcept { return Read<PipConversionAll>(); }
    decltype(auto) GetPipConversionFire() const noexcept { return Read<PipConversionFire>(); }
    decltype(auto) GetPipConversionIce() const noexcept { return Read<PipConversionIce>(); }
    decltype(auto) GetPipConversionStorm() const noexcept { return Read<PipConversionStorm>(); }
    decltype(auto) GetPipConversionLife() const noexcept { return Read<PipConversionLife>(); }
    decltype(auto) GetPipConversionMyth() const noexcept { return Read<PipConversionMyth>(); }
    decltype(auto) GetPipConversionDeath() const noexcept { return Read<PipConversionDeath>(); }
    decltype(auto) GetPipConversionBalance() const noexcept { return Read<PipConversionBalance>(); }
    decltype(auto) GetShadowPipRating() const noexcept { return Read<ShadowPipRating>(); }
    decltype(auto) GetArchmastery() const noexcept { return Read<Archmastery>(); }

    AMBROSE_TYPED_VIEW(MagicLevelInfoView)
};

class MobRankLevelView : public TypedView<MobRankLevelView>
{
public:
    enum Field : std::size_t { Rank, Level, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<int32>(Rank, "int", "m_rank"),
        ViewField::Of<int32>(Level, "int", "m_level"),
    } };
    static constexpr ViewDefinition Definition{ "MobRankLevelView", "class MobRankLevel", Fields };

    decltype(auto) GetRank() const noexcept { return Read<Rank>(); }
    decltype(auto) GetLevel() const noexcept { return Read<Level>(); }

    AMBROSE_TYPED_VIEW(MobRankLevelView)
};

class SchoolLevelTableView : public TypedView<SchoolLevelTableView>
{
public:
    enum Field : std::size_t { SchoolName, LevelTable, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(SchoolName, "std::string", "m_className"),
        ViewField::Of<PropertyValue::List>(LevelTable, "class MagicLevelInfo*", "m_classLevelInfo"),
    } };
    static constexpr ViewDefinition Definition{ "SchoolLevelTableView", "class ClassInfo", Fields };

    decltype(auto) GetSchoolName() const noexcept { return Read<SchoolName>(); }
    decltype(auto) GetLevelTable() const noexcept { return Read<LevelTable>(); }

    AMBROSE_TYPED_VIEW(SchoolLevelTableView)
};

class MagicSchoolTemplateView : public TypedView<MagicSchoolTemplateView>
{
public:
    enum Field : std::size_t { Behaviors, SchoolName, MinLevel, SchoolIndex, SecondarySchoolBadges, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(Behaviors, "class BehaviorTemplate*", "m_behaviors"),
        ViewField::Of<std::string>(SchoolName, "std::string", "m_schoolName"),
        ViewField::Of<uint32>(MinLevel, "unsigned int", "m_minLevel"),
        ViewField::Of<int32>(SchoolIndex, "int", "m_schoolIndex"),
        ViewField::Of<PropertyValue::List>(SecondarySchoolBadges, "std::string", "m_secondarySchoolBadgeList"),
    } };
    static constexpr ViewDefinition Definition{ "MagicSchoolTemplateView", "class MagicSchoolTemplate", Fields };

    decltype(auto) GetBehaviors() const noexcept { return Read<Behaviors>(); }
    decltype(auto) GetSchoolName() const noexcept { return Read<SchoolName>(); }
    decltype(auto) GetMinLevel() const noexcept { return Read<MinLevel>(); }
    decltype(auto) GetSchoolIndex() const noexcept { return Read<SchoolIndex>(); }
    decltype(auto) GetSecondarySchoolBadges() const noexcept { return Read<SecondarySchoolBadges>(); }

    AMBROSE_TYPED_VIEW(MagicSchoolTemplateView)
};

class StatEffectConfigView : public TypedView<StatEffectConfigView>
{
public:
    enum Field : std::size_t { CritAndBlockBands, PipConversionBands, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(CritAndBlockBands, "class SharedPointer<class CritAndBlockLevelData>", "m_critAndBlockLevelData"),
        ViewField::Of<PropertyValue::List>(PipConversionBands, "class SharedPointer<class PipConversionLevelData>", "m_pipConversionLevelData"),
    } };
    static constexpr ViewDefinition Definition{ "StatEffectConfigView", "class WizStatisticEffectConfig", Fields };

    decltype(auto) GetCritAndBlockBands() const noexcept { return Read<CritAndBlockBands>(); }
    decltype(auto) GetPipConversionBands() const noexcept { return Read<PipConversionBands>(); }

    AMBROSE_TYPED_VIEW(StatEffectConfigView)
};

class CritAndBlockBandView : public TypedView<CritAndBlockBandView>
{
public:
    enum Field : std::size_t { MinLevel, Values, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<int32>(MinLevel, "int", "m_minLevel"),
        ViewField::Of<PropertyValue::List>(Values, "class SharedPointer<class CritAndBlockValues>", "m_critAndBlockValues"),
    } };
    static constexpr ViewDefinition Definition{ "CritAndBlockBandView", "class CritAndBlockLevelData", Fields };

    decltype(auto) GetMinLevel() const noexcept { return Read<MinLevel>(); }
    decltype(auto) GetValues() const noexcept { return Read<Values>(); }

    AMBROSE_TYPED_VIEW(CritAndBlockBandView)
};

class CritAndBlockValuesView : public TypedView<CritAndBlockValuesView>
{
public:
    enum Field : std::size_t { CapValue, CriticalHitScalarBase, CriticalHitScalingFactor, BlockScalarBase, BlockScalingFactor, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<float>(CapValue, "float", "m_capValue"),
        ViewField::Of<float>(CriticalHitScalarBase, "float", "m_criticalHitScalarBase"),
        ViewField::Of<float>(CriticalHitScalingFactor, "float", "m_criticalHitScalingFactor"),
        ViewField::Of<float>(BlockScalarBase, "float", "m_blockScalarBase"),
        ViewField::Of<float>(BlockScalingFactor, "float", "m_blockScalingFactor"),
    } };
    static constexpr ViewDefinition Definition{ "CritAndBlockValuesView", "class CritAndBlockValues", Fields };

    decltype(auto) GetCapValue() const noexcept { return Read<CapValue>(); }
    decltype(auto) GetCriticalHitScalarBase() const noexcept { return Read<CriticalHitScalarBase>(); }
    decltype(auto) GetCriticalHitScalingFactor() const noexcept { return Read<CriticalHitScalingFactor>(); }
    decltype(auto) GetBlockScalarBase() const noexcept { return Read<BlockScalarBase>(); }
    decltype(auto) GetBlockScalingFactor() const noexcept { return Read<BlockScalingFactor>(); }

    AMBROSE_TYPED_VIEW(CritAndBlockValuesView)
};

class PipConversionBandView : public TypedView<PipConversionBandView>
{
public:
    enum Field : std::size_t { MinLevel, Values, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<int32>(MinLevel, "int", "m_minLevel"),
        ViewField::Of<PropertyValue::List>(Values, "class SharedPointer<class PipConversionValues>", "m_levelValues"),
    } };
    static constexpr ViewDefinition Definition{ "PipConversionBandView", "class PipConversionLevelData", Fields };

    decltype(auto) GetMinLevel() const noexcept { return Read<MinLevel>(); }
    decltype(auto) GetValues() const noexcept { return Read<Values>(); }

    AMBROSE_TYPED_VIEW(PipConversionBandView)
};

class PipConversionValuesView : public TypedView<PipConversionValuesView>
{
public:
    enum Field : std::size_t { CapValue, ScalarBase, ScalingFactor, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<float>(CapValue, "float", "m_capValue"),
        ViewField::Of<float>(ScalarBase, "float", "m_scalarBase"),
        ViewField::Of<float>(ScalingFactor, "float", "m_scalingFactor"),
    } };
    static constexpr ViewDefinition Definition{ "PipConversionValuesView", "class PipConversionValues", Fields };

    decltype(auto) GetCapValue() const noexcept { return Read<CapValue>(); }
    decltype(auto) GetScalarBase() const noexcept { return Read<ScalarBase>(); }
    decltype(auto) GetScalingFactor() const noexcept { return Read<ScalingFactor>(); }

    AMBROSE_TYPED_VIEW(PipConversionValuesView)
};

namespace LevelViews
{
    void RegisterAll(TypedViewRegistry& registry);
}

#endif
