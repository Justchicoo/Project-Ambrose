/*
 * Project Ambrose by Imjustchico
 * Tests the client catalog: each template's category comes from its class and the behaviors that mark it a mob, an NPC, a mount, furniture, a pet, a jewel or equipment, and its group from the folder below ObjectData; a build strips the class prefix, names display keys through the locale table, drops empty behavior names, sorts categories by name and templates by group and id, and keeps each zone's placed template ids once in order; a write lays out the index, the category files and the zones, splits a file that passes the size cap into numbered parts, replaces the revision folder a run before it wrote, and refuses a revision that cannot name a folder.
 */

#include "ClientCatalog.h"
#include "LocaleStore.h"
#include "LogTestDirectory.h"
#include "TemplateExtractor.h"
#include "ZoneExtractor.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    ExtractedTemplate Template(uint32 id, std::string className, std::string path, std::vector<std::string> behaviors, std::string displayKey = {})
    {
        ExtractedTemplate found;
        found.TemplateId = id;
        found.ClassName = std::move(className);
        found.Archive = "Root.wad";
        found.Path = std::move(path);
        found.ObjectName = found.Path.substr(found.Path.rfind('/') + 1);
        found.DisplayKey = std::move(displayKey);
        for (std::string& name : behaviors)
            found.Behaviors.push_back({ 1, std::move(name) });
        return found;
    }

    LocaleTable English()
    {
        LocaleTable::TextMap texts;
        texts.emplace("WizardMobs_00000001", "Lord Vlad");
        texts.emplace("Zones_00000001", "The Commons");
        return LocaleTable("en-US", std::move(texts), {}, 1, 2, 0, {});
    }

    Json ReadJson(std::filesystem::path const& file)
    {
        std::ifstream in(file);
        return Json::parse(in);
    }
}

TEST(ClientCatalog, CategoryComesFromClassAndBehaviors)
{
    EXPECT_EQ(ClientCatalog::CategoryOf("WizGameObjectTemplate", { "NPCBehavior", "MobDeckBehavior" }, "DM"), "Mobs");
    EXPECT_EQ(ClientCatalog::CategoryOf("WizGameObjectTemplate", { "NPCBehavior", "WizardQuestingBehavior" }, "WC"), "NPCs");
    EXPECT_EQ(ClientCatalog::CategoryOf("GameObjectTemplate", { "InteractableBehavior" }, "WC"), "Objects");
    EXPECT_EQ(ClientCatalog::CategoryOf("WizItemTemplate", { "MountItemBehavior" }, "Housing/SP"), "Mounts");
    EXPECT_EQ(ClientCatalog::CategoryOf("WizItemTemplate", { "PetNameBehavior", "FurnitureInfoBehavior", "PetItemBehavior" }, "Housing/HouseGuests"), "Housing");
    EXPECT_EQ(ClientCatalog::CategoryOf("WizItemTemplate", { "PetItemBehavior", "PetNameBehavior" }, "Pets"), "Pets");
    EXPECT_EQ(ClientCatalog::CategoryOf("WizItemTemplate", { "RenderBehavior" }, "Jewels/Jewel"), "Jewels");
    EXPECT_EQ(ClientCatalog::CategoryOf("WizItemTemplate", { "RenderBehavior" }, "JewelsAndMore"), "Items");
    EXPECT_EQ(ClientCatalog::CategoryOf("WizItemTemplate", { "RenderBehavior", "JewelSocketBehavior" }, "KM Equipment/Wands"), "Equipment");
    EXPECT_EQ(ClientCatalog::CategoryOf("WizItemTemplate", { "ElixirBehavior" }, "Elixirs"), "Elixirs");
    EXPECT_EQ(ClientCatalog::CategoryOf("TieredSpellTemplate", {}, "Spells/Tiered Spells"), "Spells");
    EXPECT_EQ(ClientCatalog::CategoryOf("RecipeTemplate", {}, "Equipment_Recipes"), "Recipes");
    EXPECT_EQ(ClientCatalog::CategoryOf("SomethingNewTemplate", {}, ""), ClientCatalog::OtherCategory);
}

