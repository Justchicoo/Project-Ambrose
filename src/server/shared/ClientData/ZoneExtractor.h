/*
 * Project Ambrose by Imjustchico
 * Extracts every zone of the user's own install from the gamedata.bin its GameData archive holds: the zone's settings, each named location and one row for each entry of its object list, read through the zone views, with the zone known by the name its own data gives it, which the archive's name must match. A part of a zone whose class the type dump cannot describe is reported with its class hash and path rather than guessed at or dropped silently: an object list entry of such a class is left out, and inside an entry that is kept the part is left out of what the row holds; every other problem is reported up to a cap, either from an open GameData folder and catalog or straight from an install folder and type dump, telling a caller that asks how many archives it has read after each one.
 */

#ifndef AMBROSE_ZONEEXTRACTOR_H
#define AMBROSE_ZONEEXTRACTOR_H

#include "PropertyValue.h"
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
};

struct SkippedZonePart
{
    std::string Zone;
    std::string Path;
    uint32 ClassHash = 0;
    bool WholeObject = false;
};

struct ZoneExtraction
{
    static constexpr std::size_t MaxReportedErrors = 100;

    std::vector<ExtractedZone> Zones;
    std::vector<SkippedZonePart> Skipped;
    std::size_t Archives = 0;
    std::vector<std::string> Errors;
    std::size_t ErrorCount = 0;

    bool Ok() const noexcept { return ErrorCount == 0; }
    void AddError(std::string error);
    void FinishErrors();
    std::size_t GetLocationCount() const noexcept;
    std::size_t GetObjectCount() const noexcept;
    std::size_t GetSkippedObjectCount() const noexcept;
    ExtractedZone const* Find(std::string_view path) const noexcept;
};

using ZoneExtractionProgress = std::function<void(std::size_t archivesRead, std::size_t archives)>;

class ZoneExtractor
{
public:
    static constexpr std::string_view DataEntry = "gamedata.bin";
    static constexpr std::size_t MaxEntryBytes = 64 * 1024 * 1024;

    static ZoneExtraction Extract(std::filesystem::path const& gameData, TypeCatalogPtr const& catalog, ZoneExtractionProgress const& progress = {});
    static std::optional<ZoneExtraction> ExtractFromInstall(std::filesystem::path const& clientDir, std::filesystem::path const& typeDump, std::string& error,
        ZoneExtractionProgress const& progress = {});
    static void ReadZone(TypeCatalogPtr const& catalog, std::string_view archiveStem, std::span<uint8 const> data, ZoneExtraction& extraction);
    static std::string ArchiveStemOf(std::string_view zonePath);
};

#endif
