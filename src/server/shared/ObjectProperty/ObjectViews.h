/*
 * Project Ambrose by Imjustchico
 * The built-in typed views game code reads client objects through: character creation info, core, client and wizard client objects, the core template every template derives from, game object and wizard item templates, the template manifest and its locations, spell templates with their effects and pip ranks, a tiered spell's retired flag and the group each tiered spell belongs to, the spellbook behavior and the tracker it keeps for each spell, sigils with their circles and a combat sigil's scalars and limits, requirement lists and named effects.
 */

#ifndef AMBROSE_OBJECTVIEWS_H
#define AMBROSE_OBJECTVIEWS_H

#include "TypedView.h"

#include <array>
#include <cstddef>
#include <string>

class WizardCharacterCreationInfoView : public TypedView<WizardCharacterCreationInfoView>
{
public:
    enum Field : std::size_t { TemplateId, Name, ShouldRenameField, GlobalId, UserId, Quarantined, LastLoginTime, AvatarBehavior, EquipmentInfoList, Location, Level, World, SchoolOfFocus, NameIndices, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<int32>(TemplateId, "int", "m_templateID"),
        ViewField::Of<std::u16string>(Name, "std::wstring", "m_name"),
        ViewField::Of<bool>(ShouldRenameField, "bool", "m_shouldRename"),
        ViewField::Of<uint64>(GlobalId, "gid", "m_globalID"),
        ViewField::Of<uint64>(UserId, "gid", "m_userID"),
        ViewField::Of<bool>(Quarantined, "bool", "m_quarantined"),
        ViewField::Of<uint32>(LastLoginTime, "unsigned int", "m_lastLoginTime"),
        ViewField::Of<PropertyObjectPtr>(AvatarBehavior, "class SharedPointer<class WizardCharacterBehavior>", "m_avatarBehavior"),
        ViewField::Of<PropertyObjectPtr>(EquipmentInfoList, "class SharedPointer<class EquippedItemInfoList>", "m_equipmentInfoList"),
        ViewField::Of<std::string>(Location, "std::string", "m_location"),
        ViewField::Of<int32>(Level, "int", "m_level"),
        ViewField::Of<int32>(World, "int", "m_world"),
        ViewField::Of<uint32>(SchoolOfFocus, "unsigned int", "m_schoolOfFocus"),
        ViewField::Of<uint32>(NameIndices, "unsigned int", "m_nameIndices"),
    } };
    static constexpr ViewDefinition Definition{ "WizardCharacterCreationInfoView", "class WizardCharacterCreationInfo", Fields };

    decltype(auto) GetTemplateId() const noexcept { return Read<TemplateId>(); }
    decltype(auto) GetName() const noexcept { return Read<Name>(); }
    decltype(auto) ShouldRename() const noexcept { return Read<ShouldRenameField>(); }
    decltype(auto) GetGlobalId() const noexcept { return Read<GlobalId>(); }
    decltype(auto) GetUserId() const noexcept { return Read<UserId>(); }
    decltype(auto) IsQuarantined() const noexcept { return Read<Quarantined>(); }
    decltype(auto) GetLastLoginTime() const noexcept { return Read<LastLoginTime>(); }
    decltype(auto) GetAvatarBehavior() const noexcept { return Read<AvatarBehavior>(); }
    decltype(auto) GetEquipmentInfoList() const noexcept { return Read<EquipmentInfoList>(); }
    decltype(auto) GetLocation() const noexcept { return Read<Location>(); }
    decltype(auto) GetLevel() const noexcept { return Read<Level>(); }
    decltype(auto) GetWorld() const noexcept { return Read<World>(); }
    decltype(auto) GetSchoolOfFocus() const noexcept { return Read<SchoolOfFocus>(); }
    decltype(auto) GetNameIndices() const noexcept { return Read<NameIndices>(); }

    AMBROSE_TYPED_VIEW(WizardCharacterCreationInfoView)
};

