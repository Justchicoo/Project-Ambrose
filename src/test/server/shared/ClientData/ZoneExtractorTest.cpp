/*
 * Project Ambrose by Imjustchico
 * Tests the zone extractor on zone data the test encodes as versionable objects through a type dump it writes and reads back through a second dump that lacks one object class, as the install's sigil classes are missing from the real dump: every location and every object list entry the reader can describe becomes a row with its class, template, orientation vector, start state, override name, global dynamic and undetectable flags, loading type and spawn requirements, which the zone manager reads back from the world database, an entry of the missing class is left out and reported with its class hash, a missing part deeper inside a kept entry is reported and the entry kept, a zone whose name is not its archive's is an error, archives are read in name order, a caller that asks is told after each one, and one without gamedata.bin gives no zone, the SQL script writes NULL where an object has no requirements, and with AMBROSE_TEST_DB set the script applies twice to a new world database and loads in the zone manager with the rows it extracted.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "ObjectSerializer.h"
#include "StringHash.h"
#include "TypedView.h"
#include "WorldSqlScript.h"
#include "ZoneExtractor.h"
#include "ZoneMgr.h"
#include "ZoneScript.h"
#include "ZoneViews.h"

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 1 | 2 | 4;
    constexpr char const* Hub = "WizardCity/WC_Hub";
    constexpr char const* HubArchive = "WizardCity-WC_Hub";
    constexpr char const* Sigil = "class MinigameSigilInfo";

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string const& container)
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, std::vector<std::string> const& bases, std::vector<std::pair<std::string, std::string>> const& fields,
        std::vector<std::string> const& lists = {})
    {
        Json properties = Json::object();
        uint32 id = 0;
        for (auto const& [type, field] : fields)
        {
            bool const list = std::find(lists.begin(), lists.end(), field) != lists.end();
            properties[field] = Property(type, field, id++, list ? "List" : "Static");
            if (type.starts_with("enum "))
                properties[field]["enum_options"] = Json{ { "STATIC_CLIENT_SERVER", 0 }, { "STATIC_CLIENT", 1 }, { "STATIC_SERVER", 2 }, { "DYNAMIC_SERVER", 3 } };
        }
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", bases }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    std::vector<std::pair<std::string, std::string>> ObjectInfoFields()
    {
        return { { "unsigned __int64", "m_templateID.m_full" }, { "unsigned int", "m_nObjectID" }, { "class Vector3D", "m_location" }, { "class Vector3D", "m_orientation" },
            { "float", "m_fScale" }, { "std::string", "m_zoneTag" }, { "std::string", "m_startState" }, { "std::string", "m_overrideName" }, { "bool", "m_globalDynamic" },
            { "bool", "m_bUndetectable" }, { "class SharedPointer<class RequirementList>", "m_spawnRequirements" }, { "enum CoreObjectInfo::LoadingType", "m_loadingType" } };
    }

    std::string ZoneDump(bool withMissingClasses)
    {
        Json classes = Json::object();
        classes[std::to_string(StringHash::KiStringHash("class PropertyClass"))] = Json{ { "name", "class PropertyClass" }, { "bases", Json::array() },
            { "hash", StringHash::KiStringHash("class PropertyClass") }, { "properties", Json::object() } };
        for (char const* name : { "class Vector3D", "enum CoreObjectInfo::LoadingType" })
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", Json::array() }, { "hash", StringHash::KiStringHash(name) },
                { "properties", Json::object() } };
        std::vector<std::string> const plain{ "class PropertyClass" };
        AddClass(classes, "class Requirement", plain, {});
        AddClass(classes, "class RequirementList", plain, { { "class Requirement*", "m_requirements" } }, { "m_requirements" });
        AddClass(classes, "class LocationTemplate", plain, { { "std::string", "m_locName" }, { "class Vector3D", "m_location" }, { "float", "m_direction" } });
        AddClass(classes, "class CoreObjectInfo", plain, ObjectInfoFields());
        std::vector<std::pair<std::string, std::string>> emitter = ObjectInfoFields();
        emitter.emplace_back("float", "m_radius");
        AddClass(classes, "class PositionalSoundEmitterInfo", { "class CoreObjectInfo", "class PropertyClass" }, emitter);
        if (withMissingClasses)
        {
            std::vector<std::pair<std::string, std::string>> sigil = ObjectInfoFields();
            sigil.emplace_back("std::string", "m_sigilName");
            AddClass(classes, Sigil, { "class CoreObjectInfo", "class PropertyClass" }, sigil);
            AddClass(classes, "class ReqQuestState", { "class Requirement", "class PropertyClass" }, { { "unsigned int", "m_questID" } });
        }
        AddClass(classes, "class WizZoneData", plain, { { "std::string", "m_zoneName" }, { "std::string", "m_zoneDisplayName" }, { "class LocationTemplate", "m_locationList" },
            { "class SharedPointer<class CoreObjectInfo>", "m_objectList" }, { "int", "m_healingPerMinute" }, { "int", "m_nSoftLimit" }, { "int", "m_nHardLimit" },
            { "float", "m_farClip" }, { "bool", "m_noMounts" } }, { "m_locationList", "m_objectList" });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    struct Placed
    {
        std::string ClassName;
        uint64 TemplateId = 0;
        uint32 ObjectId = 0;
        PropertyTypes::Vector3D Location;
        PropertyTypes::Vector3D Orientation;
        std::string StartState;
        int64 Loading = 0;
        bool Requirements = false;
        bool UnknownRequirement = false;
        std::string OverrideName = {};
        bool GlobalDynamic = false;
        bool Undetectable = false;
    };

    class ZoneExtractorTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _writer = std::make_unique<TypeRegistry>();
            ASSERT_TRUE(_writer->LoadFromText(ZoneDump(true), "writer.json")) << _writer->GetErrors().front();
            ZoneViews::RegisterAll(_views);
            _reader = std::make_unique<TypeRegistry>(&_views);
            ASSERT_TRUE(_reader->LoadFromText(ZoneDump(false), "reader.json")) << _reader->GetErrors().front();
            _objects = {
                { "class CoreObjectInfo", 4336, 114611, { -6094.86f, -1074.56f, 225.58f }, { 0.0f, 0.0f, 0.5387f }, "", 3, true, false, "Kiosk Keeper", true, true },
                { Sigil, 4400, 114612, { 1.0f, 2.0f, 3.0f }, { 0.0f, 0.0f, 0.0f }, "", 3, false, false },
                { "class PositionalSoundEmitterInfo", 2960, 0, { 4401.12f, 573.98f, -75.28f }, { 0.0f, 0.0f, 0.0f }, "Playing", 1, false, false },
                { "class CoreObjectInfo", 1451035, 114613, { 10.0f, 20.0f, 30.0f }, { 0.1f, 0.2f, 1.25f }, "Idle", 3, true, true },
            };
        }

        PropertyObjectPtr Create(std::string const& type)
        {
            PropertyObjectPtr object = PropertyObject::Create(_writer->GetCatalog(), type);
            EXPECT_TRUE(object) << type;
            return object;
        }

        PropertyObjectPtr Requirements(bool unknown)
        {
            PropertyObjectPtr list = Create("class RequirementList");
            PropertyValue::List requirements;
            requirements.emplace_back(Create("class Requirement"));
            if (unknown)
            {
                PropertyObjectPtr quest = Create("class ReqQuestState");
                EXPECT_EQ(quest->Set("m_questID", uint32{ 77 }), PropertySetResult::Ok);
                requirements.emplace_back(std::move(quest));
            }
            EXPECT_EQ(list->Set("m_requirements", std::move(requirements)), PropertySetResult::Ok);
            return list;
        }

        std::vector<uint8> ZoneData(std::string const& name)
        {
            PropertyObjectPtr zone = Create("class WizZoneData");
            EXPECT_EQ(zone->Set("m_zoneName", name), PropertySetResult::Ok);
            EXPECT_EQ(zone->Set("m_zoneDisplayName", std::string("WizardZone_TheCommons")), PropertySetResult::Ok);
            EXPECT_EQ(zone->Set("m_healingPerMinute", int32{ 20 }), PropertySetResult::Ok);
            EXPECT_EQ(zone->Set("m_nSoftLimit", int32{ 50 }), PropertySetResult::Ok);
            EXPECT_EQ(zone->Set("m_nHardLimit", int32{ 100 }), PropertySetResult::Ok);
            EXPECT_EQ(zone->Set("m_farClip", 24500.0f), PropertySetResult::Ok);
            PropertyValue::List locations;
            for (auto const& [locationName, x, direction] : { std::tuple{ "Start", 707.2f, -0.75f }, std::tuple{ "Target location (WC_Hub Street1 Exit)", -5.0f, 1.5f } })
            {
                PropertyObjectPtr location = Create("class LocationTemplate");
                EXPECT_EQ(location->Set("m_locName", std::string(locationName)), PropertySetResult::Ok);
                EXPECT_EQ(location->Set("m_location", PropertyTypes::Vector3D{ x, -231.1f, -30.5f }), PropertySetResult::Ok);
                EXPECT_EQ(location->Set("m_direction", direction), PropertySetResult::Ok);
                locations.emplace_back(std::move(location));
            }
            EXPECT_EQ(zone->Set("m_locationList", std::move(locations)), PropertySetResult::Ok);
            PropertyValue::List objects;
            for (Placed const& placed : _objects)
            {
                PropertyObjectPtr object = Create(placed.ClassName);
                EXPECT_EQ(object->Set("m_templateID.m_full", placed.TemplateId), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_nObjectID", placed.ObjectId), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_location", placed.Location), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_orientation", placed.Orientation), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_fScale", 1.0f), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_zoneTag", fmt::format("tag {}", placed.ObjectId)), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_startState", placed.StartState), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_loadingType", placed.Loading), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_overrideName", placed.OverrideName), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_globalDynamic", placed.GlobalDynamic), PropertySetResult::Ok);
                EXPECT_EQ(object->Set("m_bUndetectable", placed.Undetectable), PropertySetResult::Ok);
                if (placed.Requirements)
                {
                    EXPECT_EQ(object->Set("m_spawnRequirements", Requirements(placed.UnknownRequirement)), PropertySetResult::Ok);
                }
                objects.emplace_back(std::move(object));
            }
            EXPECT_EQ(zone->Set("m_objectList", std::move(objects)), PropertySetResult::Ok);
            SerializerOptions options;
            options.Versionable = true;
            options.Flags = SerializerFlag::None;
            options.Mask = 0;
            EncodeResult encoded = ObjectSerializer::Encode(zone.get(), options);
            EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
            return std::move(encoded.Bytes);
        }

        void WriteArchive(std::string const& stem, std::vector<uint8> const* data)
        {
            KiwadBuilder builder(2);
            if (data)
                builder.Add(std::string(ZoneExtractor::DataEntry), *data, true);
            else
                builder.Add("collision.xml", std::string_view("<collision/>"), false);
            std::vector<uint8> const archive = builder.Build();
            std::ofstream(_directory.Path() / (stem + ".wad"), std::ios::binary).write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
        }

        ZoneExtraction ReadHub(std::string const& name = Hub)
        {
            ZoneExtraction extraction;
            std::vector<uint8> const data = ZoneData(name);
            ZoneExtractor::ReadZone(_reader->GetCatalog(), HubArchive, data, extraction);
            return extraction;
        }

        static std::string Report(ZoneExtraction const& extraction)
        {
            std::string report;
            for (std::string const& error : extraction.Errors)
                report += error + "\n";
            return report;
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        std::unique_ptr<TypeRegistry> _writer;
        std::unique_ptr<TypeRegistry> _reader;
        std::vector<Placed> _objects;
    };
}

TEST_F(ZoneExtractorTest, EachLocationAndEachEntryTheDumpDescribesBecomesARow)
{
    ZoneExtraction const extraction = ReadHub();
    ASSERT_TRUE(extraction.Ok()) << Report(extraction);
    ASSERT_EQ(extraction.Zones.size(), 1u);
    ExtractedZone const& zone = extraction.Zones.front();
    EXPECT_EQ(zone.Path, Hub);
    EXPECT_EQ(zone.DisplayNameKey, "WizardZone_TheCommons");
    EXPECT_EQ(zone.HealingPerMinute, 20);
    EXPECT_EQ(zone.SoftLimit, 50);
    EXPECT_EQ(zone.HardLimit, 100);
    EXPECT_FLOAT_EQ(zone.FarClip, 24500.0f);
    ASSERT_EQ(zone.Locations.size(), 2u);
    EXPECT_EQ(zone.Locations[1].Name, "Target location (WC_Hub Street1 Exit)");
    EXPECT_FLOAT_EQ(zone.Locations[0].Direction, -0.75f);
    EXPECT_EQ(zone.Locations[0].Location, (PropertyTypes::Vector3D{ 707.2f, -231.1f, -30.5f }));

    ASSERT_EQ(zone.Objects.size(), 3u) << "the sigil entry the reader's dump cannot describe is left out";
    ExtractedObject const& kiosk = zone.Objects[0];
    EXPECT_EQ(kiosk.ClassName, "class CoreObjectInfo");
    EXPECT_EQ(kiosk.TemplateId, 4336u);
    EXPECT_EQ(kiosk.ObjectId, 114611u);
    EXPECT_EQ(kiosk.Orientation, (PropertyTypes::Vector3D{ 0.0f, 0.0f, 0.5387f })) << "the orientation stays the vector the zone gives";
    EXPECT_EQ(kiosk.ZoneTag, "tag 114611");
    EXPECT_EQ(kiosk.LoadingType, 3);
    EXPECT_EQ(kiosk.OverrideName, "Kiosk Keeper");
    EXPECT_TRUE(kiosk.GlobalDynamic);
    EXPECT_TRUE(kiosk.Undetectable);
    ASSERT_TRUE(kiosk.SpawnRequirements.has_value());
    SerializerOptions versionable;
    versionable.Versionable = true;
    versionable.Mask = 0;
    DecodeResult const requirements = ObjectSerializer::Decode(_reader->GetCatalog(), *kiosk.SpawnRequirements, versionable);
    ASSERT_TRUE(requirements.Ok()) << requirements.Detail;
    EXPECT_EQ(requirements.Object->GetClass().Name, "class RequirementList") << "the requirements are kept as the bytes the zone data holds them in";
    ExtractedObject const& emitter = zone.Objects[1];
    EXPECT_EQ(emitter.ClassName, "class PositionalSoundEmitterInfo") << "a subclass of CoreObjectInfo is kept with its own class";
    EXPECT_EQ(emitter.StartState, "Playing") << "the start state is the name of a state, not a number";
    EXPECT_EQ(emitter.LoadingType, 1);
    EXPECT_FALSE(emitter.SpawnRequirements.has_value());
    EXPECT_TRUE(emitter.OverrideName.empty());
    EXPECT_FALSE(emitter.GlobalDynamic);
    EXPECT_FALSE(emitter.Undetectable);
    EXPECT_EQ(zone.Objects[2].TemplateId, 1451035u);
    EXPECT_EQ(zone.Objects[2].StartState, "Idle");
}

TEST_F(ZoneExtractorTest, AnEntryOfAClassTheDumpLacksIsReportedWithItsHash)
{
    ZoneExtraction const extraction = ReadHub();
    ASSERT_TRUE(extraction.Ok()) << Report(extraction);
    EXPECT_EQ(extraction.GetSkippedObjectCount(), 1u);
    auto const whole = std::find_if(extraction.Skipped.begin(), extraction.Skipped.end(), [](SkippedZonePart const& part) { return part.WholeObject; });
    ASSERT_NE(whole, extraction.Skipped.end());
    EXPECT_EQ(whole->Zone, Hub);
    EXPECT_EQ(whole->ClassHash, StringHash::KiStringHash(Sigil));
    EXPECT_NE(whole->Path.find("m_objectList[1]"), std::string::npos) << whole->Path;

    auto const part = std::find_if(extraction.Skipped.begin(), extraction.Skipped.end(), [](SkippedZonePart const& skipped) { return !skipped.WholeObject; });
    ASSERT_NE(part, extraction.Skipped.end()) << "a requirement the reader's dump lacks, inside an entry that is kept, is reported";
    EXPECT_EQ(part->ClassHash, StringHash::KiStringHash("class ReqQuestState"));
    EXPECT_NE(part->Path.find("m_objectList[3]"), std::string::npos) << part->Path;
    EXPECT_EQ(extraction.Zones.front().Objects.size(), 3u) << "the entry holding the unknown requirement is still a row";
}

TEST_F(ZoneExtractorTest, AZoneWhoseNameIsNotItsArchivesIsAnError)
{
    ZoneExtraction const extraction = ReadHub("WizardCity/WC_Ravenwood");
    EXPECT_FALSE(extraction.Ok());
    EXPECT_TRUE(extraction.Zones.empty());
    ASSERT_FALSE(extraction.Errors.empty());
    EXPECT_NE(extraction.Errors.front().find("WizardCity-WC_Ravenwood.wad"), std::string::npos) << extraction.Errors.front();
}

TEST_F(ZoneExtractorTest, ArchivesAreReadInNameOrderAndOneWithoutZoneDataGivesNoZone)
{
    std::vector<uint8> const hub = ZoneData(Hub);
    std::vector<uint8> const nested = ZoneData("WizardCity/Interiors/WC_Headmistress_House");
    WriteArchive(HubArchive, &hub);
    WriteArchive("WizardCity-Interiors-WC_Headmistress_House", &nested);
    WriteArchive("Root", nullptr);
    std::vector<std::pair<std::size_t, std::size_t>> told;
    ZoneExtraction const extraction = ZoneExtractor::Extract(_directory.Path(), _reader->GetCatalog(),
        [&told](std::size_t read, std::size_t archives) { told.emplace_back(read, archives); });
    ASSERT_TRUE(extraction.Ok()) << Report(extraction);
    EXPECT_EQ(extraction.Archives, 3u);
    EXPECT_EQ(told, (std::vector<std::pair<std::size_t, std::size_t>>{ { 1, 3 }, { 2, 3 }, { 3, 3 } })) << "a caller that asks is told after each archive";
    ASSERT_EQ(extraction.Zones.size(), 2u);
    EXPECT_EQ(extraction.Zones[0].Path, "WizardCity/Interiors/WC_Headmistress_House") << "every dash of a nested zone's archive is a slash of its path";
    EXPECT_EQ(extraction.Zones[1].Path, Hub);
    EXPECT_EQ(extraction.GetLocationCount(), 4u);
    EXPECT_EQ(extraction.GetObjectCount(), 6u);
    EXPECT_NE(extraction.Find(Hub), nullptr);
    EXPECT_EQ(ZoneExtractor::ArchiveStemOf("WizardCity/Interiors/WC_Headmistress_House"), "WizardCity-Interiors-WC_Headmistress_House");
}

TEST_F(ZoneExtractorTest, TheScriptReplacesTheZoneTablesAndWritesNullForNoRequirements)
{
    ZoneExtraction const extraction = ReadHub();
    ASSERT_TRUE(extraction.Ok()) << Report(extraction);
    WorldSqlScript const script = ZoneScript::Build(extraction);
    std::vector<std::string> const& statements = script.GetStatements();
    ASSERT_EQ(statements.size(), 6u);
    EXPECT_EQ(statements[0], "DELETE FROM `zone_template`");
    EXPECT_NE(statements[1].find(WorldSqlScript::Literal(std::string(Hub))), std::string::npos) << statements[1];
    EXPECT_EQ(statements[4], "DELETE FROM `zone_object`");
    EXPECT_NE(statements[5].find("`spawn_requirements`"), std::string::npos) << statements[5];
    EXPECT_NE(statements[5].find("'', 0, 0, 1, NULL)"), std::string::npos) << "the emitter's loading type is 1 and it has no spawn requirements: " << statements[5];
    EXPECT_EQ(WorldSqlScript::Literal(std::monostate{}), "NULL");
    EXPECT_EQ(ZoneScript::GetTables(), (std::vector<std::string_view>{ "zone_template", "zone_location", "zone_object" }));
}

TEST_F(ZoneExtractorTest, TheScriptAppliesTwiceAndTheZoneManagerLoadsWhatWasExtracted)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> server = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(server);
    MySQLConnectionInfo world = *server;
    world.Database = fmt::format("ambrose_extract_zones_{:08x}", std::random_device()());
    server->Database.clear();
    struct Cleanup
    {
        MySQLConnectionInfo Server;
        std::string Name;
        ~Cleanup()
        {
            sZoneMgr.Clear();
            WorldDatabase.Close();
            MySQLConnection connection(Server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(Name)));
        }
    } const cleanup{ *server, world.Database };

    ZoneExtraction const extraction = ReadHub();
    ASSERT_TRUE(extraction.Ok()) << Report(extraction);
    WorldSqlScript const script = ZoneScript::Build(extraction);
    ASSERT_TRUE(DBUpdater::Run(world, "world", UpdaterSettings{}));
    std::string error;
    for (int round = 0; round < 2; ++round)
    {
        ASSERT_TRUE(script.Apply(world, error)) << error;
        ASSERT_TRUE(WorldDatabase.SetConnectionInfo(world.ToConnectionString(), 1, 1));
        ASSERT_EQ(WorldDatabase.Open(), 0u);
        ZoneLoadResult const loaded = sZoneMgr.LoadAll();
        ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
        EXPECT_EQ(loaded.Zones, 1u);
        EXPECT_EQ(loaded.Locations, 2u);
        EXPECT_EQ(loaded.Objects, 3u);
        ZonePlace const start = sZoneMgr.FindPlace(Hub, "Start");
        ASSERT_TRUE(start.Found());
        EXPECT_FLOAT_EQ(start.Location.Yaw, -0.75f);
        std::vector<ZoneObjectSpawn> const* const objects = sZoneMgr.GetObjects()->In(Hub);
        ASSERT_NE(objects, nullptr);
        ASSERT_EQ(objects->size(), 3u);
        EXPECT_EQ(objects->at(0).TemplateId, 4336u);
        EXPECT_EQ(objects->at(0).Orientation, (PropertyTypes::Vector3D{ 0.0f, 0.0f, 0.5387f }));
        EXPECT_TRUE(objects->at(0).IsSentByServer());
        EXPECT_TRUE(objects->at(0).HasSpawnRequirements);
        EXPECT_EQ(objects->at(0).OverrideName, "Kiosk Keeper");
        EXPECT_TRUE(objects->at(0).GlobalDynamic);
        EXPECT_TRUE(objects->at(0).Undetectable);
        EXPECT_EQ(objects->at(1).ClassName, "class PositionalSoundEmitterInfo");
        EXPECT_EQ(objects->at(1).Loading, ZoneObjectLoading::StaticClient);
        EXPECT_FALSE(objects->at(1).HasSpawnRequirements);
        EXPECT_EQ(objects->at(2).StartState, "Idle");
        sZoneMgr.Clear();
        WorldDatabase.Close();
    }
}
