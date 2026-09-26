/*
 * Project Ambrose by Imjustchico
 * Tests the template store on an install the test builds, a Root.wad holding TemplateManifest.xml and the Krokotopia-WorldData.wad and Recipes-WorldData.wad piped paths name, read through a type dump it writes: a template is its file, archive, object name and behaviors in order with a null entry kept as an empty slot and an entry with an empty name left out, as the client builds an object's behaviors, a recipe is a template too with the name its class flags ObjectName, a second lookup is a cache hit handing out the same template, a missing id is null and named without throwing, each step that fails is named and nothing it touched is kept, `.reload templates` swaps in an edited manifest while a template already handed out keeps its contents, a broken manifest keeps the old map and reports every fault, the player's template is read again from the file when it reloads and must be a game object template, the least recently used template is dropped past the budget, and lookups from several threads through reloads each get a template or a reason.
 */

#include "ObjectTemplateMgr.h"
#include "BindFile.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;
    using Entries = std::vector<std::pair<std::string, std::vector<uint8>>>;

    constexpr uint32 Saved = 1 | 2 | 4;

    constexpr uint32 ObjectName = uint32{ 1 } << 27;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static", uint32 flags = Saved)
    {
        bool const pointer = type.ends_with('*');
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", flags }, { "container", container }, { "dynamic", container != "Static" },
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
        Json core = Json::object();
        core["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "List");
        add("class CoreTemplate", Json::array({ "PropertyClass" }), core);
        Json recipe = Json::object();
        recipe["m_behaviors"] = Property("class BehaviorTemplate*", "m_behaviors", 0, "List");
        recipe["m_recipeName"] = Property("std::string", "m_recipeName", 1, "Static", Saved | ObjectName);
        recipe["m_cookTime"] = Property("int", "m_cookTime", 2);
        add("class RecipeTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), recipe);
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
        add("class GameObjectTemplate", Json::array({ "CoreTemplate", "PropertyClass" }), object);
        return Json{ { "version", 2 }, { "classes", classes } }.dump();
    }

    bool Holds(std::vector<std::string> const& errors, std::string const& text)
    {
        for (std::string const& error : errors)
            if (error.find(text) != std::string::npos)
                return true;
        return false;
    }

    class ObjectTemplateMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            _views.Add(TemplateManifestView::Definition);
            _views.Add(TemplateLocationView::Definition);
            _views.Add(CoreTemplateView::Definition);
            _views.Add(GameObjectTemplateView::Definition);
            sReloadMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            std::vector<std::string> cleared;
            ASSERT_TRUE(sTypeRegistry.ClearSupplement(cleared));
            ASSERT_TRUE(sTypeRegistry.LoadFromText(TemplateDump(), "templates.json")) << sTypeRegistry.GetErrors().front();
            _catalog = sTypeRegistry.GetCatalog();

            std::filesystem::create_directories(GameData());
            WriteRoot(DefaultManifest(), DefaultFiles());
            WriteArchive("Krokotopia-WorldData.wad", { { "ObjectData/Mummy.xml", Template(11, "Mummy", { "NPCBehavior" }) } });
            WriteArchive("Recipes-WorldData.wad", { { "ObjectData/Recipe-Robe.xml", Recipe("Recipe-Robe", 90) } });
            _store.SetInstall(_directory.Path());
            std::vector<std::string> errors;
            ASSERT_TRUE(_store.LoadManifest(errors)) << errors.front();
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            _store.Clear();
            sTypeRegistry.Clear();
            std::vector<std::string> cleared;
            sTypeRegistry.ClearSupplement(cleared);
            sTypeRegistry.SetViews(&sTypedViewRegistry);
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

        void WriteRoot(std::vector<uint8> manifest, Entries files)
        {
            files.emplace_back(std::string(ObjectTemplateMgr::ManifestEntry), std::move(manifest));
            WriteArchive("Root.wad", files);
        }

        std::vector<uint8> DefaultManifest()
        {
            return Manifest({ { 1, "ObjectData/Player.xml" }, { 7, "ObjectData/Npc.xml" }, { 8, "ObjectData/Missing.xml" }, { 9, "ObjectData/Other.xml" }, { 10, "ObjectData/Baboon.xml" },
                { 11, "|Krokotopia|WorldData|ObjectData/Mummy.xml" }, { 12, "|Grizzleheim|WorldData|ObjectData/Bear.xml" }, { 14, "|Recipes|WorldData|ObjectData/Recipe-Robe.xml" } });
        }

        Entries DefaultFiles()
        {
            return { { "ObjectData/Player.xml", Template(1, "Player Object", { "WizardEquipmentBehavior", "WizardClientPetBehavior" }) },
                { "ObjectData/Npc.xml", Template(7, "Ravenwood Guard", { "NPCBehavior", std::nullopt, "AnimationBehavior" }) }, { "ObjectData/Other.xml", Manifest({}) },
                { "ObjectData/Baboon.xml", Template(10, "Baboon", { "NPCBehavior", "", std::nullopt, "AnimationBehavior" }) } };
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

        std::vector<uint8> Recipe(std::string const& name, int32 cookTime)
        {
            PropertyObjectPtr recipe = PropertyObject::Create(_catalog, "class RecipeTemplate");
            EXPECT_TRUE(recipe);
            EXPECT_EQ(recipe->Set("m_recipeName", name), PropertySetResult::Ok);
            EXPECT_EQ(recipe->Set("m_cookTime", cookTime), PropertySetResult::Ok);
            return Write(recipe);
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
        TypeCatalogPtr _catalog;
        ObjectTemplateMgr _store;
    };
}

TEST_F(ObjectTemplateMgrTest, ATemplateIsItsFileArchiveNameAndBehaviorsInOrderWithEmptySlotsKept)
{
    std::shared_ptr<ObjectTemplate const> const found = _store.GetTemplate(7);
    ASSERT_TRUE(found) << _store.Lookup(7).Error;
    EXPECT_EQ(found->TemplateId, 7u);
    EXPECT_EQ(found->Archive, "Root.wad");
    EXPECT_EQ(found->File, "ObjectData/Npc.xml");
    EXPECT_EQ(found->ObjectName, "Ravenwood Guard");
    EXPECT_EQ(found->Behaviors, (std::vector<std::string>{ "NPCBehavior", "", "AnimationBehavior" })) << "a null entry holds its place as an empty name, since the client reads behaviors by position";
    std::optional<GameObjectTemplateView> const view = found->As<GameObjectTemplateView>();
    ASSERT_TRUE(view) << "a template reads through the typed view of its class";
    EXPECT_EQ(view->GetObjectName(), "Ravenwood Guard");
    EXPECT_EQ(view->GetTemplateId(), 7u);
    EXPECT_GT(found->Bytes, sizeof(ObjectTemplate));
}

TEST_F(ObjectTemplateMgrTest, ABehaviorWithAnEmptyNameIsLeftOutWhileANullOneKeepsItsSlot)
{
    std::shared_ptr<ObjectTemplate const> const baboon = _store.GetTemplate(10);
    ASSERT_TRUE(baboon) << _store.Lookup(10).Error;
    EXPECT_EQ(baboon->Behaviors, (std::vector<std::string>{ "NPCBehavior", "", "AnimationBehavior" }))
        << "the client adds no behavior for an entry named nothing and an empty one for a null entry, so only the null entry holds a place";
    std::optional<CoreTemplateView> const core = baboon->As<CoreTemplateView>();
    ASSERT_TRUE(core);
    EXPECT_EQ(core->GetBehaviors().size(), 4u) << "the template itself keeps every entry";
}

TEST_F(ObjectTemplateMgrTest, APipedPathIsReadFromItsWorldArchive)
{
    std::shared_ptr<ObjectTemplate const> const found = _store.GetTemplate(11);
    ASSERT_TRUE(found) << _store.Lookup(11).Error;
    EXPECT_EQ(found->Archive, "Krokotopia-WorldData.wad");
    EXPECT_EQ(found->File, "ObjectData/Mummy.xml");
    EXPECT_EQ(found->ObjectName, "Mummy");
    EXPECT_EQ(found->Behaviors, (std::vector<std::string>{ "NPCBehavior" }));
    EXPECT_EQ(_store.GetManifest()->GetArchives(), (std::vector<std::string>{ "Grizzleheim-WorldData.wad", "Krokotopia-WorldData.wad", "Recipes-WorldData.wad", "Root.wad" }));
}

TEST_F(ObjectTemplateMgrTest, ARecipeIsATemplateNamedByThePropertyItsClassFlagsObjectName)
{
    std::shared_ptr<ObjectTemplate const> const recipe = _store.GetTemplate(14);
    ASSERT_TRUE(recipe) << _store.Lookup(14).Error;
    EXPECT_EQ(recipe->TemplateId, 14u);
    EXPECT_EQ(recipe->Archive, "Recipes-WorldData.wad");
    EXPECT_EQ(recipe->ObjectName, "Recipe-Robe");
    EXPECT_TRUE(recipe->Behaviors.empty());
    EXPECT_TRUE(recipe->As<CoreTemplateView>());
    EXPECT_FALSE(recipe->As<GameObjectTemplateView>()) << "a recipe is a template but not a game object";
    PropertyValue const* const cookTime = recipe->Object->Get("m_cookTime");
    ASSERT_NE(cookTime, nullptr);
    EXPECT_EQ(*cookTime->GetIf<int32>(), 90);
}

TEST_F(ObjectTemplateMgrTest, ASecondLookupIsACacheHitHandingOutTheSameTemplate)
{
    TemplateLookup const first = _store.Lookup(7);
    ASSERT_TRUE(first.Template) << first.Error;
    TemplateCacheStats stats = _store.GetCacheStats();
    EXPECT_EQ(stats.Misses, 1u);
    EXPECT_EQ(stats.Hits, 0u);
    EXPECT_EQ(stats.Entries, 1u);
    EXPECT_EQ(stats.Bytes, first.Template->Bytes);

    TemplateLookup const second = _store.Lookup(7);
    EXPECT_EQ(second.Template, first.Template) << "a hit hands out the template already decoded";
    EXPECT_TRUE(second.Error.empty());
    stats = _store.GetCacheStats();
    EXPECT_EQ(stats.Hits, 1u);
    EXPECT_EQ(stats.Misses, 1u);
    EXPECT_EQ(stats.Entries, 1u);
}

TEST_F(ObjectTemplateMgrTest, AMissingIdIsNullAndNamedWithoutThrowing)
{
    std::shared_ptr<ObjectTemplate const> found;
    EXPECT_NO_THROW(found = _store.GetTemplate(99));
    EXPECT_FALSE(found);
    TemplateLookup const lookup = _store.Lookup(99);
    EXPECT_FALSE(lookup.Template);
    EXPECT_EQ(lookup.Error, "TemplateManifest.xml lists no template 99");
    EXPECT_EQ(_store.GetCacheStats().Entries, 0u);
}

TEST_F(ObjectTemplateMgrTest, EachStepThatCannotBeTakenIsNamedAndNothingIsKept)
{
    TemplateLookup lookup = _store.Lookup(8);
    EXPECT_FALSE(lookup.Template);
    EXPECT_NE(lookup.Error.find("template 8 is ObjectData/Missing.xml in Root.wad, which cannot be read"), std::string::npos) << lookup.Error;

    lookup = _store.Lookup(9);
    EXPECT_FALSE(lookup.Template);
    EXPECT_EQ(lookup.Error, "ObjectData/Other.xml in Root.wad is a class TemplateManifest, which is not a CoreTemplate");

    lookup = _store.Lookup(12);
    EXPECT_FALSE(lookup.Template);
    EXPECT_NE(lookup.Error.find("Grizzleheim-WorldData.wad cannot be opened"), std::string::npos) << lookup.Error;

    EXPECT_EQ(_store.GetCacheStats().Entries, 0u);
    EXPECT_EQ(_store.GetCacheStats().Misses, 3u);
}

TEST_F(ObjectTemplateMgrTest, NothingIsFoundBeforeAManifestIsRead)
{
    ObjectTemplateMgr store;
    TemplateLookup const lookup = store.Lookup(7);
    EXPECT_FALSE(lookup.Template);
    EXPECT_EQ(lookup.Error, "the template manifest is not loaded");
    std::vector<std::string> errors;
    EXPECT_FALSE(store.LoadManifest(errors));
    EXPECT_EQ(errors, (std::vector<std::string>{ "no Wizard101 install is in use, so the template manifest cannot be read" }));
    errors.clear();
    EXPECT_FALSE(store.LoadPlayer(errors));
    EXPECT_EQ(errors, (std::vector<std::string>{ "the template manifest is not loaded, so the player's template cannot be read" }));
    EXPECT_FALSE(store.GetPlayer()->IsLoaded());
}

TEST_F(ObjectTemplateMgrTest, ReloadingTemplatesServesAnEditedManifestWhileATemplateHandedOutKeepsItsContents)
{
    _store.RegisterReloadTargets();
    std::shared_ptr<ObjectTemplate const> const spawned = _store.GetTemplate(7);
    ASSERT_TRUE(spawned);
    uint64 const generation = _store.GetCacheStats().Generation;

    Entries files = DefaultFiles();
    files.emplace_back("ObjectData/Captain.xml", Template(7, "Ravenwood Captain", { "NPCBehavior" }));
    files.emplace_back("ObjectData/Gnome.xml", Template(13, "Gnome", {}));
    WriteRoot(Manifest({ { 1, "ObjectData/Player.xml" }, { 7, "ObjectData/Captain.xml" }, { 13, "ObjectData/Gnome.xml" } }), files);
    EXPECT_EQ(_store.GetTemplate(13), nullptr) << "the edited manifest is not read until it is reloaded";

    ReloadOutcome const outcome = sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget);
    ASSERT_TRUE(outcome.Ok) << outcome.Errors.front();
    std::shared_ptr<ObjectTemplate const> const reloaded = _store.GetTemplate(7);
    ASSERT_TRUE(reloaded) << _store.Lookup(7).Error;
    EXPECT_EQ(reloaded->File, "ObjectData/Captain.xml");
    EXPECT_EQ(reloaded->ObjectName, "Ravenwood Captain");
    EXPECT_EQ(spawned->File, "ObjectData/Npc.xml") << "an object made before the reload keeps the template it was made from";
    EXPECT_EQ(spawned->ObjectName, "Ravenwood Guard");
    EXPECT_EQ(spawned->Behaviors, (std::vector<std::string>{ "NPCBehavior", "", "AnimationBehavior" }));
    std::shared_ptr<ObjectTemplate const> const added = _store.GetTemplate(13);
    ASSERT_TRUE(added);
    EXPECT_EQ(added->ObjectName, "Gnome");
    EXPECT_FALSE(_store.GetTemplate(11)) << "an id the edited manifest no longer lists is gone";

    TemplateCacheStats const stats = _store.GetCacheStats();
    EXPECT_GT(stats.Generation, generation);
    EXPECT_EQ(stats.Entries, 2u) << "the template decoded under the old manifest is dropped";
    EXPECT_EQ(_store.GetManifest()->Size(), 3u);
}

TEST_F(ObjectTemplateMgrTest, ABrokenManifestKeepsTheOldMapAndReportsEveryFault)
{
    _store.RegisterReloadTargets();
    std::shared_ptr<ObjectTemplate const> const guard = _store.GetTemplate(7);
    ASSERT_TRUE(guard);
    TemplateCacheStats const before = _store.GetCacheStats();

    WriteRoot(Manifest({ { 0, "ObjectData/Zero.xml" }, { 7, "ObjectData/Npc.xml" }, { 7, "ObjectData/Again.xml" }, { 13, "" }, { 14, "|Krokotopia|ObjectData/Mummy.xml" } }), DefaultFiles());
    ReloadOutcome const outcome = sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget);
    EXPECT_FALSE(outcome.Ok);
    EXPECT_EQ(outcome.Errors.size(), 4u);
    EXPECT_TRUE(Holds(outcome.Errors, "TemplateManifest.xml lists template 0 at ObjectData/Zero.xml, and 0 names no template"));
    EXPECT_TRUE(Holds(outcome.Errors, "TemplateManifest.xml lists template 7 twice"));
    EXPECT_TRUE(Holds(outcome.Errors, "TemplateManifest.xml gives template 13 no path"));
    EXPECT_TRUE(Holds(outcome.Errors, "TemplateManifest.xml gives template 14 the path |Krokotopia|ObjectData/Mummy.xml, which names no archive and entry"));

    TemplateCacheStats const after = _store.GetCacheStats();
    EXPECT_EQ(after.Generation, before.Generation) << "the old map goes on serving";
    EXPECT_EQ(after.Entries, before.Entries);
    EXPECT_EQ(_store.GetTemplate(7), guard) << "a template kept under the old map is still served from the cache";
    EXPECT_EQ(_store.GetManifest()->Size(), 8u);
    std::shared_ptr<ObjectTemplate const> const mummy = _store.GetTemplate(11);
    ASSERT_TRUE(mummy) << _store.Lookup(11).Error;
    EXPECT_EQ(mummy->ObjectName, "Mummy");

    WriteRoot(Manifest({}), {});
    std::filesystem::resize_file(GameData() / "Root.wad", 10);
    ReloadOutcome const unreadable = sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget);
    EXPECT_FALSE(unreadable.Ok);
    EXPECT_TRUE(Holds(unreadable.Errors, "Root.wad cannot be opened")) << unreadable.Errors.front();
    EXPECT_EQ(_store.GetCacheStats().Generation, before.Generation);
}