class CoreObjectView : public TypedView<CoreObjectView>
{
public:
    enum Field : std::size_t { InactiveBehaviors, GlobalId, PermId, Location, Orientation, Scale, TemplateId, DebugName, DisplayKey, ZoneTagId, SpeedMultiplier, MobileId, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(InactiveBehaviors, "class SharedPointer<class BehaviorInstance>", "m_inactiveBehaviors"),
        ViewField::Of<uint64>(GlobalId, "unsigned __int64", "m_globalID.m_full"),
        ViewField::Of<uint64>(PermId, "unsigned __int64", "m_permID"),
        ViewField::Of<PropertyTypes::Vector3D>(Location, "class Vector3D", "m_location"),
        ViewField::Of<PropertyTypes::Vector3D>(Orientation, "class Vector3D", "m_orientation"),
        ViewField::Of<float>(Scale, "float", "m_fScale"),
        ViewField::Of<uint64>(TemplateId, "unsigned __int64", "m_templateID.m_full"),
        ViewField::Of<std::string>(DebugName, "std::string", "m_debugName"),
        ViewField::Of<std::string>(DisplayKey, "std::string", "m_displayKey"),
        ViewField::Of<uint32>(ZoneTagId, "unsigned int", "m_zoneTagID"),
        ViewField::Of<int16>(SpeedMultiplier, "short", "m_speedMultiplier"),
        ViewField::Of<uint16>(MobileId, "unsigned short", "m_nMobileID"),
    } };
    static constexpr ViewDefinition Definition{ "CoreObjectView", "class CoreObject", Fields };

    decltype(auto) GetInactiveBehaviors() const noexcept { return Read<InactiveBehaviors>(); }
    decltype(auto) GetGlobalId() const noexcept { return Read<GlobalId>(); }
    decltype(auto) GetPermId() const noexcept { return Read<PermId>(); }
    decltype(auto) GetLocation() const noexcept { return Read<Location>(); }
    decltype(auto) GetOrientation() const noexcept { return Read<Orientation>(); }
    decltype(auto) GetScale() const noexcept { return Read<Scale>(); }
    decltype(auto) GetTemplateId() const noexcept { return Read<TemplateId>(); }
    decltype(auto) GetDebugName() const noexcept { return Read<DebugName>(); }
    decltype(auto) GetDisplayKey() const noexcept { return Read<DisplayKey>(); }
    decltype(auto) GetZoneTagId() const noexcept { return Read<ZoneTagId>(); }
    decltype(auto) GetSpeedMultiplier() const noexcept { return Read<SpeedMultiplier>(); }
    decltype(auto) GetMobileId() const noexcept { return Read<MobileId>(); }

    AMBROSE_TYPED_VIEW(CoreObjectView)
};

class ClientObjectView : public TypedView<ClientObjectView>
{
public:
    enum Field : std::size_t { CharacterId, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<uint64>(CharacterId, "gid", "m_characterId"),
    } };
    static constexpr ViewDefinition Definition{ "ClientObjectView", "class ClientObject", Fields };

    decltype(auto) GetCharacterId() const noexcept { return Read<CharacterId>(); }

    AMBROSE_TYPED_VIEW(ClientObjectView)
};

class WizClientObjectView : public TypedView<WizClientObjectView>
{
public:
    enum Field : std::size_t { GameStats, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyObjectPtr>(GameStats, "class WizGameStats*", "m_gameStats"),
    } };
    static constexpr ViewDefinition Definition{ "WizClientObjectView", "class WizClientObject", Fields };

    decltype(auto) GetGameStats() const noexcept { return Read<GameStats>(); }

    AMBROSE_TYPED_VIEW(WizClientObjectView)
};

class CoreTemplateView : public TypedView<CoreTemplateView>
{
public:
    enum Field : std::size_t { Behaviors, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(Behaviors, "class BehaviorTemplate*", "m_behaviors"),
    } };
    static constexpr ViewDefinition Definition{ "CoreTemplateView", "class CoreTemplate", Fields };

    decltype(auto) GetBehaviors() const noexcept { return Read<Behaviors>(); }

    AMBROSE_TYPED_VIEW(CoreTemplateView)
};

class GameObjectTemplateView : public TypedView<GameObjectTemplateView>
{
public:
    enum Field : std::size_t { Behaviors, ObjectName, TemplateId, VisualId, AdjectiveList, ExemptFromAoi, DisplayName, Description, ObjectType, Icon, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(Behaviors, "class BehaviorTemplate*", "m_behaviors"),
        ViewField::Of<std::string>(ObjectName, "std::string", "m_objectName"),
        ViewField::Of<uint32>(TemplateId, "unsigned int", "m_templateID"),
        ViewField::Of<uint32>(VisualId, "unsigned int", "m_visualID"),
        ViewField::Of<PropertyValue::List>(AdjectiveList, "std::string", "m_adjectiveList"),
        ViewField::Of<bool>(ExemptFromAoi, "bool", "m_exemptFromAOI"),
        ViewField::Of<std::string>(DisplayName, "std::string", "m_displayName"),
        ViewField::Of<std::string>(Description, "std::string", "m_description"),
        ViewField::Of<int64>(ObjectType, "enum ObjectType", "m_nObjectType"),
        ViewField::Of<std::string>(Icon, "std::string", "m_sIcon"),
    } };
    static constexpr ViewDefinition Definition{ "GameObjectTemplateView", "class GameObjectTemplate", Fields };

    decltype(auto) GetBehaviors() const noexcept { return Read<Behaviors>(); }
    decltype(auto) GetObjectName() const noexcept { return Read<ObjectName>(); }
    decltype(auto) GetTemplateId() const noexcept { return Read<TemplateId>(); }
    decltype(auto) GetVisualId() const noexcept { return Read<VisualId>(); }
    decltype(auto) GetAdjectiveList() const noexcept { return Read<AdjectiveList>(); }
    decltype(auto) IsExemptFromAoi() const noexcept { return Read<ExemptFromAoi>(); }
    decltype(auto) GetDisplayName() const noexcept { return Read<DisplayName>(); }
    decltype(auto) GetDescription() const noexcept { return Read<Description>(); }
    decltype(auto) GetObjectType() const noexcept { return Read<ObjectType>(); }
    decltype(auto) GetIcon() const noexcept { return Read<Icon>(); }

    AMBROSE_TYPED_VIEW(GameObjectTemplateView)
};