TEST(ClientCatalog, GroupIsTheFolderBelowObjectData)
{
    EXPECT_EQ(ClientCatalog::GroupOf("ObjectData/KM Equipment/Wands/Wand-KM-L130-Fire-01.xml"), "KM Equipment/Wands");
    EXPECT_EQ(ClientCatalog::GroupOf("Spells/Tiered Spells/Ra.xml"), "Spells/Tiered Spells");
    EXPECT_EQ(ClientCatalog::GroupOf("ObjectData/PlayerObject.xml"), "");
    EXPECT_EQ(ClientCatalog::GroupOf("TemplateManifest.xml"), "");
}

TEST(ClientCatalog, BuildSortsNamesAndKeepsZonesTemplatesOnce)
{
    TemplateExtraction templates;
    templates.Templates.push_back(Template(30, "class WizGameObjectTemplate", "ObjectData/WC/Mob-B.xml", { "NPCBehavior", "", "MobDeckBehavior" }));
    templates.Templates.push_back(Template(20, "class WizGameObjectTemplate", "ObjectData/DM/Vlad.xml", { "NPCBehavior", "MobDeckBehavior" }, "WizardMobs_00000001"));
    templates.Templates.push_back(Template(10, "class WizGameObjectTemplate", "ObjectData/WC/Mob-A.xml", { "NPCBehavior", "MobDeckBehavior" }, "Missing_1"));
    templates.Templates.push_back(Template(5, "class SpellTemplate", "Spells/Ra.xml", {}));

    ZoneExtraction zones;
    ExtractedZone commons;
    commons.Path = "WizardCity/WC_Hub";
    commons.DisplayNameKey = "Zones_00000001";
    commons.Locations.push_back({ "Start", { 1.0f, 2.0f, 3.0f }, 0.5f });
    for (uint64 const id : { uint64{ 30 }, uint64{ 0 }, uint64{ 10 }, uint64{ 30 } })
    {
        ExtractedObject object;
        object.TemplateId = id;
        commons.Objects.push_back(object);
    }
    zones.Zones.push_back(commons);

    LocaleTable const english = English();
    ClientCatalog const catalog = ClientCatalog::Build(templates, zones, &english, "r1.Test");
    ASSERT_EQ(catalog.GetCategories().size(), 2u);
    EXPECT_EQ(catalog.GetCategories()[0].Name, "Mobs");
    EXPECT_EQ(catalog.GetCategories()[1].Name, "Spells");
    std::vector<CatalogTemplate> const& mobs = catalog.GetCategories()[0].Templates;
    ASSERT_EQ(mobs.size(), 3u);
    EXPECT_EQ(mobs[0].TemplateId, 20u);
    EXPECT_EQ(mobs[0].Group, "DM");
    EXPECT_EQ(mobs[0].ClassName, "WizGameObjectTemplate");
    EXPECT_EQ(mobs[0].DisplayName, "Lord Vlad");
    EXPECT_EQ(mobs[1].TemplateId, 10u);
    EXPECT_EQ(mobs[1].DisplayName, "");
    EXPECT_EQ(mobs[2].TemplateId, 30u);
    EXPECT_EQ(mobs[2].Behaviors, (std::vector<std::string>{ "NPCBehavior", "MobDeckBehavior" }));
    EXPECT_EQ(catalog.GetTemplateCount(), 4u);
    EXPECT_EQ(catalog.GetNamedCount(), 1u);
    EXPECT_EQ(catalog.GetLocale(), "en-US");
    ASSERT_EQ(catalog.GetZones().size(), 1u);
    EXPECT_EQ(catalog.GetZones()[0].DisplayName, "The Commons");
    EXPECT_EQ(catalog.GetZones()[0].Templates, (std::vector<uint32>{ 10, 30 }));
}

