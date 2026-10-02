/*
 * Project Ambrose by Imjustchico
 * Removes first and adds after, so an edited row's object leaves before its new one arrives and its mobile id goes back to cool rather than being handed straight on. When the classes, tables or templates reload, each object still wanted is built again with the ids it holds and kept only if it comes out byte for byte the same. Each new object is built from its template and encoded with the Public, Transmit and AuthorityTransmit mask, the one the client reads MSG_NEWOBJECT with, so nothing only its owner may see is ever sent; a template whose adjectives include Critical marks its object as one the client waits for before it leaves the loading screen, and one with m_exemptFromAOI as one every wizard in the instance is shown wherever it stands.
 */

#include "MapObjectSpawner.h"
#include "Log.h"
#include "ObjectFields.h"
#include "ObjectGuid.h"
#include "ObjectViews.h"
#include "PropertyFiller.h"
#include "TypeRegistry.h"
#include "ZoneObjectBuilder.h"

#include <fmt/format.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    bool IsCritical(ObjectTemplate const& objectTemplate)
    {
        std::optional<GameObjectTemplateView> const view = GameObjectTemplateView::From(objectTemplate.Object.get());
        if (!view)
            return false;
        PropertyValue::List const& adjectives = view->GetAdjectiveList();
        return std::any_of(adjectives.begin(), adjectives.end(), [](PropertyValue const& adjective)
        {
            std::string const* const text = adjective.GetIf<std::string>();
            return text && *text == MapObjectSpawner::CriticalAdjective;
        });
    }

    bool IsExemptFromAoi(ObjectTemplate const& objectTemplate)
    {
        std::optional<GameObjectTemplateView> const view = GameObjectTemplateView::From(objectTemplate.Object.get());
        return view && view->IsExemptFromAoi();
    }

    void Report(MapObjectChanges& changes, ZoneObjectSpawn const& row, bool missingTemplate, std::string text)
    {
        changes.Problems.push_back({ row.Id, row.TemplateId, missingTemplate, std::move(text) });
    }

    TemplateLookup FindTemplate(MapObjectSources const& sources, ZoneObjectSpawn const& row)
    {
        return sources.Templates ? sources.Templates(static_cast<uint32>(row.TemplateId)) : TemplateLookup{ nullptr, "no template store is set" };
    }

    ZoneObjectPlacement Place(Map const& map, ZoneObjectSpawn const& row, uint64 globalId, uint16 mobileId)
    {
        ZoneObjectPlacement placement;
        placement.GlobalId = globalId;
        placement.PermId = ObjectGuid::PermId(map.GetZonePath(), row.TemplateId, row.ObjectId);
        placement.MobileId = mobileId;
        placement.Location = row.Position;
        placement.Orientation = row.Orientation;
        placement.Scale = row.Scale;
        return placement;
    }

    std::optional<std::vector<uint8>> Encode(ObjectTemplate const& objectTemplate, ZoneObjectPlacement const& placement, MapObjectSources const& sources, std::string& problem)
    {
        if (!sources.Types || !sources.Behaviors)
        {
            problem = "the core object and behavior tables are not loaded";
            return std::nullopt;
        }
        PropertyObjectPtr const object = ZoneObjectBuilder::Build(sources.Catalog, *sources.Types, *sources.Behaviors, objectTemplate, placement, problem);
        if (!object)
            return std::nullopt;
        ObjectField const* const field = ObjectFields::Find("MSG_NEWOBJECT", "Data");
        if (!field)
        {
            problem = "MSG_NEWOBJECT's Data is not declared";
            return std::nullopt;
        }
        SerializerOptions options;
        options.Mask = SerializerOptions::PublicMask;
        EncodeResult encoded = CoreObjectSerializer::EncodeField(*field, *object, *sources.Types, options);
        if (!encoded.Ok())
        {
            problem = std::move(encoded.Detail);
            return std::nullopt;
        }
        return std::move(encoded.Bytes);
    }

    bool BuildsAlike(Map const& map, MapObject const& object, MapObjectSources const& sources)
    {
        TemplateLookup const found = FindTemplate(sources, object.Spawn);
        if (!found.Template)
            return false;
        std::string problem;
        std::optional<std::vector<uint8>> const data = Encode(*found.Template, Place(map, object.Spawn, object.GlobalId, object.MobileId), sources, problem);
        return data && *data == object.Data && IsCritical(*found.Template) == object.Critical && IsExemptFromAoi(*found.Template) == object.ExemptFromAoi;
    }

    void Spawn(Map& map, ZoneObjectSpawn const& row, MapObjectSources const& sources, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay,
        MapObjectChanges& changes)
    {
        TemplateLookup const found = FindTemplate(sources, row);
        if (!found.Template)
        {
            Report(changes, row, true, fmt::format("zone_object row {} names template {}, which cannot be read: {}", row.Id, row.TemplateId, found.Error));
            return;
        }
        std::optional<uint64> const globalId = ObjectGuid::NextRuntime();
        if (!globalId)
        {
            Report(changes, row, false, fmt::format("zone_object row {} gets no object, since the runtime ids have run out", row.Id));
            return;
        }
        std::optional<uint16> const mobileId = map.GetMobileIds().Allocate(MobileIdAllocator::Range::Object, now);
        if (!mobileId)
        {
            Report(changes, row, false, fmt::format("zone_object row {} gets no object, since instance {} has no object mobile id left", row.Id, map.GetDynamicZoneId()));
            return;
        }
        ZoneObjectPlacement const placement = Place(map, row, *globalId, *mobileId);
        std::string problem;
        std::optional<std::vector<uint8>> data = Encode(*found.Template, placement, sources, problem);
        if (!data)
        {
            map.GetMobileIds().Release(*mobileId, now, releaseDelay);
            Report(changes, row, false, fmt::format("zone_object row {} with template {} gets no object: {}", row.Id, row.TemplateId, problem));
            return;
        }
        MapObject made;
        made.Spawn = row;
        made.GlobalId = *globalId;
        made.PermId = placement.PermId;
        made.MobileId = *mobileId;
        made.Critical = IsCritical(*found.Template);
        made.ExemptFromAoi = IsExemptFromAoi(*found.Template);
        made.Data = std::move(*data);
        map.AddObject(std::move(made));
        changes.Added.push_back(*globalId);
    }
}