class WizItemTemplateView : public TypedView<WizItemTemplateView>
{
public:
    enum Field : std::size_t { TemplateId, EquipRequirements, PurchaseRequirements, EquipEffects, BaseCost, CreditsCost, AvatarInfo, AvatarFlags, ItemLimit, HolidayFlag, ItemSetBonusTemplateId,
        School, ArenaPointCost, PvpCurrencyCost, PvpTourneyCurrencyCost, Rank, Rarity, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<uint32>(TemplateId, "unsigned int", "m_templateID"),
        ViewField::Of<PropertyObjectPtr>(EquipRequirements, "class RequirementList*", "m_equipRequirements"),
        ViewField::Of<PropertyObjectPtr>(PurchaseRequirements, "class RequirementList*", "m_purchaseRequirements"),
        ViewField::Of<PropertyValue::List>(EquipEffects, "class GameEffectInfo*", "m_equipEffects"),
        ViewField::Of<float>(BaseCost, "float", "m_baseCost"),
        ViewField::Of<float>(CreditsCost, "float", "m_creditsCost"),
        ViewField::Of<PropertyObjectPtr>(AvatarInfo, "class AvatarItemInfoBase*", "m_avatarInfo"),
        ViewField::Of<PropertyValue::List>(AvatarFlags, "std::string", "m_avatarFlags"),
        ViewField::Of<int32>(ItemLimit, "int", "m_itemLimit"),
        ViewField::Of<std::string>(HolidayFlag, "std::string", "m_holidayFlag"),
        ViewField::Of<uint32>(ItemSetBonusTemplateId, "unsigned int", "m_itemSetBonusTemplateID"),
        ViewField::Of<std::string>(School, "std::string", "m_school"),
        ViewField::Of<int32>(ArenaPointCost, "int", "m_arenaPointCost"),
        ViewField::Of<int32>(PvpCurrencyCost, "int", "m_pvpCurrencyCost"),
        ViewField::Of<int32>(PvpTourneyCurrencyCost, "int", "m_pvpTourneyCurrencyCost"),
        ViewField::Of<int32>(Rank, "int", "m_rank"),
        ViewField::Of<int64>(Rarity, "enum RarityType", "m_rarity"),
    } };
    static constexpr ViewDefinition Definition{ "WizItemTemplateView", "class WizItemTemplate", Fields };

    decltype(auto) GetTemplateId() const noexcept { return Read<TemplateId>(); }
    decltype(auto) GetEquipRequirements() const noexcept { return Read<EquipRequirements>(); }
    decltype(auto) GetPurchaseRequirements() const noexcept { return Read<PurchaseRequirements>(); }
    decltype(auto) GetEquipEffects() const noexcept { return Read<EquipEffects>(); }
    decltype(auto) GetBaseCost() const noexcept { return Read<BaseCost>(); }
    decltype(auto) GetCreditsCost() const noexcept { return Read<CreditsCost>(); }
    decltype(auto) GetAvatarInfo() const noexcept { return Read<AvatarInfo>(); }
    decltype(auto) GetAvatarFlags() const noexcept { return Read<AvatarFlags>(); }
    decltype(auto) GetItemLimit() const noexcept { return Read<ItemLimit>(); }
    decltype(auto) GetHolidayFlag() const noexcept { return Read<HolidayFlag>(); }
    decltype(auto) GetItemSetBonusTemplateId() const noexcept { return Read<ItemSetBonusTemplateId>(); }
    decltype(auto) GetSchool() const noexcept { return Read<School>(); }
    decltype(auto) GetArenaPointCost() const noexcept { return Read<ArenaPointCost>(); }
    decltype(auto) GetPvpCurrencyCost() const noexcept { return Read<PvpCurrencyCost>(); }
    decltype(auto) GetPvpTourneyCurrencyCost() const noexcept { return Read<PvpTourneyCurrencyCost>(); }
    decltype(auto) GetRank() const noexcept { return Read<Rank>(); }
    decltype(auto) GetRarity() const noexcept { return Read<Rarity>(); }

    AMBROSE_TYPED_VIEW(WizItemTemplateView)
};

class TemplateManifestView : public TypedView<TemplateManifestView>
{
public:
    enum Field : std::size_t { SerializedTemplates, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(SerializedTemplates, "class TemplateLocation", "m_serializedTemplates"),
    } };
    static constexpr ViewDefinition Definition{ "TemplateManifestView", "class TemplateManifest", Fields };

    decltype(auto) GetSerializedTemplates() const noexcept { return Read<SerializedTemplates>(); }

    AMBROSE_TYPED_VIEW(TemplateManifestView)
};

