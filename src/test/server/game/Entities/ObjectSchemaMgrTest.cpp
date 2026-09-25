/*
 * Project Ambrose by Imjustchico
 * Tests what the world database's object schema tables promise, against a real world database when AMBROSE_TEST_DB is set and a small type dump the test writes: the updates create the tables with the rows that prove the player object, a server class joins the catalog in one build and extends a class the dump describes, the core object types and behavior classes load and are found both ways, a behavior with no class stands for an empty slot, a row naming a class nothing describes or one of the wrong kind fails its reload with the row named and keeps what was serving, and a server class whose property does not hash fails its reload with the catalog left as it was.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ObjectSchemaMgr.h"
#include "ReloadMgr.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Wire = 1 | 2 | 4 | 8 | 16;
    constexpr uint32 Kept = 1 | 2 | 4 | 32;

    Json Property(std::string const& type, std::string const& name, uint32 id, uint32 flags = Wire, std::string container = "Static")
    {
        bool const pointer = type.ends_with('*') || type.starts_with("class SharedPointer<");
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    void AddClass(Json& classes, std::string const& name, Json bases, Json properties)
    {
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    std::string Dump()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), Json::object());
        Json object = Json::object();
        object["m_globalID.m_full"] = Property("unsigned __int64", "m_globalID.m_full", 0);
        AddClass(classes, "class CoreObject", Json::array({ "PropertyClass" }), object);
        AddClass(classes, "class ClientObject", Json::array({ "CoreObject", "PropertyClass" }), object);
        AddClass(classes, "class WizClientObject", Json::array({ "ClientObject", "CoreObject", "PropertyClass" }), object);
        Json instance = Json::object();
        instance["m_behaviorTemplateNameID"] = Property("unsigned int", "m_behaviorTemplateNameID", 0, Kept);
        AddClass(classes, "class BehaviorInstance", Json::array({ "PropertyClass" }), instance);
        AddClass(classes, "class TestAnimationBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), instance);
        return Json{ { "version", 2 }, { "classes", classes } }.dump();
    }

    class ObjectSchemaMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            _worldInfo = *info;
            _worldInfo.Database = fmt::format("ambrose_schema_{:08x}", std::random_device()());
            ASSERT_TRUE(DBUpdater::Run(_worldInfo, "world", UpdaterSettings{}));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_worldInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            sReloadMgr.Clear();
            sObjectSchemaMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            std::vector<std::string> cleared;
            ASSERT_TRUE(sTypeRegistry.ClearSupplement(cleared));
            sObjectSchemaMgr.RegisterReloadTargets();
        }

        void TearDown() override
        {
            sObjectSchemaMgr.Clear();
            sReloadMgr.Clear();
            sTypeRegistry.Clear();
            std::vector<std::string> cleared;
            sTypeRegistry.ClearSupplement(cleared);
            sTypeRegistry.SetViews(&sTypedViewRegistry);
            if (_open)
                WorldDatabase.Close();
            if (_worldInfo.Database.empty())
                return;
            MySQLConnectionInfo server = _worldInfo;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_worldInfo.Database)));
        }

        void Execute(std::string const& sql)
        {
            ASSERT_TRUE(WorldDatabase.DirectExecute(sql)) << sql;
        }

        void UseTestRows()
        {
            Execute("DELETE FROM `behavior_client_class`");
            Execute("DELETE FROM `core_object_type`");
            Execute("DELETE FROM `server_class`");
            uint32 const hash = StringHash::KiStringHash("TestMobileBehavior");
            Execute(fmt::format("INSERT INTO `server_class` (`hash`, `name`, `evidence`) VALUES ({}, 'TestMobileBehavior', 'test')", hash));
            Execute(fmt::format("INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES ({0}, 0, 'class BehaviorInstance'), ({0}, 1, 'class PropertyClass')", hash));
            Execute(fmt::format("INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `offset`, `flags`) VALUES ({}, 0, 'm_behaviorTemplateNameID', 'unsigned int', {}, 104, 39)",
                hash, StringHash::PropertyHash("unsigned int", "m_behaviorTemplateNameID")));
            Execute("INSERT INTO `core_object_type` (`block`, `type`, `class_name`, `evidence`) VALUES (104, 2, 'class WizClientObject', 'test')");
            Execute("INSERT INTO `behavior_client_class` (`behavior_name`, `class_name`, `evidence`) VALUES ('BasicMobileBehavior', 'TestMobileBehavior', 'test'), ('AnimationBehavior', 'class TestAnimationBehavior', 'test'), ('PathMovementBehavior', NULL, 'test')");
        }

        void LoadAll()
        {
            std::vector<std::string> errors;
            ASSERT_TRUE(sObjectSchemaMgr.LoadClasses(errors)) << (errors.empty() ? std::string() : errors.front());
            ASSERT_TRUE(sTypeRegistry.LoadFromText(Dump(), "schema.json"));
            _loaded = sTypeRegistry.GetCatalog();
            ObjectSchemaLoadResult const tables = sObjectSchemaMgr.LoadTables();
            ASSERT_TRUE(tables.Loaded) << (tables.Errors.empty() ? std::string() : tables.Errors.front());
        }

        TypedViewRegistry _views;
        TypeCatalogPtr _loaded;
        MySQLConnectionInfo _worldInfo;
        bool _open = false;
    };
}

TEST_F(ObjectSchemaMgrTest, TheUpdatesCreateTheTablesWithTheRowsThatProveThePlayerObject)
{
    QueryResult const classes = WorldDatabase.Query("SELECT `name` FROM `server_class` WHERE `hash` = 1616662572");
    ASSERT_TRUE(classes);
    EXPECT_EQ(classes->Fetch()[0].Get<std::string>(), "BasicMobileBehavior");
    QueryResult const types = WorldDatabase.Query("SELECT COUNT(*) FROM `core_object_type`");
    ASSERT_TRUE(types);
    EXPECT_EQ(types->Fetch()[0].Get<uint64>(), 2u);
    QueryResult const behaviors = WorldDatabase.Query("SELECT COUNT(*), SUM(`class_name` IS NULL) FROM `behavior_client_class`");
    ASSERT_TRUE(behaviors);
    EXPECT_EQ(behaviors->Fetch()[0].Get<uint64>(), 39u) << "one row for every behavior PlayerObject.xml names";
    EXPECT_EQ(behaviors->Fetch()[1].Get<uint64>(), 7u) << "the seven slots an accepted player object leaves empty";
    EXPECT_FALSE(WorldDatabase.DirectExecute("INSERT INTO `core_object_type` (`block`, `type`, `class_name`, `evidence`) VALUES (0, 0, 'class ClientObject', 'test')"))
        << "the plain pair cannot stand for a class";
}

TEST_F(ObjectSchemaMgrTest, TheServerClassJoinsTheCatalogAndTheTablesAreFoundBothWays)
{
    ASSERT_NO_FATAL_FAILURE(UseTestRows());
    ASSERT_NO_FATAL_FAILURE(LoadAll());
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    EXPECT_EQ(catalog, _loaded) << "the server class was in place before the dump loaded, so the dump's own build holds it and nothing rebuilds";
    ClassInfo const* const mobile = catalog->FindClass("TestMobileBehavior");
    ASSERT_NE(mobile, nullptr);
    ClassInfo const* const behavior = catalog->FindClass("class BehaviorInstance");
    ASSERT_NE(behavior, nullptr);
    EXPECT_TRUE(mobile->IsA(*behavior));
    EXPECT_TRUE(sTypeRegistry.IsFromSupplement(mobile->Hash));

    CoreObjectTypeTablePtr const types = sObjectSchemaMgr.GetCoreObjectTypes();
    ASSERT_EQ(types->Count(), 1u);
    ASSERT_NE(types->Find(104, 2), nullptr);
    EXPECT_EQ(types->Find(104, 2)->ClassName, "class WizClientObject");
    EXPECT_NE(types->FindByClass(StringHash::KiStringHash("class WizClientObject")), nullptr);

    std::shared_ptr<BehaviorClientClasses const> const behaviors = sObjectSchemaMgr.GetBehaviorClientClasses();
    EXPECT_EQ(behaviors->Count(), 3u);
    BehaviorClientClass const* const found = behaviors->Find("BasicMobileBehavior");
    ASSERT_NE(found, nullptr);
    ASSERT_TRUE(found->ClassName);
    EXPECT_EQ(*found->ClassName, "TestMobileBehavior");
    EXPECT_EQ(found->ClassHash, mobile->Hash);
    BehaviorClientClass const* const empty = behaviors->Find("PathMovementBehavior");
    ASSERT_NE(empty, nullptr);
    EXPECT_FALSE(empty->ClassName) << "no class means the slot is sent empty";
    EXPECT_EQ(behaviors->Find("NoSuchBehavior"), nullptr);
}

TEST_F(ObjectSchemaMgrTest, ARowNamingAClassNobodyDescribesFailsItsReloadAndKeepsWhatWasServing)
{
    ASSERT_NO_FATAL_FAILURE(UseTestRows());
    ASSERT_NO_FATAL_FAILURE(LoadAll());
    CoreObjectTypeTablePtr const serving = sObjectSchemaMgr.GetCoreObjectTypes();
    std::shared_ptr<BehaviorClientClasses const> const servingBehaviors = sObjectSchemaMgr.GetBehaviorClientClasses();

    Execute("INSERT INTO `core_object_type` (`block`, `type`, `class_name`, `evidence`) VALUES (7, 7, 'class Nowhere', 'test'), (8, 8, 'class TestAnimationBehavior', 'test')");
    ReloadOutcome const types = sReloadMgr.Reload(ObjectSchemaMgr::CoreObjectTypeTarget);
    EXPECT_FALSE(types.Ok);
    ASSERT_EQ(types.Errors.size(), 2u);
    EXPECT_NE(types.Errors[0].find("block 7 type 7 names class Nowhere, which the type dump does not list"), std::string::npos) << types.Errors[0];
    EXPECT_NE(types.Errors[1].find("block 8 type 8 names class TestAnimationBehavior, which is not a class CoreObject"), std::string::npos) << types.Errors[1];
    EXPECT_EQ(sObjectSchemaMgr.GetCoreObjectTypes(), serving);

    Execute("INSERT INTO `behavior_client_class` (`behavior_name`, `class_name`, `evidence`) VALUES ('WizardEquipmentBehavior', 'class WizClientObject', 'test')");
    ReloadOutcome const behaviors = sReloadMgr.Reload(ObjectSchemaMgr::BehaviorTarget);
    EXPECT_FALSE(behaviors.Ok);
    ASSERT_EQ(behaviors.Errors.size(), 1u);
    EXPECT_NE(behaviors.Errors[0].find("behavior WizardEquipmentBehavior names class WizClientObject, which is not a class BehaviorInstance"), std::string::npos) << behaviors.Errors[0];
    EXPECT_EQ(sObjectSchemaMgr.GetBehaviorClientClasses(), servingBehaviors);

    Execute("DELETE FROM `core_object_type` WHERE `block` IN (7, 8)");
    Execute("UPDATE `behavior_client_class` SET `class_name` = 'class TestAnimationBehavior' WHERE `behavior_name` = 'WizardEquipmentBehavior'");
    EXPECT_TRUE(sReloadMgr.Reload(ObjectSchemaMgr::CoreObjectTypeTarget).Ok);
    EXPECT_TRUE(sReloadMgr.Reload(ObjectSchemaMgr::BehaviorTarget).Ok);
    EXPECT_EQ(sObjectSchemaMgr.GetBehaviorClientClasses()->Count(), 4u) << "fixed rows reload without a restart";
}

TEST_F(ObjectSchemaMgrTest, AServerClassThatDoesNotHashFailsItsReloadWithTheCatalogUntouched)
{
    ASSERT_NO_FATAL_FAILURE(UseTestRows());
    ASSERT_NO_FATAL_FAILURE(LoadAll());
    TypeCatalogPtr const serving = sTypeRegistry.GetCatalog();

    Execute(fmt::format("UPDATE `server_class_property` SET `hash` = 12345 WHERE `class_hash` = {}", StringHash::KiStringHash("TestMobileBehavior")));
    ReloadOutcome const refused = sReloadMgr.Reload(ObjectSchemaMgr::ClassTarget);
    EXPECT_FALSE(refused.Ok);
    ASSERT_FALSE(refused.Errors.empty());
    EXPECT_NE(refused.Errors.front().find("declares hash 12345"), std::string::npos) << refused.Errors.front();
    EXPECT_EQ(sTypeRegistry.GetCatalog(), serving);

    Execute(fmt::format("UPDATE `server_class_property` SET `hash` = {} WHERE `class_hash` = {}", StringHash::PropertyHash("unsigned int", "m_behaviorTemplateNameID"),
        StringHash::KiStringHash("TestMobileBehavior")));
    Execute(fmt::format("INSERT INTO `server_class` (`hash`, `name`, `evidence`) VALUES ({}, 'TestFishingBehavior', 'test')", StringHash::KiStringHash("TestFishingBehavior")));
    Execute(fmt::format("INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES ({0}, 0, 'class BehaviorInstance'), ({0}, 1, 'class PropertyClass')",
        StringHash::KiStringHash("TestFishingBehavior")));
    Execute(fmt::format("INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `offset`, `flags`) VALUES ({}, 0, 'm_behaviorTemplateNameID', 'unsigned int', {}, 104, 39)",
        StringHash::KiStringHash("TestFishingBehavior"), StringHash::PropertyHash("unsigned int", "m_behaviorTemplateNameID")));
    ReloadOutcome const added = sReloadMgr.Reload(ObjectSchemaMgr::ClassTarget);
    EXPECT_TRUE(added.Ok) << (added.Errors.empty() ? std::string() : added.Errors.front());
    EXPECT_EQ(sTypeRegistry.GetCatalog()->GetGeneration(), serving->GetGeneration() + 1);
    EXPECT_NE(sTypeRegistry.GetCatalog()->FindClass("TestFishingBehavior"), nullptr) << "an added class decodes without a restart";
}
