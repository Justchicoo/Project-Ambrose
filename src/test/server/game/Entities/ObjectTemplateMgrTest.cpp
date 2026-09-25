/*
 * Project Ambrose by Imjustchico
 * Tests reading an object template from a Root.wad the test builds through a type dump it writes: TemplateManifest.xml gives the file an id lives in, and that file's BINd GameObjectTemplate gives the id, object name and behaviors in their order, with a null entry kept as an empty slot the way NPC templates leave one; a template the manifest does not list, a file the archive lacks, a file of another class and a behavior with no name are each refused, naming the step that failed.
 */

#include "ObjectTemplateMgr.h"
#include "BindFile.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 1 | 2 | 4;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static")
    {
        bool const pointer = type.ends_with('*');
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", pointer }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    std::string TemplateDump()
    {
        Json classes = Json::object();
        auto const add = [&classes](std::string const& name, Json bases, Json properties)
        {
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
        };
        add("class PropertyClass", Json::array(), Json::object());
        add("enum ObjectType", Json::array(), Json::object());
        Json location = Json::object();
        location["m_filename"] = Property("std::string", "m_filename", 0);
        location["m_id"] = Property("unsigned int", "m_id", 1);
        add("class TemplateLocation", Json::array({ "PropertyClass" }), location);
        Json manifest = Json::object();
        manifest["m_serializedTemplates"] = Property("class TemplateLocation", "m_serializedTemplates", 0, "List");
        add("class TemplateManifest", Json::array({ "PropertyClass" }), manifest);
        Json behavior = Json::object();
        behavior["m_behaviorName"] = Property("std::string", "m_behaviorName", 0);
        add("class BehaviorTemplate", Json::array({ "PropertyClass" }), behavior);
        Json object = Json::object();
        object["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "List");
        object["m_objectName"] = Property("std::string", "m_objectName", 1);
        object["m_templateID"] = Property("unsigned int", "m_templateID", 2);
        object["m_visualID"] = Property("unsigned int", "m_visualID", 3);
        object["m_adjectiveList"] = Property("std::string", "m_adjectiveList", 4, "List");
        object["m_exemptFromAOI"] = Property("bool", "m_exemptFromAOI", 5);
        object["m_displayName"] = Property("std::string", "m_displayName", 6);
        object["m_description"] = Property("std::string", "m_description", 7);
        Json kind = Property("enum ObjectType", "m_nObjectType", 8);
        kind["enum_options"] = Json{ { "OBJECT_TYPE_UNKNOWN", 0 }, { "OBJECT_TYPE_NPC", 2 } };
        object["m_nObjectType"] = kind;
        object["m_sIcon"] = Property("std::string", "m_sIcon", 9);
        add("class GameObjectTemplate", Json::array({ "PropertyClass" }), object);
        return Json{ { "version", 2 }, { "classes", classes } }.dump();
    }

    class ObjectTemplateMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _views.Add(TemplateManifestView::Definition);
            _views.Add(TemplateLocationView::Definition);
            _views.Add(GameObjectTemplateView::Definition);
            _registry = std::make_unique<TypeRegistry>(&_views);
            ASSERT_TRUE(_registry->LoadFromText(TemplateDump(), "templates.json")) << _registry->GetErrors().front();
            _catalog = _registry->GetCatalog();

            KiwadBuilder builder(2);
            builder.Add(std::string(ObjectTemplateMgr::ManifestEntry),
                Manifest({ { 7, "ObjectData/Npc.xml" }, { 8, "ObjectData/Missing.xml" }, { 9, "ObjectData/Other.xml" }, { 10, "ObjectData/Nameless.xml" } }), true);
            builder.Add("ObjectData/Npc.xml", Template(7, "Ravenwood Guard", { "NPCBehavior", std::nullopt, "AnimationBehavior" }), true);
            builder.Add("ObjectData/Other.xml", Manifest({}), false);
            builder.Add("ObjectData/Nameless.xml", Template(10, "Nameless", { "" }), false);
            std::vector<uint8> const archive = builder.Build();
            std::filesystem::path const path = _directory.Path() / "Root.wad";
            std::ofstream(path, std::ios::binary).write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
            std::string error;
            _archive = KiwadArchive::Open(path, error);
            ASSERT_TRUE(_archive) << error;
        }

        std::vector<uint8> Write(PropertyObjectPtr const& object)
        {
            EncodeResult encoded = BindFile::Write(object.get());
            EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
            return std::move(encoded.Bytes);
        }

        std::vector<uint8> Manifest(std::vector<std::pair<uint32, std::string>> const& locations)
        {
            PropertyObjectPtr manifest = PropertyObject::Create(_catalog, "class TemplateManifest");
            EXPECT_TRUE(manifest);
            PropertyValue::List entries;
            for (auto const& [id, file] : locations)
            {
                PropertyObjectPtr location = PropertyObject::Create(_catalog, "class TemplateLocation");
                EXPECT_EQ(location->Set("m_id", id), PropertySetResult::Ok);
                EXPECT_EQ(location->Set("m_filename", file), PropertySetResult::Ok);
                entries.emplace_back(std::move(location));
            }
            EXPECT_EQ(manifest->Set("m_serializedTemplates", std::move(entries)), PropertySetResult::Ok);
            return Write(manifest);
        }

        std::vector<uint8> Template(uint32 id, std::string const& name, std::vector<std::optional<std::string>> const& behaviors)
        {
            PropertyObjectPtr object = PropertyObject::Create(_catalog, "class GameObjectTemplate");
            EXPECT_TRUE(object);
            EXPECT_EQ(object->Set("m_templateID", id), PropertySetResult::Ok);
            EXPECT_EQ(object->Set("m_objectName", name), PropertySetResult::Ok);
            PropertyValue::List entries;
            for (std::optional<std::string> const& behaviorName : behaviors)
            {
                if (!behaviorName)
                {
                    entries.emplace_back(PropertyObjectPtr());
                    continue;
                }
                PropertyObjectPtr behavior = PropertyObject::Create(_catalog, "class BehaviorTemplate");
                EXPECT_EQ(behavior->Set("m_behaviorName", *behaviorName), PropertySetResult::Ok);
                entries.emplace_back(std::move(behavior));
            }
            EXPECT_EQ(object->Set("m_behaviors", std::move(entries)), PropertySetResult::Ok);
            return Write(object);
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        std::unique_ptr<TypeRegistry> _registry;
        TypeCatalogPtr _catalog;
        std::unique_ptr<KiwadArchive> _archive;
    };
}

