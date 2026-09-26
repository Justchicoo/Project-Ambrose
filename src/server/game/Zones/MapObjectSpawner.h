/*
 * Project Ambrose by Imjustchico
 * Keeps a zone instance's objects in line with its zone's rows: every row whose object the server sends, as the client builds the static kinds from its own copy of the zone, becomes an object with a runtime global id, the permID its zone, template and object id give it, a mobile id from the instance's object range and the MSG_NEWOBJECT Data it travels as, encoded once; a row that is gone, or no longer the row it was, takes its object with it, and so does a row whose object the classes, tables or templates now build differently, and a row a template or a table cannot build is reported with its zone row and template rather than sent half made. What changed is handed back by global id so the players already in the instance can be told, and the global ids of the objects the client waits for are encoded as the CriticalObjectList MSG_LOGINCOMPLETE carries, nothing when there are none. The world's own population reads the zone rows, templates, core object table and behavior classes the server holds, does nothing while an instance already holds the generations of all of them being served, and logs how many objects an instance was given the first time and what came and went on a later refresh.
 */

#ifndef AMBROSE_MAPOBJECTSPAWNER_H
#define AMBROSE_MAPOBJECTSPAWNER_H

#include "CoreObjectSerializer.h"
#include "Map.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct MapObjectSources
{
    TypeCatalogPtr Catalog = {};
    CoreObjectTypeTablePtr Types = {};
    std::shared_ptr<BehaviorClientClasses const> Behaviors = {};
    std::function<TemplateLookup(uint32)> Templates = {};
};

struct MapObjectProblem
{
    uint64 SpawnId = 0;
    uint64 TemplateId = 0;
    bool MissingTemplate = false;
    std::string Text = {};
};

struct MapObjectChanges
{
    uint32 DynamicZoneId = 0;
    std::vector<uint64> Removed = {};
    std::vector<uint64> Added = {};
    std::vector<MapObjectProblem> Problems = {};

    bool Changed() const noexcept { return !Removed.empty() || !Added.empty(); }
};

class MapObjectSpawner
{
public:
    static constexpr std::string_view CriticalAdjective = "Critical";
    static constexpr std::string_view CriticalObjectListClass = "class CriticalObjectList";

    MapObjectSpawner() = delete;

    static MapObjectChanges Reconcile(Map& map, std::vector<ZoneObjectSpawn> const& rows, MapObjectStamp const& stamp, MapObjectSources const& sources, Map::Clock::time_point now,
        std::chrono::milliseconds releaseDelay);
    static std::vector<uint64> CriticalIds(Map const& map);
    static std::vector<uint8> EncodeCriticalObjects(TypeCatalogPtr const& catalog, Map const& map, std::string& problem);
    static MapObjectStamp WorldStamp();
    static MapObjectChanges PopulateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay);
};

#endif
