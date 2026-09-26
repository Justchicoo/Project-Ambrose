/*
 * Project Ambrose by Imjustchico
 * The built-in typed views game code reads client objects through: character creation info, core, client and wizard client objects, the core template every template derives from, game object and wizard item templates, the template manifest and its locations, requirement lists and named effects.
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
