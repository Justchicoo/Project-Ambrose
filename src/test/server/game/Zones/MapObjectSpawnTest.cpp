/*
 * Project Ambrose by Imjustchico
 * Tests how a zone instance comes to hold its zone's objects, on classes the test lays out the way the client's are: a row whose object the server sends becomes an object whose MSG_NEWOBJECT Data reads back as the class its template's class builds, opening with that template's core type and template type, standing and facing where the row says, with the mobile and perm ids it was given and one behavior slot for each the template names; rows the client builds itself are left to it; a row naming a template that cannot be read is reported and skipped while the instance still loads, and logged once however many instances and refreshes meet it; a template marked Critical makes its object one the client waits for, and those objects' ids travel as a CriticalObjectList that opens with its class hash, as the object Data opens with its core header, neither wrapped in an envelope; rows added and deleted by a reload change the live instance and nothing else, an edited row's object is replaced, and so is an object whose template reloads differently or can no longer be read, while one whose template reads back the same is kept, a second pass over the same rows changes nothing, and an instance that already holds the generations being served is left as it is, which is all a failed reload leaves behind.
 */

#include "CoreObjectSerializer.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "Map.h"
#include "MapMgr.h"
#include "MapObjectSpawner.h"
#include "ObjectFields.h"
#include "ObjectGuid.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "ObjectViews.h"
#include "StringHash.h"
#include "TestAppender.h"
#include "TypedView.h"
#include "ZoneMgr.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <map>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 8 | 16;
    constexpr uint32 WirePublic = Wire | 4;
    constexpr uint32 Kept = 1 | 2 | 4;
    constexpr char const* Hub = "WizardCity/WC_Hub";
    constexpr uint32 KioskTemplate = 4336;
    constexpr uint32 DoorTemplate = 1451036;

    Json Property(std::string const& type, std::string const& name, uint32 id, uint32 flags = WirePublic, std::string const& container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    Json ObjectProperties()
    {
        Json properties = Json::object();
        properties["m_inactiveBehaviors"] = Property("class SharedPointer<class BehaviorInstance>", "m_inactiveBehaviors", 0, WirePublic, "List");
        properties["m_globalID.m_full"] = Property("unsigned __int64", "m_globalID.m_full", 1);
        properties["m_permID"] = Property("unsigned __int64", "m_permID", 2);
        properties["m_location"] = Property("class Vector3D", "m_location", 3);
        properties["m_orientation"] = Property("class Vector3D", "m_orientation", 4);
        properties["m_fScale"] = Property("float", "m_fScale", 5);
        properties["m_templateID.m_full"] = Property("unsigned __int64", "m_templateID.m_full", 6);
        properties["m_nMobileID"] = Property("unsigned short", "m_nMobileID", 7);
        return properties;
    }

    Json TemplateProperties()
    {
        Json properties = Json::object();
        properties["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, Kept, "List");
        properties["m_objectName"] = Property("std::string", "m_objectName", 1, Kept);
        properties["m_templateID"] = Property("unsigned int", "m_templateID", 2, Kept);
        properties["m_visualID"] = Property("unsigned int", "m_visualID", 3, Kept);
        properties["m_adjectiveList"] = Property("std::string", "m_adjectiveList", 4, Kept, "List");
        properties["m_exemptFromAOI"] = Property("bool", "m_exemptFromAOI", 5, Kept);
        properties["m_displayName"] = Property("std::string", "m_displayName", 6, Kept);
        properties["m_description"] = Property("std::string", "m_description", 7, Kept);
        Json type = Property("enum ObjectType", "m_nObjectType", 8, Kept);
        type["enum_options"] = Json{ { "OBJECT_TYPE_UNKNOWN", 0 } };
        properties["m_nObjectType"] = type;
        properties["m_sIcon"] = Property("std::string", "m_sIcon", 9, Kept);
        return properties;
    }

    std::string Dump()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        AddClass(classes, "class Vector3D", Json::array(), Json::object());
        AddClass(classes, "enum ObjectType", Json::array(), Json::object());
        AddClass(classes, "class CoreObject", Json::array({ "PropertyClass" }), ObjectProperties());
        AddClass(classes, "class ClientObject", Json::array({ "CoreObject", "PropertyClass" }), ObjectProperties());
        AddClass(classes, "class WizClientObject", Json::array({ "ClientObject", "CoreObject", "PropertyClass" }), ObjectProperties());
        Json instance = Json::object();
        instance["m_behaviorTemplateNameID"] = Property("unsigned int", "m_behaviorTemplateNameID", 0, Kept);
        AddClass(classes, "class BehaviorInstance", Json::array({ "PropertyClass" }), instance);
        AddClass(classes, "class TestRenderBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), instance);
        Json behavior = Json::object();
        behavior["m_behaviorName"] = Property("std::string", "m_behaviorName", 0, Kept);
        AddClass(classes, "class BehaviorTemplate", Json::array({ "PropertyClass" }), behavior);
        Json core = Json::object();
        core["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, Kept, "List");
        AddClass(classes, "class CoreTemplate", Json::array({ "PropertyClass" }), core);
        AddClass(classes, "class GameObjectTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), TemplateProperties());
        AddClass(classes, "class WizGameObjectTemplate", Json::array({ "GameObjectTemplate", "CoreTemplate", "PropertyClass" }), TemplateProperties());
        Json critical = Json::object();
        critical["m_objList"] = Property("gid", "m_objList", 0, WirePublic, "List");
        AddClass(classes, "class CriticalObjectList", Json::array({ "PropertyClass" }), critical);
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    struct CapturedLog
    {
        CapturedLog() : Store(std::make_shared<TestAppenderStore>())
        {
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(Store));
            sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n"));
        }

        ~CapturedLog()
        {
            sLog.Reset();
        }

        std::size_t Count(std::string_view text) const
        {
            std::size_t found = 0;
            for (LogMessage const& message : Store->Messages("Capture"))
                if (message.Text.find(text) != std::string::npos)
                    ++found;
            return found;
        }

        std::shared_ptr<TestAppenderStore> Store;
    };

    ZoneObjectSpawn Row(uint64 id, uint32 templateId, ZoneObjectLoading loading, PropertyTypes::Vector3D position)
    {
        ZoneObjectSpawn row;
        row.Id = id;
        row.ClassName = "class CoreObjectInfo";
        row.TemplateId = templateId;
        row.ObjectId = static_cast<uint32>(100000 + id);
        row.Position = position;
        row.Orientation = { 0.0f, 0.0f, 0.5f };
        row.Scale = 1.25f;
        row.Loading = loading;
        return row;
    }

    class MapObjectSpawnTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _views.Add(GameObjectTemplateView::Definition);
            _registry = std::make_unique<TypeRegistry>(&_views);
            ASSERT_TRUE(_registry->LoadFromText(Dump(), "spawn.json")) << _registry->GetErrors().front();
            _catalog = _registry->GetCatalog();
            std::vector<std::string> errors;
            _types = CoreObjectTypeTable::Build({ { 2, "class ClientObject" }, { 104, "class WizClientObject" } },
                { { "class GameObjectTemplate", 2, 2 }, { "class WizGameObjectTemplate", 104, 2 } }, *_catalog, errors);
            ASSERT_TRUE(_types) << (errors.empty() ? std::string() : errors.front());
            _behaviors = BehaviorClientClasses::Build({ { "RenderBehavior", "class TestRenderBehavior", 0 }, { "DeletedBehavior", std::nullopt, 0 } }, *_catalog, errors);
            ASSERT_TRUE(_behaviors) << (errors.empty() ? std::string() : errors.front());
            _templates[KioskTemplate] = Template(KioskTemplate, "class WizGameObjectTemplate", { "RenderBehavior", "", "DeletedBehavior" }, {});
            _templates[DoorTemplate] = Template(DoorTemplate, "class GameObjectTemplate", { "RenderBehavior" }, { "Critical" });
        }

        std::shared_ptr<ObjectTemplate const> Template(uint32 id, std::string const& className, std::vector<std::string> behaviors, std::vector<std::string> const& adjectives)
        {
            PropertyObjectPtr object = PropertyObject::Create(_catalog, className);
            EXPECT_TRUE(object) << className;
            if (!object)
                return nullptr;
            PropertyValue::List list;
            for (std::string const& adjective : adjectives)
                list.emplace_back(adjective);
            EXPECT_EQ(object->Set("m_adjectiveList", std::move(list)), PropertySetResult::Ok);
            auto made = std::make_shared<ObjectTemplate>();
            made->TemplateId = id;
            made->ObjectName = className;
            made->Behaviors = std::move(behaviors);
            made->Object = std::move(object);
            return made;
        }

        MapObjectSources Sources()
        {
            MapObjectSources sources;
            sources.Catalog = _catalog;
            sources.Types = _types;
            sources.Behaviors = _behaviors;
            sources.Templates = [this](uint32 id) -> TemplateLookup
            {
                auto const found = _templates.find(id);
                if (found == _templates.end())
                    return { nullptr, "TemplateManifest.xml does not list it" };
                return { found->second, {} };
            };
            return sources;
        }

        MapObjectChanges Reconcile(Map& map, std::vector<ZoneObjectSpawn> const& rows, uint64 generation, uint64 templates = 0)
        {
            return MapObjectSpawner::Reconcile(map, rows, MapObjectStamp{ generation, 0, 0, 0, templates }, Sources(), _now, std::chrono::milliseconds(2000));
        }

        DecodeResult Decode(MapObject const& object)
        {
            ObjectField const* const field = ObjectFields::Find("MSG_NEWOBJECT", "Data");
            EXPECT_NE(field, nullptr);
            SerializerOptions options;
            options.Mask = SerializerOptions::PublicMask;
            return field ? CoreObjectSerializer::DecodeField(_catalog, *field, object.Data, *_types, options) : DecodeResult{};
        }

        TypedViewRegistry _views;
        std::unique_ptr<TypeRegistry> _registry;
        TypeCatalogPtr _catalog;
        CoreObjectTypeTablePtr _types;
        std::shared_ptr<BehaviorClientClasses const> _behaviors;
        std::map<uint32, std::shared_ptr<ObjectTemplate const>> _templates;
        Map::Clock::time_point _now = Map::Clock::now();
    };
}

TEST_F(MapObjectSpawnTest, ARowTheServerSendsBecomesAnObjectThatReadsBackAsItsTemplatesClass)
{
    Map map(7, Hub, true);
    std::vector<ZoneObjectSpawn> const rows = { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, { 1.0f, 2.0f, 3.0f }),
        Row(2, KioskTemplate, ZoneObjectLoading::StaticClient, { 4.0f, 5.0f, 6.0f }), Row(3, KioskTemplate, ZoneObjectLoading::StaticClientServer, { 7.0f, 8.0f, 9.0f }) };
    MapObjectChanges const changes = Reconcile(map, rows, 1);
    EXPECT_TRUE(changes.Problems.empty()) << changes.Problems.front().Text;
    ASSERT_EQ(map.GetObjects().size(), 1u) << "the client builds the objects of its own static kinds from its copy of the zone";
    ASSERT_EQ(changes.Added.size(), 1u);
    MapObject const& object = map.GetObjects().front();
    EXPECT_EQ(changes.Added.front(), object.GlobalId);
    EXPECT_TRUE(ObjectGuid::IsRuntime(object.GlobalId));
    EXPECT_EQ(object.PermId, ObjectGuid::PermId(Hub, KioskTemplate, 100001));
    EXPECT_EQ(MobileIdAllocator::RangeOf(object.MobileId), MobileIdAllocator::Range::Object);
    EXPECT_FALSE(object.Critical);
    EXPECT_EQ(map.GetObjectStamp(), std::optional<MapObjectStamp>(MapObjectStamp{ 1 }));

    ASSERT_GE(object.Data.size(), 6u);
    EXPECT_EQ(std::vector<uint8>(object.Data.begin(), object.Data.begin() + 6), (std::vector<uint8>{ 104, 2, 0xF0, 0x10, 0x00, 0x00 }))
        << "the Data is the object itself, opening with its core header, because the client's MSG_NewObject handler reads it without decompressing it first";
    DecodeResult const decoded = Decode(object);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(decoded.Object->GetClass().Name, "class WizClientObject");
    ASSERT_TRUE(decoded.Header);
    EXPECT_EQ(*decoded.Header, (CoreObjectHeader{ 104, 2, KioskTemplate }));
    EXPECT_EQ(*decoded.Object->Get("m_location")->GetIf<PropertyTypes::Vector3D>(), (PropertyTypes::Vector3D{ 1.0f, 2.0f, 3.0f }));
    EXPECT_EQ(*decoded.Object->Get("m_orientation")->GetIf<PropertyTypes::Vector3D>(), (PropertyTypes::Vector3D{ 0.0f, 0.0f, 0.5f }));
    EXPECT_FLOAT_EQ(*decoded.Object->Get("m_fScale")->GetIf<float>(), 1.25f);
    EXPECT_EQ(*decoded.Object->Get("m_globalID.m_full")->GetIf<uint64>(), object.GlobalId);
    EXPECT_EQ(*decoded.Object->Get("m_permID")->GetIf<uint64>(), object.PermId);
    EXPECT_EQ(*decoded.Object->Get("m_nMobileID")->GetIf<uint16>(), object.MobileId);
    EXPECT_EQ(*decoded.Object->Get("m_templateID.m_full")->GetIf<uint64>(), KioskTemplate);
    PropertyValue::List const& behaviors = *decoded.Object->Get("m_inactiveBehaviors")->GetList();
    ASSERT_EQ(behaviors.size(), 3u) << "one slot for each behavior the template names";
    ASSERT_NE(behaviors[0].AsObject(), nullptr);
    EXPECT_EQ(behaviors[0].AsObject()->GetClass().Name, "class TestRenderBehavior");
    EXPECT_TRUE(behaviors[1].IsNullObject()) << "the template leaves this slot empty";
    EXPECT_TRUE(behaviors[2].IsNullObject()) << "the client builds nothing for this behavior";
}