class TemplateLocationView : public TypedView<TemplateLocationView>
{
public:
    enum Field : std::size_t { Filename, Id, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(Filename, "std::string", "m_filename"),
        ViewField::Of<uint32>(Id, "unsigned int", "m_id"),
    } };
    static constexpr ViewDefinition Definition{ "TemplateLocationView", "class TemplateLocation", Fields };

    decltype(auto) GetFilename() const noexcept { return Read<Filename>(); }
    decltype(auto) GetId() const noexcept { return Read<Id>(); }

    AMBROSE_TYPED_VIEW(TemplateLocationView)
};

class SpellTemplateView : public TypedView<SpellTemplateView>
{
public:
    enum Field : std::size_t { Name, Description, DisplayName, SpellBase, Effects, MagicSchoolName, TypeName, TrainingCost, Accuracy, PvP, PvE, Treasure, SpellSourceType,
        LevelRestriction, SpellRank, SecondarySchoolName, RequiredSchoolName, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(Name, "std::string", "m_name"),
        ViewField::Of<std::string>(Description, "std::string", "m_description"),
        ViewField::Of<std::string>(DisplayName, "std::string", "m_displayName"),
        ViewField::Of<std::string>(SpellBase, "std::string", "m_spellBase"),
        ViewField::Of<PropertyValue::List>(Effects, "class SharedPointer<class SpellEffect>", "m_effects"),
        ViewField::Of<std::string>(MagicSchoolName, "std::string", "m_sMagicSchoolName"),
        ViewField::Of<std::string>(TypeName, "std::string", "m_sTypeName"),
        ViewField::Of<int32>(TrainingCost, "int", "m_trainingCost"),
        ViewField::Of<int32>(Accuracy, "int", "m_accuracy"),
        ViewField::Of<bool>(PvP, "bool", "m_PvP"),
        ViewField::Of<bool>(PvE, "bool", "m_PvE"),
        ViewField::Of<bool>(Treasure, "bool", "m_Treasure"),
        ViewField::Of<int64>(SpellSourceType, "enum SpellTemplate::kSpellSourceType", "m_spellSourceType"),
        ViewField::Of<int32>(LevelRestriction, "int", "m_levelRestriction"),
        ViewField::Of<PropertyObjectPtr>(SpellRank, "class SharedPointer<class SpellRank>", "m_spellRank"),
        ViewField::Of<std::string>(SecondarySchoolName, "std::string", "m_secondarySchoolName"),
        ViewField::Of<std::string>(RequiredSchoolName, "std::string", "m_requiredSchoolName"),
    } };
    static constexpr ViewDefinition Definition{ "SpellTemplateView", "class SpellTemplate", Fields };

    decltype(auto) GetName() const noexcept { return Read<Name>(); }
    decltype(auto) GetDescription() const noexcept { return Read<Description>(); }
    decltype(auto) GetDisplayName() const noexcept { return Read<DisplayName>(); }
    decltype(auto) GetSpellBase() const noexcept { return Read<SpellBase>(); }
    decltype(auto) GetEffects() const noexcept { return Read<Effects>(); }
    decltype(auto) GetMagicSchoolName() const noexcept { return Read<MagicSchoolName>(); }
    decltype(auto) GetTypeName() const noexcept { return Read<TypeName>(); }
    decltype(auto) GetTrainingCost() const noexcept { return Read<TrainingCost>(); }
    decltype(auto) GetAccuracy() const noexcept { return Read<Accuracy>(); }
    decltype(auto) IsPvP() const noexcept { return Read<PvP>(); }
    decltype(auto) IsPvE() const noexcept { return Read<PvE>(); }
    decltype(auto) IsTreasure() const noexcept { return Read<Treasure>(); }
    decltype(auto) GetSpellSourceType() const noexcept { return Read<SpellSourceType>(); }
    decltype(auto) GetLevelRestriction() const noexcept { return Read<LevelRestriction>(); }
    decltype(auto) GetSpellRank() const noexcept { return Read<SpellRank>(); }
    decltype(auto) GetSecondarySchoolName() const noexcept { return Read<SecondarySchoolName>(); }
    decltype(auto) GetRequiredSchoolName() const noexcept { return Read<RequiredSchoolName>(); }

    AMBROSE_TYPED_VIEW(SpellTemplateView)
};