TEST_F(ObjectTemplateMgrTest, ARootWadWithoutAManifestIsRefusedAndTheOldMapKept)
{
    _store.RegisterReloadTargets();
    WriteArchive("Root.wad", DefaultFiles());
    ReloadOutcome const outcome = sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget);
    EXPECT_FALSE(outcome.Ok);
    EXPECT_TRUE(Holds(outcome.Errors, "TemplateManifest.xml cannot be read")) << outcome.Errors.front();
    EXPECT_EQ(_store.GetManifest()->Size(), 8u);
}

TEST_F(ObjectTemplateMgrTest, ThePlayersTemplateIsHeldApartAndReadFromTheFileAgainWhenItReloads)
{
    _store.RegisterReloadTargets();
    std::vector<std::string> errors;
    ASSERT_TRUE(_store.LoadPlayer(errors)) << errors.front();
    std::shared_ptr<ObjectTemplate const> const player = _store.GetPlayer();
    ASSERT_TRUE(player->IsLoaded());
    EXPECT_EQ(player->TemplateId, ObjectTemplateMgr::PlayerTemplateId);
    EXPECT_EQ(player->ObjectName, "Player Object");
    EXPECT_EQ(player->Behaviors, (std::vector<std::string>{ "WizardEquipmentBehavior", "WizardClientPetBehavior" }));
    EXPECT_EQ(_store.GetTemplate(ObjectTemplateMgr::PlayerTemplateId), player) << "the player's template is kept in the cache as well";

    Entries files = DefaultFiles();
    files.front().second = Template(1, "Player Object", { "WizardEquipmentBehavior", "WizardClientPetBehavior", "WizardClientMountBehavior" });
    WriteRoot(DefaultManifest(), files);
    ReloadOutcome const outcome = sReloadMgr.Reload(ObjectTemplateMgr::PlayerTarget);
    ASSERT_TRUE(outcome.Ok) << outcome.Errors.front();
    EXPECT_EQ(_store.GetPlayer()->Behaviors.size(), 3u) << "the player's template is read from the file as it is now";
    EXPECT_EQ(player->Behaviors.size(), 2u) << "a wizard made before the reload keeps the template it was made from";

    files.front().second = Manifest({});
    WriteRoot(DefaultManifest(), files);
    ReloadOutcome const broken = sReloadMgr.Reload(ObjectTemplateMgr::PlayerTarget);
    EXPECT_FALSE(broken.Ok);
    EXPECT_TRUE(Holds(broken.Errors, "ObjectData/Player.xml in Root.wad is a class TemplateManifest, which is not a CoreTemplate")) << broken.Errors.front();
    EXPECT_EQ(_store.GetPlayer()->Behaviors.size(), 3u) << "a player's template that cannot be read keeps the one before";

    files.front().second = Recipe("Player Object", 1);
    WriteRoot(DefaultManifest(), files);
    ReloadOutcome const recipe = sReloadMgr.Reload(ObjectTemplateMgr::PlayerTarget);
    EXPECT_FALSE(recipe.Ok);
    EXPECT_TRUE(Holds(recipe.Errors, "the player's template is ObjectData/Player.xml in Root.wad, a class RecipeTemplate, which is not a GameObjectTemplate")) << recipe.Errors.front();
    EXPECT_EQ(_store.GetPlayer()->Behaviors.size(), 3u);
}