TEST_F(MapObjectSpawnTest, ARowWhoseTemplateCannotBeReadIsReportedAndSkippedAndTheInstanceStillLoads)
{
    Map map(7, Hub, true);
    MapObjectChanges const changes = Reconcile(map, { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}), Row(2, 999, ZoneObjectLoading::DynamicServer, {}) }, 1);
    EXPECT_EQ(map.GetObjects().size(), 1u);
    ASSERT_EQ(changes.Problems.size(), 1u);
    EXPECT_TRUE(changes.Problems.front().MissingTemplate);
    EXPECT_EQ(changes.Problems.front().SpawnId, 2u);
    EXPECT_NE(changes.Problems.front().Text.find("template 999"), std::string::npos) << changes.Problems.front().Text;
    EXPECT_EQ(map.GetMobileIds().Held(MobileIdAllocator::Range::Object), 1u) << "a skipped row holds no mobile id";
}

TEST_F(MapObjectSpawnTest, AMissingTemplateIsLoggedOnceHoweverManyInstancesAndRefreshesMeetIt)
{
    CapturedLog log;
    sMapMgr.Clear();
    uint64 pass = 0;
    std::vector<ZoneObjectSpawn> const rows = { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}), Row(2, 999, ZoneObjectLoading::DynamicServer, {}) };
    sMapMgr.SetObjectPopulator([&](Map& map, Map::Clock::time_point now, std::chrono::milliseconds releaseDelay)
    {
        return MapObjectSpawner::Reconcile(map, rows, MapObjectStamp{ ++pass }, Sources(), now, releaseDelay);
    });
    Map const& first = sMapMgr.CreatePrivate(Hub);
    Map const& second = sMapMgr.CreatePrivate(Hub);
    sMapMgr.RefreshObjects();
    EXPECT_EQ(pass, 4u);
    EXPECT_EQ(first.GetObjects().size(), 1u);
    EXPECT_EQ(second.GetObjects().size(), 1u);
    EXPECT_EQ(log.Count("names template 999"), 1u);
    sMapMgr.Clear();
}