class SpellEffectView : public TypedView<SpellEffectView>
{
public:
    enum Field : std::size_t { EffectType, EffectParam, Disposition, DamageTypeName, DamageType, PipNum, ActNum, EffectTarget, NumRounds, ParamPerRound, HealModifier,
        SpellTemplateId, EnchantmentSpellTemplateId, Cloaked, ArmorPiercingParam, ChancePerTarget, Rank, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<int64>(EffectType, "enum SpellEffect::kSpellEffects", "m_effectType"),
        ViewField::Of<int32>(EffectParam, "int", "m_effectParam"),
        ViewField::Of<int64>(Disposition, "enum SpellEffect::kHangingDisposition", "m_disposition"),
        ViewField::Of<std::string>(DamageTypeName, "std::string", "m_sDamageType"),
        ViewField::Of<uint32>(DamageType, "unsigned int", "m_damageType"),
        ViewField::Of<int32>(PipNum, "int", "m_pipNum"),
        ViewField::Of<int32>(ActNum, "int", "m_actNum"),
        ViewField::Of<int64>(EffectTarget, "enum SpellEffect::kEffectTarget", "m_effectTarget"),
        ViewField::Of<int32>(NumRounds, "int", "m_numRounds"),
        ViewField::Of<int32>(ParamPerRound, "int", "m_paramPerRound"),
        ViewField::Of<float>(HealModifier, "float", "m_healModifier"),
        ViewField::Of<uint32>(SpellTemplateId, "unsigned int", "m_spellTemplateID"),
        ViewField::Of<uint32>(EnchantmentSpellTemplateId, "unsigned int", "m_enchantmentSpellTemplateID"),
        ViewField::Of<bool>(Cloaked, "bool", "m_cloaked"),
        ViewField::Of<int32>(ArmorPiercingParam, "int", "m_armorPiercingParam"),
        ViewField::Of<int32>(ChancePerTarget, "int", "m_chancePerTarget"),
        ViewField::Of<int32>(Rank, "int", "m_rank"),
    } };
    static constexpr ViewDefinition Definition{ "SpellEffectView", "class SpellEffect", Fields };

    decltype(auto) GetEffectType() const noexcept { return Read<EffectType>(); }
    decltype(auto) GetEffectParam() const noexcept { return Read<EffectParam>(); }
    decltype(auto) GetDisposition() const noexcept { return Read<Disposition>(); }
    decltype(auto) GetDamageTypeName() const noexcept { return Read<DamageTypeName>(); }
    decltype(auto) GetDamageType() const noexcept { return Read<DamageType>(); }
    decltype(auto) GetPipNum() const noexcept { return Read<PipNum>(); }
    decltype(auto) GetActNum() const noexcept { return Read<ActNum>(); }
    decltype(auto) GetEffectTarget() const noexcept { return Read<EffectTarget>(); }
    decltype(auto) GetNumRounds() const noexcept { return Read<NumRounds>(); }
    decltype(auto) GetParamPerRound() const noexcept { return Read<ParamPerRound>(); }
    decltype(auto) GetHealModifier() const noexcept { return Read<HealModifier>(); }
    decltype(auto) GetSpellTemplateId() const noexcept { return Read<SpellTemplateId>(); }
    decltype(auto) GetEnchantmentSpellTemplateId() const noexcept { return Read<EnchantmentSpellTemplateId>(); }
    decltype(auto) IsCloaked() const noexcept { return Read<Cloaked>(); }
    decltype(auto) GetArmorPiercingParam() const noexcept { return Read<ArmorPiercingParam>(); }
    decltype(auto) GetChancePerTarget() const noexcept { return Read<ChancePerTarget>(); }
    decltype(auto) GetRank() const noexcept { return Read<Rank>(); }

    AMBROSE_TYPED_VIEW(SpellEffectView)
};

class SpellRankView : public TypedView<SpellRankView>
{
public:
    enum Field : std::size_t { Rank, BalancePips, DeathPips, FirePips, IcePips, LifePips, MythPips, StormPips, ShadowPips, XPipSpell, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<uint8>(Rank, "unsigned char", "m_spellRank"),
        ViewField::Of<uint8>(BalancePips, "unsigned char", "m_balancePips"),
        ViewField::Of<uint8>(DeathPips, "unsigned char", "m_deathPips"),
        ViewField::Of<uint8>(FirePips, "unsigned char", "m_firePips"),
        ViewField::Of<uint8>(IcePips, "unsigned char", "m_icePips"),
        ViewField::Of<uint8>(LifePips, "unsigned char", "m_lifePips"),
        ViewField::Of<uint8>(MythPips, "unsigned char", "m_mythPips"),
        ViewField::Of<uint8>(StormPips, "unsigned char", "m_stormPips"),
        ViewField::Of<uint8>(ShadowPips, "unsigned char", "m_shadowPips"),
        ViewField::Of<bool>(XPipSpell, "bool", "m_xPipSpell"),
    } };
    static constexpr ViewDefinition Definition{ "SpellRankView", "class SpellRank", Fields };

    decltype(auto) GetRank() const noexcept { return Read<Rank>(); }
    decltype(auto) GetBalancePips() const noexcept { return Read<BalancePips>(); }
    decltype(auto) GetDeathPips() const noexcept { return Read<DeathPips>(); }
    decltype(auto) GetFirePips() const noexcept { return Read<FirePips>(); }
    decltype(auto) GetIcePips() const noexcept { return Read<IcePips>(); }
    decltype(auto) GetLifePips() const noexcept { return Read<LifePips>(); }
    decltype(auto) GetMythPips() const noexcept { return Read<MythPips>(); }
    decltype(auto) GetStormPips() const noexcept { return Read<StormPips>(); }
    decltype(auto) GetShadowPips() const noexcept { return Read<ShadowPips>(); }
    decltype(auto) IsXPipSpell() const noexcept { return Read<XPipSpell>(); }

    AMBROSE_TYPED_VIEW(SpellRankView)
};