TEST_F(ObjectTemplateMgrTest, TheLeastRecentlyUsedTemplateIsDroppedOnceTheCacheIsPastItsBudget)
{
    std::shared_ptr<ObjectTemplate const> const guard = _store.GetTemplate(7);
    std::shared_ptr<ObjectTemplate const> const mummy = _store.GetTemplate(11);
    ASSERT_TRUE(guard && mummy);
    ASSERT_EQ(_store.GetTemplate(7), guard);

    _store.SetBudget(guard->Bytes + mummy->Bytes - 1);
    TemplateCacheStats stats = _store.GetCacheStats();
    EXPECT_EQ(stats.Entries, 1u);
    EXPECT_EQ(stats.Evictions, 1u);
    EXPECT_EQ(stats.Bytes, guard->Bytes);
    EXPECT_LE(stats.Bytes, stats.Budget);
    EXPECT_EQ(_store.GetTemplate(7), guard) << "the one used last stays";

    std::shared_ptr<ObjectTemplate const> const again = _store.GetTemplate(11);
    ASSERT_TRUE(again);
    EXPECT_NE(again, mummy) << "the one dropped is decoded again";
    EXPECT_EQ(mummy->ObjectName, "Mummy") << "a dropped template stays whole for whoever holds it";
    stats = _store.GetCacheStats();
    EXPECT_EQ(stats.Entries, 1u) << "keeping it again drops the guard, now the least recently used";
    EXPECT_EQ(stats.Evictions, 2u);
    EXPECT_LE(stats.Bytes, stats.Budget);

    _store.SetBudget(1);
    EXPECT_EQ(_store.GetCacheStats().Entries, 0u);
    std::shared_ptr<ObjectTemplate const> const large = _store.GetTemplate(7);
    ASSERT_TRUE(large) << "a template larger than the whole budget is still handed out";
    EXPECT_EQ(_store.GetCacheStats().Entries, 0u);
    EXPECT_EQ(_store.GetCacheStats().Bytes, 0u);
}