TEST_F(MapObjectSpawnTest, ATemplateMarkedCriticalMakesItsObjectOneTheClientWaitsFor)
{
    Map map(7, Hub, true);
    Reconcile(map, { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}), Row(2, DoorTemplate, ZoneObjectLoading::DynamicServer, {}) }, 1);
    ASSERT_EQ(map.GetObjects().size(), 2u);
    MapObject const* const door = map.FindSpawn(2);
    ASSERT_NE(door, nullptr);
    EXPECT_TRUE(door->Critical);
    EXPECT_EQ(MapObjectSpawner::CriticalIds(map), std::vector<uint64>{ door->GlobalId });
    DecodeResult const decoded = Decode(*door);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(decoded.Object->GetClass().Name, "class ClientObject") << "a GameObjectTemplate makes the client build a ClientObject";
    EXPECT_EQ(*decoded.Header, (CoreObjectHeader{ 2, 2, DoorTemplate }));
}

TEST_F(MapObjectSpawnTest, TheCriticalObjectListIsItsClassAndTheIdsUnwrapped)
{
    Map map(7, Hub, true);
    std::string problem;
    Reconcile(map, { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}) }, 1);
    EXPECT_TRUE(MapObjectSpawner::EncodeCriticalObjects(_catalog, map, problem).empty()) << "no object the client waits for means an empty field";
    EXPECT_TRUE(problem.empty()) << problem;

    Reconcile(map, { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}), Row(2, DoorTemplate, ZoneObjectLoading::DynamicServer, {}) }, 2);
    std::vector<uint8> const bytes = MapObjectSpawner::EncodeCriticalObjects(_catalog, map, problem);
    ASSERT_TRUE(problem.empty()) << problem;
    uint32 const hash = StringHash::KiStringHash("class CriticalObjectList");
    ASSERT_GE(bytes.size(), 4u);
    EXPECT_EQ(std::vector<uint8>(bytes.begin(), bytes.begin() + 4),
        (std::vector<uint8>{ static_cast<uint8>(hash), static_cast<uint8>(hash >> 8), static_cast<uint8>(hash >> 16), static_cast<uint8>(hash >> 24) }))
        << "the list opens with its class hash, because the client's MSG_LoginComplete handler reads the field straight into the plain serializer";
    ObjectField const* const field = ObjectFields::Find("MSG_LOGINCOMPLETE", "CriticalObjects");
    ASSERT_NE(field, nullptr);
    DecodeResult const decoded = ObjectSerializer::DecodeField(_catalog, *field, bytes);
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    PropertyValue::List const& ids = *decoded.Object->Get("m_objList")->GetList();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(*ids.front().GetIf<uint64>(), map.FindSpawn(2)->GlobalId);
}