class TieredSpellTemplateView : public TypedView<TieredSpellTemplateView>
{
public:
    enum Field : std::size_t { Retired, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<bool>(Retired, "bool", "m_retired"),
    } };
    static constexpr ViewDefinition Definition{ "TieredSpellTemplateView", "class TieredSpellTemplate", Fields };

    decltype(auto) IsRetired() const noexcept { return Read<Retired>(); }

    AMBROSE_TYPED_VIEW(TieredSpellTemplateView)
};

class TieredSpellGroupInfoListView : public TypedView<TieredSpellGroupInfoListView>
{
public:
    enum Field : std::size_t { Groups, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(Groups, "class SharedPointer<class TieredSpellGroupInfo>", "m_tieredSpellGroupInfoList"),
    } };
    static constexpr ViewDefinition Definition{ "TieredSpellGroupInfoListView", "class TieredSpellGroupInfoList", Fields };

    decltype(auto) GetGroups() const noexcept { return Read<Groups>(); }

    AMBROSE_TYPED_VIEW(TieredSpellGroupInfoListView)
};

class TieredSpellGroupInfoView : public TypedView<TieredSpellGroupInfoView>
{
public:
    enum Field : std::size_t { SpellName, Data, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(SpellName, "std::string", "m_spellName"),
        ViewField::Of<PropertyObjectPtr>(Data, "class SharedPointer<class TieredSpellGroupInfoData>", "m_theTieredSpellGroupInfoData"),
    } };
    static constexpr ViewDefinition Definition{ "TieredSpellGroupInfoView", "class TieredSpellGroupInfo", Fields };

    decltype(auto) GetSpellName() const noexcept { return Read<SpellName>(); }
    decltype(auto) GetData() const noexcept { return Read<Data>(); }

    AMBROSE_TYPED_VIEW(TieredSpellGroupInfoView)
};

class TieredSpellGroupInfoDataView : public TypedView<TieredSpellGroupInfoDataView>
{
public:
    enum Field : std::size_t { GroupIndex, TierOneSpellName, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<int32>(GroupIndex, "int", "m_tsGroupIndex"),
        ViewField::Of<std::string>(TierOneSpellName, "std::string", "m_tsGroupTierOneSpellName"),
    } };
    static constexpr ViewDefinition Definition{ "TieredSpellGroupInfoDataView", "class TieredSpellGroupInfoData", Fields };

    decltype(auto) GetGroupIndex() const noexcept { return Read<GroupIndex>(); }
    decltype(auto) GetTierOneSpellName() const noexcept { return Read<TierOneSpellName>(); }

    AMBROSE_TYPED_VIEW(TieredSpellGroupInfoDataView)
};

class ClientSpellbookBehaviorView : public TypedView<ClientSpellbookBehaviorView>
{
public:
    enum Field : std::size_t { BehaviorTemplateNameId, Spells, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<uint32>(BehaviorTemplateNameId, "unsigned int", "m_behaviorTemplateNameID"),
        ViewField::Of<PropertyValue::List>(Spells, "class SharedPointer<class SpellIDTracker>", "m_spellIDList"),
    } };
    static constexpr ViewDefinition Definition{ "ClientSpellbookBehaviorView", "class ClientSpellbookBehavior", Fields };

    decltype(auto) GetBehaviorTemplateNameId() const noexcept { return Read<BehaviorTemplateNameId>(); }
    decltype(auto) GetSpells() const noexcept { return Read<Spells>(); }

    AMBROSE_TYPED_VIEW(ClientSpellbookBehaviorView)
};

class SpellIDTrackerView : public TypedView<SpellIDTrackerView>
{
public:
    enum Field : std::size_t { SpellId, IsRetired, TieredSpellGroupIndex, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<uint32>(SpellId, "unsigned int", "m_spellID"),
        ViewField::Of<bool>(IsRetired, "bool", "m_isRetired"),
        ViewField::Of<int32>(TieredSpellGroupIndex, "int", "m_tieredSpellGroupIndex"),
    } };
    static constexpr ViewDefinition Definition{ "SpellIDTrackerView", "class SpellIDTracker", Fields };

    decltype(auto) GetSpellId() const noexcept { return Read<SpellId>(); }
    decltype(auto) GetIsRetired() const noexcept { return Read<IsRetired>(); }
    decltype(auto) GetTieredSpellGroupIndex() const noexcept { return Read<TieredSpellGroupIndex>(); }

    AMBROSE_TYPED_VIEW(SpellIDTrackerView)
};

