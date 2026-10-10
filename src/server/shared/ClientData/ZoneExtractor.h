/*
 * Project Ambrose by Imjustchico
 * Extracts every zone of the user's own install from the gamedata.bin its GameData archive holds: the zone's settings, each named location and one row for each entry of its object list, read through the zone views, with the zone known by the name its own data gives it, which the archive's name must match; then the walk-in volumes of its volumes.xml and the triggers of its triggers.xml, read by property name because their classes are the server's own, which only a supplement to the type dump describes, and the spawners of its spawnData.xml, each with the items it may place, read through the spawn views because the type dump describes them, where a file that does not decode is counted against its zone and the rest is still extracted, and clientSpawnData.xml, which only the client spawns, is never read; and the paths of its pathData.xml, each with its nodes in order taken from the node list its pathNodeData.bin holds as a bare versionable object, so a spawn that stands on a path can be placed on one of its nodes. A part of a zone whose class the type dump cannot describe is reported with its class hash and path rather than guessed at or dropped silently: an object list entry of such a class is left out, and inside an entry that is kept the part is left out of what the row holds; every other problem is reported up to a cap, either from an open GameData folder and catalog or straight from an install folder and type dump, telling a caller that asks how many archives it has read after each one.
 */

#ifndef AMBROSE_ZONEEXTRACTOR_H
#define AMBROSE_ZONEEXTRACTOR_H

#include "PropertyValue.h"
#include "TypeDumpLoader.h"
#include "TypeRegistry.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct ExtractedLocation
{
    std::string Name;
    PropertyTypes::Vector3D Location;
    float Direction = 0.0f;
};

struct ExtractedObject
{
    std::string ClassName;
    uint64 TemplateId = 0;
    uint32 ObjectId = 0;
    PropertyTypes::Vector3D Location;
    PropertyTypes::Vector3D Orientation;
    float Scale = 1.0f;
    std::string ZoneTag;
    std::string StartState;
    std::string OverrideName;
    bool GlobalDynamic = false;
    bool Undetectable = false;
    std::optional<std::vector<uint8>> SpawnRequirements;
    int64 LoadingType = 0;
};

struct ExtractedVolume
{
    std::string Name;
    uint32 ObjectId = 0;
    uint64 TemplateId = 0;
    std::string Shape;
    PropertyTypes::Vector3D Position;
    float Radius = 0.0f;
    float Length = 0.0f;
    float Width = 0.0f;
    float Depth = 0.0f;
    bool QuestEvents = false;
    bool PlayerOnly = false;
    int64 LoadingType = 0;
    std::optional<std::vector<uint8>> SpawnRequirements;
    std::vector<std::string> EnterEvents;
    std::vector<std::string> ExitEvents;
};

struct ExtractedTriggerResult
{
    uint32 ClassHash = 0;
    std::optional<std::string> ClassName;
    std::optional<std::vector<uint8>> Data;
};

struct ExtractedStateTrigger
{
    std::string QuestEvent;
    std::string RequiredQuest;
    std::string RequiredState;
    bool Unnamed333662217 = false;
    bool Unnamed758563334 = false;
    std::vector<std::string> Unnamed1521843245;
    std::string Unnamed3350245995;
    std::string Unnamed3431571632;
};

struct ExtractedTrigger
{
    std::string Name;
    std::string ClassName;
    int32 TriggerMax = -1;
    float Cooldown = 0.0f;
    int32 Unnamed780900737 = 0;
    bool Unnamed847435658 = false;
    std::string Unnamed1549045087;
    std::string Unnamed2293879431;
    std::optional<std::vector<uint8>> Requirements;
    std::optional<std::vector<uint8>> ObjectInfo;
    std::vector<std::string> ActivateEvents;
    std::vector<std::string> FireEvents;
    std::vector<std::string> DeactivateEvents;
    std::vector<ExtractedTriggerResult> Results;
    std::vector<ExtractedTriggerResult> CooldownResults;
    std::optional<ExtractedStateTrigger> State;
};

struct ExtractedSpawnItem
{
    uint32 PercentChance = 0;
    ExtractedObject Object;
    int64 StartNodeType = 0;
    uint32 StartNode = 0;
    uint64 PathId = 0;
    int32 UniqueLoc = 0;
};

