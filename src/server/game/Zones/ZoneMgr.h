/*
 * Project Ambrose by Imjustchico
 * The zones a world is made of, held in memory (sZoneMgr): the template of every zone the extractor read from the user's own install, the named places inside each one, and the objects placed in them, each a store of its own that is rebuilt beside the one serving and swapped in whole, so a reader holding a snapshot keeps the whole generation it started with and a build that fails leaves what was serving exactly where it was. A place is asked for by zone and name, and a name that zone does not have falls back to its Start, because a wizard sent to a door that no longer exists must still stand somewhere; a zone nothing knows is a typed refusal rather than a guess, because putting a wizard in a zone this server cannot load would strand them. What a direction means is taken only as far as the data says: a location's direction is one float, which is its yaw, and an object's is whatever shape the extractor wrote, so a yaw is offered only where the shape gives one without a convention nobody has established.
 */

#ifndef AMBROSE_ZONEMGR_H
#define AMBROSE_ZONEMGR_H

#include "ReloadableStore.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PreparedResultSet;

struct ZoneTemplate
{
    std::string Path;
    std::string DisplayNameKey;
    std::optional<float> FarClip;
    std::optional<int32> HealingPerMinute;
    std::optional<int32> SoftLimit;
    std::optional<int32> HardLimit;
    bool NoMounts = false;
};

struct ZoneLocation
{
    uint64 Id = 0;
    std::string Name;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    std::optional<float> Yaw;
};

struct ZoneObjectSpawn
{
    uint64 Id = 0;
    uint32 ObjectId = 0;
    std::optional<uint64> TemplateId;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    std::optional<float> Yaw;
    std::optional<float> Scale;
    std::string Tag;
    std::optional<int64> StartState;
    std::optional<int64> LoadingType;
};

enum class ZoneLookup : uint8
{
    Ok,
    UnknownZone,
    NoLocations,
    FellBackToStart
};

struct ZonePlace
{
    ZoneLookup Result = ZoneLookup::UnknownZone;
    ZoneLocation Location;

    bool Found() const noexcept { return Result == ZoneLookup::Ok || Result == ZoneLookup::FellBackToStart; }
};

class ZoneTemplates
{
public:
    ZoneTemplates() = default;
    explicit ZoneTemplates(std::map<std::string, ZoneTemplate, std::less<>> byPath);

    ZoneTemplate const* Find(std::string_view path) const;
    bool Has(std::string_view path) const;
    std::size_t Count() const noexcept;

private:
    std::map<std::string, ZoneTemplate, std::less<>> _byPath;
};

class ZoneLocations
{
public:
    static constexpr std::string_view StartName = "Start";

    ZoneLocations() = default;
    explicit ZoneLocations(std::map<std::string, std::vector<ZoneLocation>, std::less<>> byZone);

    std::vector<ZoneLocation> const* In(std::string_view zone) const;
    ZonePlace Find(std::string_view zone, std::string_view name) const;
    std::size_t Count() const noexcept;
    std::size_t ZoneCount() const noexcept;

private:
    std::map<std::string, std::vector<ZoneLocation>, std::less<>> _byZone;
};

class ZoneObjects
{
public:
    ZoneObjects() = default;
    explicit ZoneObjects(std::map<std::string, std::vector<ZoneObjectSpawn>, std::less<>> byZone);

    std::vector<ZoneObjectSpawn> const* In(std::string_view zone) const;
    std::size_t Count() const noexcept;
    std::size_t ZoneCount() const noexcept;

private:
    std::map<std::string, std::vector<ZoneObjectSpawn>, std::less<>> _byZone;
};

struct ZoneLoadResult
{
    bool Loaded = false;
    std::size_t Zones = 0;
    std::size_t Locations = 0;
    std::size_t Objects = 0;
    std::chrono::milliseconds Took{ 0 };
    std::vector<std::string> Errors;
};

class ZoneMgr
{
public:
    static constexpr std::string_view TemplateTarget = "zone_template";
    static constexpr std::string_view LocationTarget = "zone_location";
    static constexpr std::string_view ObjectTarget = "zone_object";
    static constexpr std::size_t MaxReportedErrors = 20;

    static ZoneMgr& Instance();

    ZoneMgr(ZoneMgr const&) = delete;
    ZoneMgr& operator=(ZoneMgr const&) = delete;

    void RegisterReloadTargets();
    ZoneLoadResult LoadAll();

    bool LoadTemplates(std::vector<std::string>& errors);
    bool LoadLocations(std::vector<std::string>& errors);
    bool LoadObjects(std::vector<std::string>& errors);

    std::shared_ptr<ZoneTemplates const> GetTemplates() const { return _templates.Get(); }
    std::shared_ptr<ZoneLocations const> GetLocations() const { return _locations.Get(); }
    std::shared_ptr<ZoneObjects const> GetObjects() const { return _objects.Get(); }

    ZonePlace FindPlace(std::string_view zone, std::string_view name) const;
    std::optional<std::string> Describe(std::string_view zone) const;
    void Clear();

    static ZoneTemplates ReadTemplates(PreparedResultSet* result, std::vector<std::string>& errors);
    static ZoneLocations ReadLocations(PreparedResultSet* result, ZoneTemplates const& templates, std::vector<std::string>& errors);
    static ZoneObjects ReadObjects(PreparedResultSet* result, ZoneTemplates const& templates, std::vector<std::string>& errors);
    static std::string_view GetLookupName(ZoneLookup lookup) noexcept;

private:
    ZoneMgr() = default;

    ReloadableStore<ZoneTemplates> _templates;
    ReloadableStore<ZoneLocations> _locations;
    ReloadableStore<ZoneObjects> _objects;
};

#define sZoneMgr ZoneMgr::Instance()

#endif
