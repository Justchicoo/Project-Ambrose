/*
 * Project Ambrose by Imjustchico
 * Tests the template extractor over a Root.wad and a world archive the test writes through a type dump it declares: every template the manifest lists that reads as a CoreTemplate becomes a row in id order with its class, source, names, display key, icon, visual id, object type, loot table, adjectives and behaviors in order, a behavior of a class the reader's dump lacks keeps its place with its class hash and the m_behaviorName its skipped bytes hold, and the template still gives a row, an empty slot gives an empty name, another CoreTemplate gives the name its ObjectName property holds, an entry that is missing or not a template is counted as not read without stopping the rest, ObjectData entries are counted apart, and the SQL script replaces the seven tables whole, the item tables emptied when no template is an item, so running it twice writes the same rows.
 */

#include "BindFile.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "TemplateExtractor.h"
#include "TemplateScript.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;
    using Entries = std::vector<std::pair<std::string, std::vector<uint8>>>;

    constexpr uint32 Saved = 1 | 2 | 4;
    constexpr uint32 ObjectName = uint32{ 1 } << 27;
    constexpr char const* Unknown = "class ShopBehaviorTemplate";

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static", uint32 flags = Saved)
    {
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", type.ends_with('*') }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    std::string TemplateDump(bool withShop)
    {
        Json classes = Json::object();
        auto const add = [&classes](std::string const& name, Json bases, Json properties)
        {
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) },
                { "properties", std::move(properties) } };
        };
        add("class PropertyClass", Json::array(), Json::object());
        add("enum ObjectType", Json::array(), Json::object());
        add("class TemplateLocation", Json::array({ "PropertyClass" }), Json{ { "m_filename", Property("std::string", "m_filename", 0) }, { "m_id", Property("unsigned int", "m_id", 1) } });
        add("class TemplateManifest", Json::array({ "PropertyClass" }), Json{ { "m_serializedTemplates", Property("class TemplateLocation", "m_serializedTemplates", 0, "List") } });
        add("class BehaviorTemplate", Json::array({ "PropertyClass" }), Json{ { "m_behaviorName", Property("std::string", "m_behaviorName", 0) } });
        if (withShop)
            add(Unknown, Json::array({ "BehaviorTemplate", "PropertyClass" }), Json{ { "m_behaviorName", Property("std::string", "m_behaviorName", 0) },
                { "m_shopName", Property("std::string", "m_shopName", 1) } });
        add("class CoreTemplate", Json::array({ "PropertyClass" }), Json{ { "m_behaviors", Property("class BehaviorTemplate*", "m_behaviors", 0, "List") } });
        add("class RecipeTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), Json{ { "m_behaviors", Property("class BehaviorTemplate*", "m_behaviors", 0, "List") },
            { "m_recipeName", Property("std::string", "m_recipeName", 1, "Static", Saved | ObjectName) } });
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
        object["m_lootTable"] = Property("std::string", "m_lootTable", 10);
        add("class GameObjectTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), object);
        return Json{ { "version", 2 }, { "classes", classes } }.dump();
    }

    class TemplateExtractorTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            for (auto const& [registry, views, withShop] : { std::tuple{ &_writer, &_writerViews, true }, std::tuple{ &_reader, &_readerViews, false } })
            {
                for (ViewDefinition const* view : { &TemplateManifestView::Definition, &TemplateLocationView::Definition, &CoreTemplateView::Definition, &GameObjectTemplateView::Definition })
                    views->Add(*view);
                registry->SetViews(views);
                ASSERT_TRUE(registry->LoadFromText(TemplateDump(withShop), "templates.json")) << registry->GetErrors().front();
            }
            std::filesystem::create_directories(GameData());
            Entries root{ { "ObjectData/WC/WC-RAV-NPC06.xml", Npc() }, { "ObjectData/Recipe.xml", Recipe() }, { "ObjectData/NotATemplate.xml", Manifest({}) } };
            root.emplace_back("TemplateManifest.xml", Manifest({ { 38232, "ObjectData/WC/WC-RAV-NPC06.xml" }, { 90, "ObjectData/Recipe.xml" },
                { 91, "ObjectData/NotATemplate.xml" }, { 92, "ObjectData/Missing.xml" }, { 39088, "|WizardCity|WorldData|ObjectData/WC-GTW-Registrar.xml" } }));
            WriteArchive("Root.wad", root);
            WriteArchive("WizardCity-WorldData.wad", { { "ObjectData/WC-GTW-Registrar.xml", Registrar() } });
        }

        std::filesystem::path GameData() const
        {
            return _directory.Path() / "Data" / "GameData";
        }

        void WriteArchive(std::string const& name, Entries const& entries)
        {
            KiwadBuilder builder(2);
            for (auto const& [entry, bytes] : entries)
                builder.Add(entry, bytes, true);
            std::vector<uint8> const archive = builder.Build();
            std::ofstream(GameData() / name, std::ios::binary | std::ios::trunc).write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));
        }

        PropertyObjectPtr Create(std::string const& type)
        {
            PropertyObjectPtr object = PropertyObject::Create(_writer.GetCatalog(), type);
            EXPECT_TRUE(object) << type;
            return object;
        }

        std::vector<uint8> Write(PropertyObjectPtr const& object)
        {
            EncodeResult encoded = BindFile::Write(object.get());
            EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
            return std::move(encoded.Bytes);
        }

        std::vector<uint8> Manifest(std::vector<std::pair<uint32, std::string>> const& locations)
        {
            PropertyObjectPtr manifest = Create("class TemplateManifest");
            PropertyValue::List entries;
            for (auto const& [id, file] : locations)
            {
                PropertyObjectPtr location = Create("class TemplateLocation");
                EXPECT_EQ(location->Set("m_id", id), PropertySetResult::Ok);
                EXPECT_EQ(location->Set("m_filename", file), PropertySetResult::Ok);
                entries.emplace_back(std::move(location));
            }
            EXPECT_EQ(manifest->Set("m_serializedTemplates", std::move(entries)), PropertySetResult::Ok);
            return Write(manifest);
        }

        PropertyObjectPtr Behavior(std::string const& type, std::string const& name)
        {
            PropertyObjectPtr behavior = Create(type);
            EXPECT_EQ(behavior->Set("m_behaviorName", name), PropertySetResult::Ok);
            return behavior;
        }

        std::vector<uint8> Npc()
        {
            PropertyObjectPtr npc = Create("class GameObjectTemplate");
            EXPECT_EQ(npc->Set("m_templateID", uint32{ 38232 }), PropertySetResult::Ok);
            EXPECT_EQ(npc->Set("m_objectName", std::string("WC-RAV-NPC06")), PropertySetResult::Ok);
            EXPECT_EQ(npc->Set("m_displayName", std::string("WC-NPCs_00000125")), PropertySetResult::Ok);
            EXPECT_EQ(npc->Set("m_sIcon", std::string("GUI/NpcPortraits/Art_Portrait_Boy_Fire.dds")), PropertySetResult::Ok);
            EXPECT_EQ(npc->Set("m_visualID", uint32{ 4321 }), PropertySetResult::Ok);
            EXPECT_EQ(npc->Set("m_nObjectType", int64{ 2 }), PropertySetResult::Ok);
            EXPECT_EQ(npc->Set("m_lootTable", std::string("WC-Student")), PropertySetResult::Ok);
            PropertyValue::List adjectives;
            adjectives.emplace_back(std::string("Student"));
            adjectives.emplace_back(std::string("Fire"));
            EXPECT_EQ(npc->Set("m_adjectiveList", std::move(adjectives)), PropertySetResult::Ok);
            PropertyValue::List behaviors;
            behaviors.emplace_back(Behavior("class BehaviorTemplate", "NPCBehavior"));
            behaviors.emplace_back(PropertyObjectPtr());
            behaviors.emplace_back(Behavior(Unknown, "BasicNPCServiceBehavior"));
            behaviors.emplace_back(Behavior("class BehaviorTemplate", "WizardQuestingBehavior"));
            EXPECT_EQ(npc->Set("m_behaviors", std::move(behaviors)), PropertySetResult::Ok);
            return Write(npc);
        }

        std::vector<uint8> Registrar()
        {
            PropertyObjectPtr registrar = Create("class GameObjectTemplate");
            EXPECT_EQ(registrar->Set("m_objectName", std::string("WC-GTW-Registrar")), PropertySetResult::Ok);
            return Write(registrar);
        }

        std::vector<uint8> Recipe()
        {
            PropertyObjectPtr recipe = Create("class RecipeTemplate");
            EXPECT_EQ(recipe->Set("m_recipeName", std::string("Recipe-Robe")), PropertySetResult::Ok);
            return Write(recipe);
        }

        LogTestDirectory _directory;
        TypedViewRegistry _writerViews;
        TypedViewRegistry _readerViews;
        TypeRegistry _writer;
        TypeRegistry _reader;
    };
}