TEST_F(MapObjectSpawnTest, RowsAReloadAddsAndDeletesChangeTheLiveInstanceAndNothingElse)
{
    Map map(7, Hub, true);
    Reconcile(map, { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}), Row(5, KioskTemplate, ZoneObjectLoading::DynamicServer, {}) }, 1);
    ASSERT_EQ(map.GetObjects().size(), 2u);
    uint64 const kept = map.FindSpawn(1)->GlobalId;
    uint64 const deleted = map.FindSpawn(5)->GlobalId;

    MapObjectChanges const changes = Reconcile(map, { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}), Row(6, DoorTemplate, ZoneObjectLoading::DynamicServer, {}) }, 2);
    EXPECT_EQ(changes.Removed, std::vector<uint64>{ deleted });
    ASSERT_EQ(changes.Added.size(), 1u);
    ASSERT_EQ(map.GetObjects().size(), 2u);
    EXPECT_EQ(map.FindSpawn(1)->GlobalId, kept) << "a row the reload did not touch keeps its object";
    EXPECT_EQ(map.FindSpawn(5), nullptr);
    ASSERT_NE(map.FindSpawn(6), nullptr);
    EXPECT_EQ(map.FindSpawn(6)->GlobalId, changes.Added.front());
    EXPECT_EQ(map.GetMobileIds().Cooling(), 1u) << "the deleted object's mobile id cools before it is handed out again";
    EXPECT_EQ(map.GetObjectStamp(), std::optional<MapObjectStamp>(MapObjectStamp{ 2 }));
}

