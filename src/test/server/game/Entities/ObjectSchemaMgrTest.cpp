/*
 * Project Ambrose by Imjustchico
 * Tests what the world database's object schema tables promise, against a real world database when AMBROSE_TEST_DB is set and a small type dump the test writes: the updates create the tables with the rows that prove the player object, and every class and property they author, the zone trigger and volume classes among them, hashes as its name says, a server class joins the catalog in one build and extends a class the dump describes, the core object types load with the template classes' pairs and give the player template's header, the behavior classes load and are found, a behavior with no class stands for an empty slot, a row naming a class nothing describes or one of the wrong kind, and a template class nothing describes, fails its reload with the row named and keeps what was serving, a server class whose property does not hash fails its reload with the catalog left as it was, and one added then joins and decodes without a restart, and the classes an install holds are written marked install with their enum options, read back on their own as the file they came from, whatever its evidence says, and unlike one read another way, replace the install classes written before them and leave the authored ones.
 */

#include "BindFile.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ObjectSchemaMgr.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"
#include "ServerClassScript.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <random>
#include <string_view>
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
        AddClass(classes, "enum Shade", Json::array(), Json::object());
        Json object = Json::object();
        object["m_globalID.m_full"] = Property("unsigned __int64", "m_globalID.m_full", 0);
        AddClass(classes, "class CoreObject", Json::array({ "PropertyClass" }), object);
        AddClass(classes, "class ClientObject", Json::array({ "CoreObject", "PropertyClass" }), object);
        AddClass(classes, "class WizClientObject", Json::array({ "ClientObject", "CoreObject", "PropertyClass" }), object);
        Json instance = Json::object();
        instance["m_behaviorTemplateNameID"] = Property("unsigned int", "m_behaviorTemplateNameID", 0, Kept);
        AddClass(classes, "class BehaviorInstance", Json::array({ "PropertyClass" }), instance);
        AddClass(classes, "class TestAnimationBehavior", Json::array({ "BehaviorInstance", "PropertyClass" }), instance);
        AddClass(classes, "class CoreTemplate", Json::array({ "PropertyClass" }), Json::object());
        AddClass(classes, "class WizGameObjectTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), Json::object());
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
            Execute("DELETE FROM `core_template_type`");
            Execute("DELETE FROM `core_object_type`");
            Execute("DELETE FROM `server_class`");
            uint32 const hash = StringHash::KiStringHash("TestMobileBehavior");
            Execute(fmt::format("INSERT INTO `server_class` (`hash`, `name`, `evidence`) VALUES ({}, 'TestMobileBehavior', 'test')", hash));
            Execute(fmt::format("INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES ({0}, 0, 'class BehaviorInstance'), ({0}, 1, 'class PropertyClass')", hash));
            Execute(fmt::format("INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `offset`, `flags`) VALUES ({}, 0, 'm_behaviorTemplateNameID', 'unsigned int', {}, 104, 39)",
                hash, StringHash::PropertyHash("unsigned int", "m_behaviorTemplateNameID")));
            Execute("INSERT INTO `core_object_type` (`core_type`, `class_name`, `evidence`) VALUES (104, 'class WizClientObject', 'test')");
            Execute("INSERT INTO `core_template_type` (`template_class`, `core_type`, `template_type`, `evidence`) VALUES ('class WizGameObjectTemplate', 104, 2, 'test')");
            Execute("INSERT INTO `behavior_client_class` (`behavior_name`, `class_name`, `evidence`) VALUES ('BasicMobileBehavior', 'TestMobileBehavior', 'test'), ('AnimationBehavior', 'class TestAnimationBehavior', 'test'), ('PathMovementBehavior', NULL, 'test')");
        }

        void LoadAll()
        {
            std::vector<std::string> errors;
            ASSERT_TRUE(sObjectSchemaMgr.LoadClasses(errors)) << (errors.empty() ? std::string() : errors.front());
            ASSERT_TRUE(sTypeRegistry.LoadFromText(Dump(), "schema.json")) << (sTypeRegistry.GetErrors().empty() ? std::string() : sTypeRegistry.GetErrors().front());
            _loaded = sTypeRegistry.GetCatalog();
            ObjectSchemaLoadResult const tables = sObjectSchemaMgr.LoadTables();
            ASSERT_TRUE(tables.Loaded) << (tables.Errors.empty() ? std::string() : tables.Errors.front());
        }

        TypeDumpLoader::RawDump InstallClass(std::string const& name)
        {
            TypeDumpLoader::RawClass type;
            type.Key = std::to_string(StringHash::KiStringHash(name));
            type.Name = name;
            type.Hash = StringHash::KiStringHash(name);
            type.Bases = { "class BehaviorInstance", "class PropertyClass" };
            type.Evidence = "seen by the test";
            TypeDumpLoader::RawProperty id;
            id.Name = "m_behaviorTemplateNameID";
            id.Type = "unsigned int";
            id.Container = "Static";
            id.Id = 0;
            id.Offset = 104;
            id.Flags = Kept;
            id.Hash = StringHash::PropertyHash("unsigned int", "m_behaviorTemplateNameID");
            TypeDumpLoader::RawProperty shade;
            shade.Name = "m_shade";
            shade.Type = "enum Shade";
            shade.Container = "Static";
            shade.Id = 1;
            shade.Flags = 1 | (1 << 21);
            shade.Hash = StringHash::PropertyHash("enum Shade", "m_shade");
            shade.Options = { { "Shade_Light", int64{ 0 } }, { "Shade_Dark", int64{ 1 } } };
            type.Properties = { id, shade };
            TypeDumpLoader::RawDump dump;
            dump.Version = TypeDumpLoader::SupportedVersion;
            dump.HasClasses = true;
            dump.Classes.push_back(std::move(type));
            return dump;
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
    EXPECT_EQ(types->Fetch()[0].Get<uint64>(), 5u) << "core types 2, 5 and 9 build ClientObject, 104 WizClientObject and 115 WizClientObjectItem";
    QueryResult const templates = WorldDatabase.Query("SELECT `core_type`, `template_type` FROM `core_template_type` WHERE `template_class` = 'class WizGameObjectTemplate'");
    ASSERT_TRUE(templates);
    EXPECT_EQ(templates->Fetch()[0].Get<uint8>(), 104u);
    EXPECT_EQ(templates->Fetch()[1].Get<uint8>(), 2u) << "the player template's class gives the 68 02 every accepted player object opens with";
    QueryResult const behaviors = WorldDatabase.Query("SELECT COUNT(*), SUM(`class_name` IS NULL) FROM `behavior_client_class`");
    ASSERT_TRUE(behaviors);
    EXPECT_EQ(behaviors->Fetch()[0].Get<uint64>(), 130u) << "one row for every behavior PlayerObject.xml names and every one the templates placed in zones name";
    EXPECT_EQ(behaviors->Fetch()[1].Get<uint64>(), 44u) << "the seven slots an accepted player object leaves empty, ten a zone object's template names that the client builds nothing the dump describes for, "
                                                           "and twenty-seven names the client program holds no string of";
    QueryResult const trainer = WorldDatabase.Query("SELECT `class_name` IS NULL FROM `behavior_client_class` WHERE `behavior_name` = 'WizTrainingBehavior'");
    ASSERT_TRUE(trainer);
    EXPECT_EQ(trainer->Fetch()[0].Get<uint64>(), 1u) << "a trainer's behavior is the server's alone";
    QueryResult const npc = WorldDatabase.Query("SELECT `class_name` FROM `behavior_client_class` WHERE `behavior_name` = 'NPCBehavior'");
    ASSERT_TRUE(npc);
    EXPECT_EQ(npc->Fetch()[0].Get<std::string>(), "class NPCBehavior");
    QueryResult const deleted = WorldDatabase.Query("SELECT `class_name` IS NULL FROM `behavior_client_class` WHERE `behavior_name` = 'DeletedBehavior'");
    ASSERT_TRUE(deleted);
    EXPECT_EQ(deleted->Fetch()[0].Get<uint64>(), 1u) << "no factory is registered for it, so its slot is sent empty";
    EXPECT_FALSE(WorldDatabase.DirectExecute("INSERT INTO `core_object_type` (`core_type`, `class_name`, `evidence`) VALUES (0, 'class ClientObject', 'test')"))
        << "core type 0, the byte that says a plain class hash follows, cannot stand for a class";
}