TEST_F(TemplateExtractorTest, EveryTemplateBecomesARowAndAnUnknownBehaviorKeepsItsPlace)
{
    TemplateExtraction const extraction = TemplateExtractor::Extract(GameData(), _reader.GetCatalog());
    ASSERT_TRUE(extraction.Ok()) << extraction.Errors.front();
    EXPECT_EQ(extraction.ManifestEntries, 5u);
    ASSERT_EQ(extraction.Templates.size(), 3u);
    EXPECT_EQ(extraction.Templates[0].TemplateId, 90u) << "rows come in id order";
    EXPECT_EQ(extraction.Templates[0].ObjectName, "Recipe-Robe") << "another CoreTemplate is named by its ObjectName property";
    EXPECT_FALSE(extraction.Templates[0].VisualId);

    ExtractedTemplate const* const npc = extraction.Find(38232);
    ASSERT_NE(npc, nullptr);
    EXPECT_EQ(npc->ClassName, "class GameObjectTemplate");
    EXPECT_EQ(npc->ClassHash, StringHash::KiStringHash("class GameObjectTemplate"));
    EXPECT_EQ(npc->Archive, "Root.wad");
    EXPECT_EQ(npc->Path, "ObjectData/WC/WC-RAV-NPC06.xml");
    EXPECT_EQ(npc->ObjectName, "WC-RAV-NPC06");
    EXPECT_EQ(npc->DisplayKey, "WC-NPCs_00000125");
    EXPECT_EQ(npc->Icon, "GUI/NpcPortraits/Art_Portrait_Boy_Fire.dds");
    EXPECT_EQ(npc->VisualId, std::optional<uint32>(4321));
    EXPECT_EQ(npc->ObjectType, std::optional<int64>(2));
    EXPECT_EQ(npc->LootTable, "WC-Student");
    EXPECT_EQ(npc->Adjectives, (std::vector<std::string>{ "Student", "Fire" }));
    ASSERT_EQ(npc->Behaviors.size(), 4u);
    EXPECT_EQ(npc->Behaviors[0].Name, std::optional<std::string>("NPCBehavior"));
    EXPECT_EQ(npc->Behaviors[1].Name, std::optional<std::string>("")) << "an empty slot";
    EXPECT_EQ(npc->Behaviors[2].ClassHash, StringHash::KiStringHash(Unknown)) << "a behavior the dump lacks keeps its place and hash";
    EXPECT_EQ(npc->Behaviors[2].Name, std::optional<std::string>("BasicNPCServiceBehavior")) << "and the name every behavior template writes under the same hash";
    EXPECT_EQ(npc->Behaviors[3].Name, std::optional<std::string>("WizardQuestingBehavior"));
    EXPECT_EQ(extraction.UnknownBehaviorClasses.at(StringHash::KiStringHash(Unknown)), 1u);
    EXPECT_EQ(extraction.GetNpcTemplateCount(), 1u);

    ExtractedTemplate const* const registrar = extraction.Find(39088);
    ASSERT_NE(registrar, nullptr);
    EXPECT_EQ(registrar->Archive, "WizardCity-WorldData.wad");
    EXPECT_EQ(registrar->ObjectName, "WC-GTW-Registrar");

    EXPECT_EQ(extraction.UnreadCount, 2u) << "the manifest's own class and a missing entry are counted, not fatal";
    EXPECT_EQ(extraction.ObjectDataEntries, 4u);
    EXPECT_EQ(extraction.ObjectDataRead, 2u);
}