TEST_F(ObjectTemplateMgrTest, ATemplateIsItsFileNameAndBehaviorsInOrderWithEmptySlotsKept)
{
    std::string error;
    std::optional<ObjectTemplate> const found = ObjectTemplateMgr::Read(*_archive, _catalog, 7, error);
    ASSERT_TRUE(found) << error;
    EXPECT_EQ(found->TemplateId, 7u);
    EXPECT_EQ(found->File, "ObjectData/Npc.xml");
    EXPECT_EQ(found->ObjectName, "Ravenwood Guard");
    EXPECT_EQ(found->Behaviors, (std::vector<std::string>{ "NPCBehavior", "", "AnimationBehavior" })) << "a null entry holds its place as an empty name, since the client reads behaviors by position";
}

TEST_F(ObjectTemplateMgrTest, EachStepThatCannotBeTakenIsRefusedByName)
{
    std::string error;
    EXPECT_FALSE(ObjectTemplateMgr::Read(*_archive, _catalog, 99, error));
    EXPECT_NE(error.find("TemplateManifest.xml lists no template 99"), std::string::npos) << error;

    EXPECT_FALSE(ObjectTemplateMgr::Read(*_archive, _catalog, 8, error));
    EXPECT_NE(error.find("template 8 is in ObjectData/Missing.xml, which cannot be read"), std::string::npos) << error;

    EXPECT_FALSE(ObjectTemplateMgr::Read(*_archive, _catalog, 9, error));
    EXPECT_NE(error.find("ObjectData/Other.xml does not read as a GameObjectTemplate: its root is another class"), std::string::npos) << error;

    EXPECT_FALSE(ObjectTemplateMgr::Read(*_archive, _catalog, 10, error));
    EXPECT_NE(error.find("behavior 0 of ObjectData/Nameless.xml carries no m_behaviorName"), std::string::npos) << error;
}