TEST_F(ObjectSchemaMgrTest, EveryAuthoredClassAndPropertyHashesAsItsNameSays)
{
    TypeDumpLoader::RawDump authored;
    std::vector<std::string> errors;
    ASSERT_TRUE(ServerClassScript::Read(authored, errors, ServerClassScript::AuthoredSource)) << (errors.empty() ? std::string() : errors.front());
    std::vector<std::string> names;
    std::size_t properties = 0;
    for (TypeDumpLoader::RawClass const& type : authored.Classes)
    {
        names.push_back(*type.Name);
        EXPECT_EQ(*type.Hash, StringHash::KiStringHash(*type.Name)) << *type.Name;
        EXPECT_FALSE(type.Evidence.value_or("").empty()) << *type.Name << " says what proves it";
        for (TypeDumpLoader::RawProperty const& property : type.Properties)
        {
            ++properties;
            EXPECT_EQ(*property.Hash, TypeDumpLoader::ExpectedPropertyHash(*property.Type, property.Name)) << *type.Name << "." << property.Name;
        }
    }
    for (std::string_view const name : { "class TriggerList", "class Trigger", "class StateTrigger", "class TriggerObjectInfo", "class TriggerVolumeList", "class TriggerVolume", "class TriggerGroupList",
             "class TriggerGroup", "class ResClientNotifyText", "class ResAddDynaMod", "class ResActorDialog", "class ResDownloadPackage" })
        EXPECT_NE(std::find(names.begin(), names.end(), name), names.end()) << name;
    EXPECT_EQ(StringHash::KiStringHash("class TriggerList"), 0x06DAAC43u) << "the root of every triggers.xml";
    EXPECT_EQ(StringHash::KiStringHash("class Trigger"), 0x068C265Bu);
    EXPECT_EQ(StringHash::KiStringHash("class TriggerVolumeList"), 0x1B6EF770u) << "the root of every volumes.xml";
    EXPECT_EQ(StringHash::KiStringHash("class TriggerVolume"), 0x1B7B55F6u);
    EXPECT_GE(properties, 90u);
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
    ASSERT_NE(types->Find(104), nullptr);
    EXPECT_EQ(types->Find(104)->ClassName, "class WizClientObject");
    EXPECT_TRUE(types->IsCoreClass(StringHash::KiStringHash("class WizClientObject")));
    ClassInfo const* const playerTemplate = catalog->FindClass("class WizGameObjectTemplate");
    ASSERT_NE(playerTemplate, nullptr);
    EXPECT_EQ(types->HeaderFor(*playerTemplate, 1), (CoreObjectHeader{ 104, 2, 1 }));

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

    Execute("INSERT INTO `core_object_type` (`core_type`, `class_name`, `evidence`) VALUES (7, 'class Nowhere', 'test'), (8, 'class TestAnimationBehavior', 'test')");
    ReloadOutcome const types = sReloadMgr.Reload(ObjectSchemaMgr::CoreObjectTypeTarget);
    EXPECT_FALSE(types.Ok);
    ASSERT_EQ(types.Errors.size(), 2u);
    EXPECT_NE(types.Errors[0].find("core type 7 names class Nowhere, which the type dump does not list"), std::string::npos) << types.Errors[0];
    EXPECT_NE(types.Errors[1].find("core type 8 names class TestAnimationBehavior, which is not a class CoreObject"), std::string::npos) << types.Errors[1];
    EXPECT_EQ(sObjectSchemaMgr.GetCoreObjectTypes(), serving);
    Execute("DELETE FROM `core_object_type` WHERE `core_type` IN (7, 8)");

    Execute("INSERT INTO `core_object_type` (`core_type`, `class_name`, `evidence`) VALUES (9, 'class ClientObject', 'test')");
    Execute("INSERT INTO `core_template_type` (`template_class`, `core_type`, `template_type`, `evidence`) VALUES ('class Nowhere', 9, 9, 'test')");
    ReloadOutcome const templates = sReloadMgr.Reload(ObjectSchemaMgr::CoreTemplateTypeTarget);
    EXPECT_FALSE(templates.Ok);
    ASSERT_EQ(templates.Errors.size(), 1u);
    EXPECT_NE(templates.Errors[0].find("template class class Nowhere is not listed in the type dump"), std::string::npos) << templates.Errors[0];
    EXPECT_EQ(sObjectSchemaMgr.GetCoreObjectTypes(), serving);
    Execute("DELETE FROM `core_template_type` WHERE `template_class` = 'class Nowhere'");

    Execute("INSERT INTO `behavior_client_class` (`behavior_name`, `class_name`, `evidence`) VALUES ('WizardEquipmentBehavior', 'class WizClientObject', 'test')");
    ReloadOutcome const behaviors = sReloadMgr.Reload(ObjectSchemaMgr::BehaviorTarget);
    EXPECT_FALSE(behaviors.Ok);
    ASSERT_EQ(behaviors.Errors.size(), 1u);
    EXPECT_NE(behaviors.Errors[0].find("behavior WizardEquipmentBehavior names class WizClientObject, which is not a class BehaviorInstance"), std::string::npos) << behaviors.Errors[0];
    EXPECT_EQ(sObjectSchemaMgr.GetBehaviorClientClasses(), servingBehaviors);

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
    TypeCatalogPtr const reloaded = sTypeRegistry.GetCatalog();
    ASSERT_NE(reloaded->FindClass("TestFishingBehavior"), nullptr) << "an added class joins without a restart";
    PropertyObjectPtr const fishing = PropertyObject::Create(reloaded, "TestFishingBehavior");
    ASSERT_TRUE(fishing);
    ASSERT_EQ(fishing->Set("m_behaviorTemplateNameID", uint32{ 41 }), PropertySetResult::Ok);
    EncodeResult const written = BindFile::Write(fishing.get());
    ASSERT_TRUE(written.Ok()) << written.Detail;
    BindReadResult const read = BindFile::Read(reloaded, written.Bytes);
    ASSERT_TRUE(read.Ok()) << read.Detail;
    EXPECT_EQ(read.Decoded.Object->GetClass().Name, "TestFishingBehavior") << "and decodes as itself";
    EXPECT_EQ(*read.Decoded.Object->Get("m_behaviorTemplateNameID")->GetIf<uint32>(), 41u);
}