class SigilTemplateView : public TypedView<SigilTemplateView>
{
public:
    enum Field : std::size_t { SigilName, SigilType, UseState, SubCircles, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(SigilName, "std::string", "m_sigilName"),
        ViewField::Of<std::string>(SigilType, "std::string", "m_sigilType"),
        ViewField::Of<bool>(UseState, "bool", "m_useState"),
        ViewField::Of<PropertyValue::List>(SubCircles, "class SigilSubCircle*", "m_subCircles"),
    } };
    static constexpr ViewDefinition Definition{ "SigilTemplateView", "class SigilTemplate", Fields };

    decltype(auto) GetSigilName() const noexcept { return Read<SigilName>(); }
    decltype(auto) GetSigilType() const noexcept { return Read<SigilType>(); }
    decltype(auto) IsUseState() const noexcept { return Read<UseState>(); }
    decltype(auto) GetSubCircles() const noexcept { return Read<SubCircles>(); }

    AMBROSE_TYPED_VIEW(SigilTemplateView)
};

class CombatSigilTemplateView : public TypedView<CombatSigilTemplateView>
{
public:
    enum Field : std::size_t { EngageRadius, BattlefieldEffects, ShadowThresholdType, ShadowThresholdFactor, ShadowPipRatingFactor, ScalarDamagePvP, ScalarResistPvP, ScalarPiercePvP, ScalarDamagePvE, ScalarResistPvE,
        ScalarPiercePvE, DamageLimitPvP, DK0PvP, DN0PvP, ResistLimitPvP, RK0PvP, RN0PvP, DamageLimitPvE, DK0PvE, DN0PvE, ResistLimitPvE, RK0PvE, RN0PvE, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<float>(EngageRadius, "float", "m_engageRadius"),
        ViewField::Of<PropertyValue::List>(BattlefieldEffects, "class SharedPointer<class SpellEffect>", "m_battlefieldEffects"),
        ViewField::Of<int64>(ShadowThresholdType, "enum kShadow_Threshold_Type", "m_shadowThresholdType"),
        ViewField::Of<float>(ShadowThresholdFactor, "float", "m_shadowThresholdFactor"),
        ViewField::Of<float>(ShadowPipRatingFactor, "float", "m_shadowPipRatingFactor"),
        ViewField::Of<float>(ScalarDamagePvP, "float", "m_scalarDamagePvP"),
        ViewField::Of<float>(ScalarResistPvP, "float", "m_scalarResistPvP"),
        ViewField::Of<float>(ScalarPiercePvP, "float", "m_scalarPiercePvP"),
        ViewField::Of<float>(ScalarDamagePvE, "float", "m_scalarDamagePvE"),
        ViewField::Of<float>(ScalarResistPvE, "float", "m_scalarResistPvE"),
        ViewField::Of<float>(ScalarPiercePvE, "float", "m_scalarPiercePvE"),
        ViewField::Of<float>(DamageLimitPvP, "float", "m_damageLimitPvP"),
        ViewField::Of<float>(DK0PvP, "float", "m_dK0PvP"),
        ViewField::Of<float>(DN0PvP, "float", "m_dN0PvP"),
        ViewField::Of<float>(ResistLimitPvP, "float", "m_resistLimitPvP"),
        ViewField::Of<float>(RK0PvP, "float", "m_rK0PvP"),
        ViewField::Of<float>(RN0PvP, "float", "m_rN0PvP"),
        ViewField::Of<float>(DamageLimitPvE, "float", "m_damageLimitPvE"),
        ViewField::Of<float>(DK0PvE, "float", "m_dK0PvE"),
        ViewField::Of<float>(DN0PvE, "float", "m_dN0PvE"),
        ViewField::Of<float>(ResistLimitPvE, "float", "m_resistLimitPvE"),
        ViewField::Of<float>(RK0PvE, "float", "m_rK0PvE"),
        ViewField::Of<float>(RN0PvE, "float", "m_rN0PvE"),
    } };
    static constexpr ViewDefinition Definition{ "CombatSigilTemplateView", "class CombatSigilTemplate", Fields };

    decltype(auto) GetEngageRadius() const noexcept { return Read<EngageRadius>(); }
    decltype(auto) GetBattlefieldEffects() const noexcept { return Read<BattlefieldEffects>(); }
    decltype(auto) GetShadowThresholdType() const noexcept { return Read<ShadowThresholdType>(); }
    decltype(auto) GetShadowThresholdFactor() const noexcept { return Read<ShadowThresholdFactor>(); }
    decltype(auto) GetShadowPipRatingFactor() const noexcept { return Read<ShadowPipRatingFactor>(); }
    decltype(auto) GetScalarDamagePvP() const noexcept { return Read<ScalarDamagePvP>(); }
    decltype(auto) GetScalarResistPvP() const noexcept { return Read<ScalarResistPvP>(); }
    decltype(auto) GetScalarPiercePvP() const noexcept { return Read<ScalarPiercePvP>(); }
    decltype(auto) GetScalarDamagePvE() const noexcept { return Read<ScalarDamagePvE>(); }
    decltype(auto) GetScalarResistPvE() const noexcept { return Read<ScalarResistPvE>(); }
    decltype(auto) GetScalarPiercePvE() const noexcept { return Read<ScalarPiercePvE>(); }
    decltype(auto) GetDamageLimitPvP() const noexcept { return Read<DamageLimitPvP>(); }
    decltype(auto) GetDK0PvP() const noexcept { return Read<DK0PvP>(); }
    decltype(auto) GetDN0PvP() const noexcept { return Read<DN0PvP>(); }
    decltype(auto) GetResistLimitPvP() const noexcept { return Read<ResistLimitPvP>(); }
    decltype(auto) GetRK0PvP() const noexcept { return Read<RK0PvP>(); }
    decltype(auto) GetRN0PvP() const noexcept { return Read<RN0PvP>(); }
    decltype(auto) GetDamageLimitPvE() const noexcept { return Read<DamageLimitPvE>(); }
    decltype(auto) GetDK0PvE() const noexcept { return Read<DK0PvE>(); }
    decltype(auto) GetDN0PvE() const noexcept { return Read<DN0PvE>(); }
    decltype(auto) GetResistLimitPvE() const noexcept { return Read<ResistLimitPvE>(); }
    decltype(auto) GetRK0PvE() const noexcept { return Read<RK0PvE>(); }
    decltype(auto) GetRN0PvE() const noexcept { return Read<RN0PvE>(); }

    AMBROSE_TYPED_VIEW(CombatSigilTemplateView)
};