TEST_F(TemplateExtractorTest, TheScriptReplacesTheTablesWhole)
{
    TemplateExtraction const extraction = TemplateExtractor::Extract(GameData(), _reader.GetCatalog());
    ASSERT_TRUE(extraction.Ok());
    WorldSqlScript const script = TemplateScript::Build(extraction);
    std::vector<std::string> const& statements = script.GetStatements();
    ASSERT_EQ(statements.size(), 12u);
    EXPECT_EQ(statements[0], "DELETE FROM `object_template`");
    EXPECT_EQ(statements[2], "DELETE FROM `object_template_adjective`");
    EXPECT_EQ(statements[4], "DELETE FROM `object_template_behavior`");
    EXPECT_NE(statements[5].find(fmt::format("(38232, 2, {}, {})", StringHash::KiStringHash(Unknown), WorldSqlScript::Literal(std::string("BasicNPCServiceBehavior")))), std::string::npos)
        << statements[5];
    EXPECT_EQ(statements[6], "DELETE FROM `item_template`") << "no item template, so the item tables are only emptied";
    EXPECT_EQ(statements[7], "DELETE FROM `item_template_requirement_list`");
    EXPECT_EQ(statements[8], "DELETE FROM `item_template_requirement`");
    EXPECT_EQ(statements[9], "DELETE FROM `item_template_effect`");
    EXPECT_EQ(statements[10], "DELETE FROM `item_set_bonus`");
    EXPECT_EQ(statements[11], "DELETE FROM `item_set_bonus_data`");
    EXPECT_EQ(TemplateScript::Build(extraction).ToText(), script.ToText()) << "the same install writes the same rows";
    EXPECT_EQ(TemplateScript::GetTables(), (std::vector<std::string_view>{ "object_template", "object_template_adjective", "object_template_behavior", "item_template",
        "item_template_requirement_list", "item_template_requirement", "item_template_effect", "item_set_bonus", "item_set_bonus_data" }));
}