TEST_F(MapObjectSpawnTest, AnEditedRowIsReplacedAndTheSameRowsTwiceChangeNothing)
{
    Map map(7, Hub, true);
    std::vector<ZoneObjectSpawn> rows = { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, { 1.0f, 1.0f, 1.0f }) };
    Reconcile(map, rows, 1);
    uint64 const before = map.FindSpawn(1)->GlobalId;
    EXPECT_FALSE(Reconcile(map, rows, 2).Changed());
    EXPECT_EQ(map.FindSpawn(1)->GlobalId, before);

    rows.front().Position = { 9.0f, 9.0f, 9.0f };
    MapObjectChanges const moved = Reconcile(map, rows, 3);
    EXPECT_EQ(moved.Removed, std::vector<uint64>{ before });
    ASSERT_EQ(moved.Added.size(), 1u);
    EXPECT_NE(moved.Added.front(), before);
    DecodeResult const decoded = Decode(*map.FindSpawn(1));
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(*decoded.Object->Get("m_location")->GetIf<PropertyTypes::Vector3D>(), (PropertyTypes::Vector3D{ 9.0f, 9.0f, 9.0f }));
}

TEST_F(MapObjectSpawnTest, ATemplateThatReloadsDifferentlyReplacesItsObjectAndOneThatReloadsTheSameKeepsIt)
{
    Map map(7, Hub, true);
    std::vector<ZoneObjectSpawn> const rows = { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}), Row(2, DoorTemplate, ZoneObjectLoading::DynamicServer, {}) };
    Reconcile(map, rows, 1, 1);
    uint64 const kiosk = map.FindSpawn(1)->GlobalId;
    uint64 const door = map.FindSpawn(2)->GlobalId;

    EXPECT_FALSE(Reconcile(map, rows, 1, 2).Changed()) << "templates that read back the same leave every object as it was";
    EXPECT_EQ(map.FindSpawn(1)->GlobalId, kiosk);

    _templates[KioskTemplate] = Template(KioskTemplate, "class WizGameObjectTemplate", { "RenderBehavior" }, {});
    MapObjectChanges const changes = Reconcile(map, rows, 1, 3);
    EXPECT_EQ(changes.Removed, std::vector<uint64>{ kiosk });
    ASSERT_EQ(changes.Added.size(), 1u);
    EXPECT_EQ(map.FindSpawn(2)->GlobalId, door) << "the template that did not change keeps its object";
    DecodeResult const decoded = Decode(*map.FindSpawn(1));
    ASSERT_TRUE(decoded.Ok()) << decoded.Detail;
    EXPECT_EQ(decoded.Object->Get("m_inactiveBehaviors")->GetList()->size(), 1u);

    _templates.erase(DoorTemplate);
    MapObjectChanges const lost = Reconcile(map, rows, 1, 4);
    EXPECT_EQ(lost.Removed, std::vector<uint64>{ door }) << "an object whose template can no longer be read leaves rather than stay as it was";
    ASSERT_EQ(lost.Problems.size(), 1u);
    EXPECT_TRUE(lost.Problems.front().MissingTemplate);
}

TEST_F(MapObjectSpawnTest, AnInstanceHoldingTheServedGenerationIsLeftAsItIs)
{
    sZoneMgr.Clear();
    Map map(7, Hub, true);
    MapObjectSpawner::Reconcile(map, { Row(1, KioskTemplate, ZoneObjectLoading::DynamicServer, {}) }, MapObjectSpawner::WorldStamp(), Sources(), _now, std::chrono::milliseconds(2000));
    ASSERT_EQ(map.GetObjects().size(), 1u);
    MapObjectChanges const changes = MapObjectSpawner::PopulateFromWorld(map, _now, std::chrono::milliseconds(2000));
    EXPECT_FALSE(changes.Changed()) << "a reload that failed replaced nothing, so the generation served is the one the instance holds";
    EXPECT_EQ(changes.DynamicZoneId, 7u);
    EXPECT_EQ(map.GetObjects().size(), 1u);
}