TEST(ClientCatalog, WriteLaysOutTheFolderAndReplacesTheOldOne)
{
    LogTestDirectory directory;
    TemplateExtraction templates;
    templates.Templates.push_back(Template(20, "class WizGameObjectTemplate", "ObjectData/DM/Vlad.xml", { "NPCBehavior", "MobDeckBehavior" }, "WizardMobs_00000001"));
    templates.Templates.push_back(Template(5, "class SpellTemplate", "Spells/Ra.xml", {}));
    ZoneExtraction zones;
    ExtractedZone commons;
    commons.Path = "WizardCity/WC_Hub";
    zones.Zones.push_back(commons);
    LocaleTable const english = English();
    ClientCatalog const catalog = ClientCatalog::Build(templates, zones, &english, "r1.Test");

    std::filesystem::path const folder = directory.Path() / "r1.Test";
    std::filesystem::create_directories(folder / "templates");
    std::ofstream(folder / "templates" / "Stale.json") << "[]";
    std::string error;
    ASSERT_TRUE(catalog.Write(directory.Path(), error)) << error;
    EXPECT_FALSE(std::filesystem::exists(folder / "templates" / "Stale.json"));
    EXPECT_FALSE(std::filesystem::exists(directory.Path() / "r1.Test.partial"));

    Json const index = ReadJson(folder / "index.json");
    EXPECT_EQ(index["revision"], "r1.Test");
    EXPECT_EQ(index["templates"], 2);
    EXPECT_EQ(index["named"], 1);
    EXPECT_EQ(index["zoneFiles"], Json::array({ "zones.json" }));
    ASSERT_EQ(index["categories"].size(), 2u);
    EXPECT_EQ(index["categories"][0]["name"], "Mobs");
    EXPECT_EQ(index["categories"][0]["files"], Json::array({ "templates/Mobs.json" }));
    EXPECT_EQ(index["categories"][0]["groups"]["DM"], 1);

    Json const mobs = ReadJson(folder / "templates" / "Mobs.json");
    ASSERT_EQ(mobs.size(), 1u);
    EXPECT_EQ(mobs[0]["id"], 20);
    EXPECT_EQ(mobs[0]["display"], "Lord Vlad");
    EXPECT_EQ(mobs[0]["class"], "WizGameObjectTemplate");
    EXPECT_EQ(mobs[0]["path"], "ObjectData/DM/Vlad.xml");
    EXPECT_EQ(ReadJson(folder / "zones.json")[0]["path"], "WizardCity/WC_Hub");
}

TEST(ClientCatalog, AFilePastTheCapIsSplitIntoParts)
{
    LogTestDirectory directory;
    TemplateExtraction templates;
    std::string const longName(400, 'x');
    for (uint32 id = 1; id <= 3000; ++id)
        templates.Templates.push_back(Template(id, "class SpellTemplate", "Spells/" + longName + std::to_string(id) + ".xml", {}));
    ClientCatalog const catalog = ClientCatalog::Build(templates, {}, nullptr, "r2.Test");
    std::string error;
    ASSERT_TRUE(catalog.Write(directory.Path(), error)) << error;

    Json const index = ReadJson(directory.Path() / "r2.Test" / "index.json");
    Json const& files = index["categories"][0]["files"];
    ASSERT_GE(files.size(), 2u);
    EXPECT_EQ(files[0], "templates/Spells-01.json");
    std::size_t rows = 0;
    for (Json const& file : files)
    {
        std::filesystem::path const part = directory.Path() / "r2.Test" / file.get<std::string>();
        EXPECT_LE(std::filesystem::file_size(part), ClientCatalog::MaxFileBytes);
        rows += ReadJson(part).size();
    }
    EXPECT_EQ(rows, 3000u);
}

TEST(ClientCatalog, ARevisionThatCannotNameAFolderIsRefused)
{
    LogTestDirectory directory;
    ClientCatalog const catalog = ClientCatalog::Build({}, {}, nullptr, "../up");
    std::string error;
    EXPECT_FALSE(catalog.Write(directory.Path(), error));
    EXPECT_FALSE(error.empty());
}