class SigilSubCircleView : public TypedView<SigilSubCircleView>
{
public:
    enum Field : std::size_t { LocationType, LocationPreference, Rotation, Radius, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(LocationType, "std::string", "m_locationType"),
        ViewField::Of<std::string>(LocationPreference, "std::string", "m_locationPreference"),
        ViewField::Of<float>(Rotation, "float", "m_rotation"),
        ViewField::Of<float>(Radius, "float", "m_radius"),
    } };
    static constexpr ViewDefinition Definition{ "SigilSubCircleView", "class SigilSubCircle", Fields };

    decltype(auto) GetLocationType() const noexcept { return Read<LocationType>(); }
    decltype(auto) GetLocationPreference() const noexcept { return Read<LocationPreference>(); }
    decltype(auto) GetRotation() const noexcept { return Read<Rotation>(); }
    decltype(auto) GetRadius() const noexcept { return Read<Radius>(); }

    AMBROSE_TYPED_VIEW(SigilSubCircleView)
};

class RequirementListView : public TypedView<RequirementListView>
{
public:
    enum Field : std::size_t { ApplyNot, Operator, Requirements, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<bool>(ApplyNot, "bool", "m_applyNOT"),
        ViewField::Of<int64>(Operator, "enum Requirement::Operator", "m_operator"),
        ViewField::Of<PropertyValue::List>(Requirements, "class Requirement*", "m_requirements"),
    } };
    static constexpr ViewDefinition Definition{ "RequirementListView", "class RequirementList", Fields };

    decltype(auto) AppliesNot() const noexcept { return Read<ApplyNot>(); }
    decltype(auto) GetOperator() const noexcept { return Read<Operator>(); }
    decltype(auto) GetRequirements() const noexcept { return Read<Requirements>(); }

    AMBROSE_TYPED_VIEW(RequirementListView)
};

class NamedEffectView : public TypedView<NamedEffectView>
{
public:
    enum Field : std::size_t { CurrentTickCount, EffectNameId, OnPet, OriginatorId, ItemSlotId, InternalId, EndTime, OverrideName, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<double>(CurrentTickCount, "double", "m_currentTickCount"),
        ViewField::Of<uint32>(EffectNameId, "unsigned int", "m_effectNameID"),
        ViewField::Of<bool>(OnPet, "bool", "m_bIsOnPet"),
        ViewField::Of<uint64>(OriginatorId, "gid", "m_originatorID"),
        ViewField::Of<uint32>(ItemSlotId, "unsigned int", "m_itemSlotID"),
        ViewField::Of<int32>(InternalId, "int", "m_internalID"),
        ViewField::Of<uint32>(EndTime, "unsigned int", "m_endTime"),
        ViewField::Of<std::string>(OverrideName, "std::string", "m_overrideName"),
    } };
    static constexpr ViewDefinition Definition{ "NamedEffectView", "class NamedEffect", Fields };

    decltype(auto) GetCurrentTickCount() const noexcept { return Read<CurrentTickCount>(); }
    decltype(auto) GetEffectNameId() const noexcept { return Read<EffectNameId>(); }
    decltype(auto) IsOnPet() const noexcept { return Read<OnPet>(); }
    decltype(auto) GetOriginatorId() const noexcept { return Read<OriginatorId>(); }
    decltype(auto) GetItemSlotId() const noexcept { return Read<ItemSlotId>(); }
    decltype(auto) GetInternalId() const noexcept { return Read<InternalId>(); }
    decltype(auto) GetEndTime() const noexcept { return Read<EndTime>(); }
    decltype(auto) GetOverrideName() const noexcept { return Read<OverrideName>(); }

    AMBROSE_TYPED_VIEW(NamedEffectView)
};

namespace ObjectViews
{
    void RegisterAll(TypedViewRegistry& registry);
}

#endif