TEST_F(ObjectSchemaMgrTest, InstallClassesReplaceTheInstallClassesBeforeThemWithTheirEnumOptionsAndLeaveTheAuthoredOnes)
{
    Execute("DELETE FROM `server_class` WHERE `name` <> 'BasicMobileBehavior'");
    std::string error;
    ASSERT_TRUE(ServerClassScript::Build(InstallClass("TestShadeBehavior")).Apply(_worldInfo, error)) << error;
    std::vector<std::string> loading;
    ASSERT_TRUE(sObjectSchemaMgr.LoadClasses(loading)) << (loading.empty() ? std::string() : loading.front());
    ASSERT_TRUE(sTypeRegistry.LoadFromText(Dump(), "schema.json"));
    ClassInfo const* const shaded = sTypeRegistry.GetCatalog()->FindClass("TestShadeBehavior");
    ASSERT_NE(shaded, nullptr);
    PropertyInfo const* const shade = shaded->FindProperty("m_shade");
    ASSERT_NE(shade, nullptr);
    ASSERT_EQ(shade->Options.size(), 2u) << "an enum property's options travel through server_class_property_option";
    EXPECT_EQ(shade->FindOptionValue("Shade_Dark"), 1);
    QueryResult const marked = WorldDatabase.Query("SELECT `source`, `evidence` FROM `server_class` WHERE `name` = 'TestShadeBehavior'");
    ASSERT_TRUE(marked);
    EXPECT_EQ(marked->Fetch()[0].Get<std::string>(), "install");
    EXPECT_EQ(marked->Fetch()[1].Get<std::string>(), "seen by the test");

    TypeDumpLoader::RawDump held;
    std::vector<std::string> reading;
    ASSERT_TRUE(ServerClassScript::Read(held, reading, ServerClassScript::InstallSource)) << (reading.empty() ? std::string() : reading.front());
    ASSERT_EQ(held.Classes.size(), 1u) << "only the classes the install gave, not the authored ones";
    EXPECT_TRUE(ServerClassScript::Matches(held, InstallClass("TestShadeBehavior"))) << "what was written reads back as the file it came from";
    TypeDumpLoader::RawDump changed = InstallClass("TestShadeBehavior");
    changed.Classes.front().Evidence = "seen another day";
    EXPECT_TRUE(ServerClassScript::Matches(held, changed)) << "the evidence alone is no difference";
    changed.Classes.front().Properties.back().Container = "List";
    EXPECT_FALSE(ServerClassScript::Matches(held, changed)) << "a property read another way is";
    EXPECT_FALSE(ServerClassScript::Matches(held, InstallClass("TestOtherBehavior")));

    ASSERT_TRUE(ServerClassScript::Build(InstallClass("TestOtherBehavior")).Apply(_worldInfo, error)) << error;
    std::vector<std::string> errors;
    ASSERT_TRUE(sObjectSchemaMgr.LoadClasses(errors)) << (errors.empty() ? std::string() : errors.front());
    EXPECT_EQ(sTypeRegistry.GetCatalog()->FindClass("TestShadeBehavior"), nullptr) << "a later extraction replaces what the one before it wrote";
    EXPECT_NE(sTypeRegistry.GetCatalog()->FindClass("TestOtherBehavior"), nullptr);
    EXPECT_NE(sTypeRegistry.GetCatalog()->FindClass("BasicMobileBehavior"), nullptr) << "the authored class stays";
    QueryResult const options = WorldDatabase.Query("SELECT COUNT(*) FROM `server_class_property_option`");
    ASSERT_TRUE(options);
    EXPECT_EQ(options->Fetch()[0].Get<uint64>(), 2u) << "the replaced class's options went with it";
}