MapObjectChanges MapObjectSpawner::Reconcile(Map& map, std::vector<ZoneObjectSpawn> const& rows, MapObjectStamp const& stamp, MapObjectSources const& sources, Map::Clock::time_point now,
    std::chrono::milliseconds releaseDelay)
{
    MapObjectChanges changes;
    changes.DynamicZoneId = map.GetDynamicZoneId();
    std::vector<ZoneObjectSpawn const*> wanted;
    for (ZoneObjectSpawn const& row : rows)
        if (row.IsSentByServer())
            wanted.push_back(&row);

    bool const rebuild = map.GetObjectStamp() && !map.GetObjectStamp()->BuildsAlike(stamp);
    std::vector<uint64> gone;
    for (MapObject const& object : map.GetObjects())
    {
        auto const kept = std::find_if(wanted.begin(), wanted.end(), [&object](ZoneObjectSpawn const* row) { return row->Id == object.Spawn.Id; });
        if (kept == wanted.end() || !(**kept == object.Spawn) || (rebuild && !BuildsAlike(map, object, sources)))
            gone.push_back(object.Spawn.Id);
    }
    for (uint64 const spawnId : gone)
        if (std::optional<MapObject> const removed = map.RemoveSpawn(spawnId, now, releaseDelay))
            changes.Removed.push_back(removed->GlobalId);

    for (ZoneObjectSpawn const* row : wanted)
        if (!map.FindSpawn(row->Id))
            Spawn(map, *row, sources, now, releaseDelay, changes);
    map.SetObjectStamp(stamp);
    return changes;
}

std::vector<uint64> MapObjectSpawner::CriticalIds(Map const& map)
{
    std::vector<uint64> critical;
    for (MapObject const& object : map.GetObjects())
        if (object.Critical)
            critical.push_back(object.GlobalId);
    return critical;
}

std::vector<uint8> MapObjectSpawner::EncodeCriticalObjects(TypeCatalogPtr const& catalog, Map const& map, std::string& problem)
{
    problem.clear();
    std::vector<uint64> const ids = CriticalIds(map);
    if (ids.empty())
        return {};
    PropertyObjectPtr const list = catalog ? PropertyObject::Create(catalog, CriticalObjectListClass) : nullptr;
    ObjectField const* const field = ObjectFields::Find("MSG_LOGINCOMPLETE", "CriticalObjects");
    if (!list || !field)
    {
        problem = !list ? fmt::format("the type dump has no {}", CriticalObjectListClass) : std::string("MSG_LOGINCOMPLETE's CriticalObjects is not declared");
        return {};
    }
    PropertyValue::List gids;
    gids.reserve(ids.size());
    for (uint64 const id : ids)
        gids.emplace_back(id);
    PropertyFiller(*list, problem).Set("m_objList", std::move(gids));
    if (!problem.empty())
        return {};
    EncodeResult encoded = ObjectSerializer::EncodeField(*field, list.get());
    if (!encoded.Ok())
    {
        problem = std::move(encoded.Detail);
        return {};
    }
    return std::move(encoded.Bytes);
}

MapObjectStamp MapObjectSpawner::WorldStamp()
{
    return MapObjectStamp{ sZoneMgr.GetObjectGeneration(), sTypeRegistry.GetGeneration(), sObjectSchemaMgr.GetCoreObjectTypeGeneration(), sObjectSchemaMgr.GetBehaviorGeneration(),
        sObjectTemplateMgr.GetGeneration() };
}

MapObjectChanges MapObjectSpawner::PopulateFromWorld(Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay)
{
    MapObjectStamp const stamp = WorldStamp();
    if (map.GetObjectStamp() == stamp)
        return MapObjectChanges{ map.GetDynamicZoneId() };
    bool const first = !map.GetObjectStamp().has_value();
    std::shared_ptr<ZoneObjects const> const objects = sZoneMgr.GetObjects();
    std::vector<ZoneObjectSpawn> const* const rows = objects ? objects->In(map.GetZonePath()) : nullptr;
    MapObjectSources sources;
    sources.Catalog = sTypeRegistry.GetCatalog();
    sources.Types = sObjectSchemaMgr.GetCoreObjectTypes();
    sources.Behaviors = sObjectSchemaMgr.GetBehaviorClientClasses();
    sources.Templates = [](uint32 templateId) { return sObjectTemplateMgr.Lookup(templateId); };
    MapObjectChanges changes = Reconcile(map, rows ? *rows : std::vector<ZoneObjectSpawn>{}, stamp, sources, now, releaseDelay);
    if (first)
        LOG_INFO("server.zones", "{} objects spawned in {}, {} of them critical", map.GetObjects().size(), map.GetZonePath(), CriticalIds(map).size());
    else if (changes.Changed())
        LOG_INFO("server.zones", "Instance {} of {} took {} object(s) away and brought {} in after what builds its objects reloaded", map.GetDynamicZoneId(), map.GetZonePath(),
            changes.Removed.size(), changes.Added.size());
    return changes;
}