TEST_F(ObjectTemplateMgrTest, LookupsFromSeveralThreadsThroughReloadsEachGetATemplateOrAReason)
{
    std::atomic<bool> stop{ false };
    std::atomic<uint64> served{ 0 };
    std::atomic<uint64> unexplained{ 0 };
    std::vector<std::thread> readers;
    for (uint32 reader = 0; reader < 4; ++reader)
        readers.emplace_back([this, reader, &stop, &served, &unexplained]
        {
            uint32 const ids[] = { 1, 7, 11, 99 };
            for (uint32 turn = reader; !stop.load(); ++turn)
            {
                TemplateLookup const lookup = _store.Lookup(ids[turn % 4]);
                if (lookup.Template)
                    served.fetch_add(1);
                else if (lookup.Error.empty())
                    unexplained.fetch_add(1);
            }
        });
    std::size_t failedReloads = 0;
    for (uint32 round = 0; round < 20; ++round)
    {
        std::vector<std::string> errors;
        if (!_store.LoadManifest(errors))
            ++failedReloads;
        _store.SetBudget(round % 2 == 0 ? 1 : ObjectTemplateMgr::DefaultBudget);
    }
    stop = true;
    for (std::thread& reader : readers)
        reader.join();
    EXPECT_EQ(failedReloads, 0u);
    EXPECT_GT(served.load(), 0u);
    EXPECT_EQ(unexplained.load(), 0u);
    TemplateCacheStats const stats = _store.GetCacheStats();
    EXPECT_LE(stats.Bytes, stats.Budget);
}