struct ExtractedSpawner
{
    std::string Name;
    uint64 Id = 0;
    bool Active = false;
    bool PopSensitive = false;
    uint32 MaxSpawns = 0;
    bool AtLeastOneSpawn = false;
    bool ActivateAtMax = false;
    int32 SpawnTime = 0;
    uint32 RespawnRate = 0;
    bool GlobalDynamic = false;
    bool WaitForTimer = false;
    uint32 ZoneLevelMin = 0;
    uint32 ZoneLevelMax = 0;
    uint32 ZoneLevelUp = 0;
    std::optional<std::vector<uint8>> GlobalDynamicReqs;
    std::vector<ExtractedSpawnItem> Items;
};

struct ExtractedPathNode
{
    uint64 Id = 0;
    PropertyTypes::Vector3D Location;
    float Radius = 0.0f;
    float Direction = 0.0f;
    float Roll = 0.0f;
};

struct ExtractedPath
{
    uint64 Id = 0;
    std::string Name;
    std::vector<ExtractedPathNode> Nodes;
};

struct ExtractedZone
{
    std::string Path;
    std::string DisplayNameKey;
    float FarClip = 0.0f;
    int32 HealingPerMinute = 0;
    int32 SoftLimit = 0;
    int32 HardLimit = 0;
    bool NoMounts = false;
    std::vector<ExtractedLocation> Locations;
    std::vector<ExtractedObject> Objects;
    std::vector<ExtractedVolume> Volumes;
    std::vector<ExtractedTrigger> Triggers;
    std::vector<ExtractedSpawner> Spawners;
    std::vector<ExtractedPath> Paths;
};

struct SkippedZonePart
{
    std::string Zone;
    std::string Path;
    uint32 ClassHash = 0;
    bool WholeObject = false;
};

struct TriggerFileFailure
{
    std::string Zone;
    std::string File;
    std::string Detail;
};

struct ZoneExtraction
{
    static constexpr std::size_t MaxReportedErrors = 100;

    std::vector<ExtractedZone> Zones;
    std::vector<SkippedZonePart> Skipped;
    std::vector<TriggerFileFailure> TriggerFailures;
    std::size_t Archives = 0;
    std::vector<std::string> Errors;
    std::size_t ErrorCount = 0;

    bool Ok() const noexcept { return ErrorCount == 0; }
    void AddError(std::string error);
    void FinishErrors();
    std::size_t GetLocationCount() const noexcept;
    std::size_t GetObjectCount() const noexcept;
    std::size_t GetSkippedObjectCount() const noexcept;
    std::size_t GetVolumeCount() const noexcept;
    std::size_t GetTriggerCount() const noexcept;
    std::size_t GetSpawnerCount() const noexcept;
    std::size_t GetPathCount() const noexcept;
    std::size_t GetTriggerFailureZoneCount() const;
    ExtractedZone const* Find(std::string_view path) const noexcept;
};

using ZoneExtractionProgress = std::function<void(std::size_t archivesRead, std::size_t archives)>;

class ZoneExtractor
{
public:
    static constexpr std::string_view DataEntry = "gamedata.bin";
    static constexpr std::string_view VolumeEntry = "volumes.xml";
    static constexpr std::string_view TriggerEntry = "triggers.xml";
    static constexpr std::string_view SpawnEntry = "spawnData.xml";
    static constexpr std::string_view PathEntry = "pathData.xml";
    static constexpr std::string_view PathNodeEntry = "pathNodeData.bin";
    static constexpr std::size_t MaxEntryBytes = 64 * 1024 * 1024;

    static ZoneExtraction Extract(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, ZoneExtractionProgress const& progress = {});
    static std::optional<ZoneExtraction> ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error,
        ZoneExtractionProgress const& progress = {}, TypeDumpLoader::RawDump supplement = {});
    static void ReadZone(TypeCatalogPtr const& catalog, std::string_view archiveStem, std::span<uint8 const> data, ZoneExtraction& extraction);
    static void ReadVolumes(TypeCatalogPtr const& catalog, ExtractedZone& zone, std::span<uint8 const> data, ZoneExtraction& extraction);
    static void ReadTriggers(TypeCatalogPtr const& catalog, ExtractedZone& zone, std::span<uint8 const> data, ZoneExtraction& extraction);
    static void ReadSpawns(TypeCatalogPtr const& catalog, ExtractedZone& zone, std::span<uint8 const> data, ZoneExtraction& extraction);
    static void ReadPaths(TypeCatalogPtr const& catalog, ExtractedZone& zone, std::span<uint8 const> paths, std::span<uint8 const> nodes, ZoneExtraction& extraction);
    static std::string ArchiveStemOf(std::string_view zonePath);
};

#endif
