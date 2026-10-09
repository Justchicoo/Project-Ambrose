/*
 * Project Ambrose by Imjustchico
 * The typed views the zone extractor reads a zone's gamedata.bin through: the WizZoneData at its root with the settings the zone tables keep, each LocationTemplate its location list holds, and each CoreObjectInfo its object list holds, subclasses included; and the ones it reads a zone's spawnData.xml through: the SpawnManager at its root, each SpawnObject it lists with its counts, timers and requirements, each SpawnItem with its chance and the SpawnObjectInfo it places, read as a CoreObjectInfo with the path and start node fields a spawn adds.
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

class SpawnManagerView : public TypedView<SpawnManagerView>
{
public:
    enum Field : std::size_t { Spawners, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<PropertyValue::List>(Spawners, "class SharedPointer<class SpawnObject>", "m_spawners"),
    } };
    static constexpr ViewDefinition Definition{ "SpawnManagerView", "class SpawnManager", Fields };

    decltype(auto) GetSpawners() const noexcept { return Read<Spawners>(); }

    AMBROSE_TYPED_VIEW(SpawnManagerView)
};

class SpawnObjectView : public TypedView<SpawnObjectView>
{
public:
    enum Field : std::size_t
    {
        Name, Id, Active, PopSensitive, MaxSpawns, AtLeastOneSpawn, ActivateAtMax, SpawnTime, RespawnRate, SpawnList, GlobalDynamicReqs, GlobalDynamic, WaitForTimer,
        ZoneLevelMin, ZoneLevelMax, ZoneLevelUp, FieldCount
    };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<std::string>(Name, "std::string", "m_name"),
        ViewField::Of<uint64>(Id, "gid", "m_id"),
        ViewField::Of<bool>(Active, "bool", "m_active"),
        ViewField::Of<bool>(PopSensitive, "bool", "m_popSensitive"),
        ViewField::Of<uint32>(MaxSpawns, "unsigned int", "m_maxNumberOfSpawns"),
        ViewField::Of<bool>(AtLeastOneSpawn, "bool", "m_atLeastOneSpawn"),
        ViewField::Of<bool>(ActivateAtMax, "bool", "m_activateAtMax"),
        ViewField::Of<int32>(SpawnTime, "int", "m_spawnTime"),
        ViewField::Of<uint32>(RespawnRate, "unsigned int", "m_respawnRate"),
        ViewField::Of<PropertyValue::List>(SpawnList, "class SpawnItem*", "m_spawnList"),
        ViewField::Of<PropertyObjectPtr>(GlobalDynamicReqs, "class RequirementList*", "m_globalDynamicReqs"),
        ViewField::Of<bool>(GlobalDynamic, "bool", "m_globalDynamic"),
        ViewField::Of<bool>(WaitForTimer, "bool", "m_waitForTimer"),
        ViewField::Of<uint32>(ZoneLevelMin, "unsigned int", "m_zoneLevelMin"),
        ViewField::Of<uint32>(ZoneLevelMax, "unsigned int", "m_zoneLevelMax"),
        ViewField::Of<uint32>(ZoneLevelUp, "unsigned int", "m_zoneLevelUp"),
    } };
    static constexpr ViewDefinition Definition{ "SpawnObjectView", "class SpawnObject", Fields };

    decltype(auto) GetName() const noexcept { return Read<Name>(); }
    decltype(auto) GetId() const noexcept { return Read<Id>(); }
    decltype(auto) IsActive() const noexcept { return Read<Active>(); }
    decltype(auto) IsPopSensitive() const noexcept { return Read<PopSensitive>(); }
    decltype(auto) GetMaxSpawns() const noexcept { return Read<MaxSpawns>(); }
    decltype(auto) HasAtLeastOneSpawn() const noexcept { return Read<AtLeastOneSpawn>(); }
    decltype(auto) ActivatesAtMax() const noexcept { return Read<ActivateAtMax>(); }
    decltype(auto) GetSpawnTime() const noexcept { return Read<SpawnTime>(); }
    decltype(auto) GetRespawnRate() const noexcept { return Read<RespawnRate>(); }
    decltype(auto) GetSpawnList() const noexcept { return Read<SpawnList>(); }
    decltype(auto) GetGlobalDynamicReqs() const noexcept { return Read<GlobalDynamicReqs>(); }
    decltype(auto) IsGlobalDynamic() const noexcept { return Read<GlobalDynamic>(); }
    decltype(auto) WaitsForTimer() const noexcept { return Read<WaitForTimer>(); }
    decltype(auto) GetZoneLevelMin() const noexcept { return Read<ZoneLevelMin>(); }
    decltype(auto) GetZoneLevelMax() const noexcept { return Read<ZoneLevelMax>(); }
    decltype(auto) GetZoneLevelUp() const noexcept { return Read<ZoneLevelUp>(); }

    AMBROSE_TYPED_VIEW(SpawnObjectView)
};

class SpawnItemView : public TypedView<SpawnItemView>
{
public:
    enum Field : std::size_t { PercentChance, ObjectInfo, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<uint8>(PercentChance, "unsigned char", "m_percentChance"),
        ViewField::Of<PropertyObjectPtr>(ObjectInfo, "class SpawnObjectInfo*", "m_objectInfo"),
    } };
    static constexpr ViewDefinition Definition{ "SpawnItemView", "class SpawnItem", Fields };

    decltype(auto) GetPercentChance() const noexcept { return Read<PercentChance>(); }
    decltype(auto) GetObjectInfo() const noexcept { return Read<ObjectInfo>(); }

    AMBROSE_TYPED_VIEW(SpawnItemView)
};

class SpawnObjectInfoView : public TypedView<SpawnObjectInfoView>
{
public:
    enum Field : std::size_t { StartNodeType, StartNode, PathId, UniqueLoc, FieldCount };
    static constexpr std::array<ViewField, FieldCount> Fields{ {
        ViewField::Of<int64>(StartNodeType, "enum SpawnObjectInfo::StartNodeType", "m_kStartNodeType"),
        ViewField::Of<uint32>(StartNode, "unsigned int", "m_startNode"),
        ViewField::Of<uint64>(PathId, "gid", "m_pathID"),
        ViewField::Of<int8>(UniqueLoc, "char", "m_uniqueLoc"),
    } };
    static constexpr ViewDefinition Definition{ "SpawnObjectInfoView", "class SpawnObjectInfo", Fields };

    decltype(auto) GetStartNodeType() const noexcept { return Read<StartNodeType>(); }
    decltype(auto) GetStartNode() const noexcept { return Read<StartNode>(); }
    decltype(auto) GetPathId() const noexcept { return Read<PathId>(); }
    decltype(auto) GetUniqueLoc() const noexcept { return Read<UniqueLoc>(); }

    AMBROSE_TYPED_VIEW(SpawnObjectInfoView)
};

namespace ZoneViews
{
    void RegisterAll(TypedViewRegistry& registry);
}

#endif
