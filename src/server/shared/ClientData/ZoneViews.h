/*
 * Project Ambrose by Imjustchico
 * The typed views the zone extractor reads a zone's gamedata.bin through: the WizZoneData at its root with the settings the zone tables keep, each LocationTemplate its location list holds, and each CoreObjectInfo its object list holds, subclasses included.
 */

#ifndef AMBROSE_ZONEVIEWS_H
#define AMBROSE_ZONEVIEWS_H

#include "TypedView.h"

#include <array>
#include <cstddef>
#include <string>

class WizZoneDataView : public TypedView<WizZoneDataView>
{
public:
    enum Field : std::size_t { ZoneName, DisplayName, Locations, Objects, HealingPerMinute, SoftLimit, HardLimit, FarClip, NoMounts, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(ZoneName, "std::string", "m_zoneName"),
        ViewField::Of<std::string>(DisplayName, "std::string", "m_zoneDisplayName"),
        ViewField::Of<PropertyValue::List>(Locations, "class LocationTemplate", "m_locationList"),
        ViewField::Of<PropertyValue::List>(Objects, "class SharedPointer<class CoreObjectInfo>", "m_objectList"),
        ViewField::Of<int32>(HealingPerMinute, "int", "m_healingPerMinute"),
        ViewField::Of<int32>(SoftLimit, "int", "m_nSoftLimit"),
        ViewField::Of<int32>(HardLimit, "int", "m_nHardLimit"),
        ViewField::Of<float>(FarClip, "float", "m_farClip"),
        ViewField::Of<bool>(NoMounts, "bool", "m_noMounts"),
    } };
    static constexpr ViewDefinition Definition{ "WizZoneDataView", "class WizZoneData", Fields };

    decltype(auto) GetZoneName() const noexcept { return Read<ZoneName>(); }
    decltype(auto) GetDisplayName() const noexcept { return Read<DisplayName>(); }
    decltype(auto) GetLocations() const noexcept { return Read<Locations>(); }
    decltype(auto) GetObjects() const noexcept { return Read<Objects>(); }
    decltype(auto) GetHealingPerMinute() const noexcept { return Read<HealingPerMinute>(); }
    decltype(auto) GetSoftLimit() const noexcept { return Read<SoftLimit>(); }
    decltype(auto) GetHardLimit() const noexcept { return Read<HardLimit>(); }
    decltype(auto) GetFarClip() const noexcept { return Read<FarClip>(); }
    decltype(auto) HasNoMounts() const noexcept { return Read<NoMounts>(); }

    AMBROSE_TYPED_VIEW(WizZoneDataView)
};

class LocationTemplateView : public TypedView<LocationTemplateView>
{
public:
    enum Field : std::size_t { Name, Location, Direction, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(Name, "std::string", "m_locName"),
        ViewField::Of<PropertyTypes::Vector3D>(Location, "class Vector3D", "m_location"),
        ViewField::Of<float>(Direction, "float", "m_direction"),
    } };
    static constexpr ViewDefinition Definition{ "LocationTemplateView", "class LocationTemplate", Fields };

    decltype(auto) GetName() const noexcept { return Read<Name>(); }
    decltype(auto) GetLocation() const noexcept { return Read<Location>(); }
    decltype(auto) GetDirection() const noexcept { return Read<Direction>(); }

    AMBROSE_TYPED_VIEW(LocationTemplateView)
};

class CoreObjectInfoView : public TypedView<CoreObjectInfoView>
{
public:
    enum Field : std::size_t
    {
        TemplateId, ObjectId, Location, Orientation, Scale, ZoneTag, StartState, OverrideName, GlobalDynamic, Undetectable, SpawnRequirements, LoadingType, FieldCount
    };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<uint64>(TemplateId, "unsigned __int64", "m_templateID.m_full"),
        ViewField::Of<uint32>(ObjectId, "unsigned int", "m_nObjectID"),
        ViewField::Of<PropertyTypes::Vector3D>(Location, "class Vector3D", "m_location"),
        ViewField::Of<PropertyTypes::Vector3D>(Orientation, "class Vector3D", "m_orientation"),
        ViewField::Of<float>(Scale, "float", "m_fScale"),
        ViewField::Of<std::string>(ZoneTag, "std::string", "m_zoneTag"),
        ViewField::Of<std::string>(StartState, "std::string", "m_startState"),
        ViewField::Of<std::string>(OverrideName, "std::string", "m_overrideName"),
        ViewField::Of<bool>(GlobalDynamic, "bool", "m_globalDynamic"),
        ViewField::Of<bool>(Undetectable, "bool", "m_bUndetectable"),
        ViewField::Of<PropertyObjectPtr>(SpawnRequirements, "class SharedPointer<class RequirementList>", "m_spawnRequirements"),
        ViewField::Of<int64>(LoadingType, "enum CoreObjectInfo::LoadingType", "m_loadingType"),
    } };
    static constexpr ViewDefinition Definition{ "CoreObjectInfoView", "class CoreObjectInfo", Fields };

    decltype(auto) GetTemplateId() const noexcept { return Read<TemplateId>(); }
    decltype(auto) GetObjectId() const noexcept { return Read<ObjectId>(); }
    decltype(auto) GetLocation() const noexcept { return Read<Location>(); }
    decltype(auto) GetOrientation() const noexcept { return Read<Orientation>(); }
    decltype(auto) GetScale() const noexcept { return Read<Scale>(); }
    decltype(auto) GetZoneTag() const noexcept { return Read<ZoneTag>(); }
    decltype(auto) GetStartState() const noexcept { return Read<StartState>(); }
    decltype(auto) GetOverrideName() const noexcept { return Read<OverrideName>(); }
    decltype(auto) IsGlobalDynamic() const noexcept { return Read<GlobalDynamic>(); }
    decltype(auto) IsUndetectable() const noexcept { return Read<Undetectable>(); }
    decltype(auto) GetSpawnRequirements() const noexcept { return Read<SpawnRequirements>(); }
    decltype(auto) GetLoadingType() const noexcept { return Read<LoadingType>(); }

    AMBROSE_TYPED_VIEW(CoreObjectInfoView)
};

namespace ZoneViews
{
    void RegisterAll(TypedViewRegistry& registry);
}

#endif
