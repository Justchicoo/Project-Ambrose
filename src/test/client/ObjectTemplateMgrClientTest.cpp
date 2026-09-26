/*
 * Project Ambrose by Imjustchico
 * Reads templates from the user's own r806919 install through the template store, when AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH name it: the manifest lists 137423 templates in 27 archives, 12807 of them in a World-Part.wad, template 1 is the PlayerObject every wizard is made from and 1652259 the Balance hat, read through their typed views, a recipe is a template named by its recipe name, a manifest path the install does not hold is named with its archive and entry, a first access takes under 5 ms once its archive is open, every one of them in an optimized build and on average in a debug build, which runs about ten times slower, and 10000 random templates of every kind, game objects, items, spells, recipes, decks and sounds among them, decode with each game object template carrying the id the manifest lists it under, and under a small budget never hold more than it.
 */

#include "Environment.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogConfig.h"
#include "LogTestDirectory.h"
#include "ObjectTemplateMgr.h"
#include "ObjectViews.h"
#include "PropertyObject.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
    class ObjectTemplateMgrClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            std::optional<std::string> const dump = Ambrose::GetEnv("AMBROSE_TYPE_DUMP_PATH");
            if (!client || client->empty() || !dump || dump->empty())
                return;
            s_install = LogConfig::Utf8Path(*client);
            ASSERT_TRUE(sTypeRegistry.LoadFromFile(LogConfig::Utf8Path(*dump)));
            s_loaded = true;
        }

        static void TearDownTestSuite()
        {
            if (s_loaded)
                sTypeRegistry.Clear();
            s_loaded = false;
        }

        void SetUp() override
        {
            if (!s_loaded)
                GTEST_SKIP() << "AMBROSE_CLIENT_DIR and AMBROSE_TYPE_DUMP_PATH are not both set";
            _store.SetInstall(s_install);
            std::vector<std::string> errors;
            ASSERT_TRUE(_store.LoadManifest(errors)) << errors.front();
        }

        std::vector<uint32> PickIds(std::size_t count, uint32 seed) const
        {
            std::shared_ptr<TemplateManifest const> const manifest = _store.GetManifest();
            std::vector<uint32> ids;
            ids.reserve(manifest->Size());
            for (auto const& [id, location] : manifest->GetLocations())
                ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            std::mt19937 random(seed);
            std::shuffle(ids.begin(), ids.end(), random);
            ids.resize(std::min(count, ids.size()));
            return ids;
        }

        static inline std::filesystem::path s_install;
        static inline bool s_loaded = false;
        ObjectTemplateMgr _store;
    };
}

TEST_F(ObjectTemplateMgrClientTest, TheManifestListsEveryTemplateWithTheWorldArchivesItsPipedPathsName)
{
    std::shared_ptr<TemplateManifest const> const manifest = _store.GetManifest();
    EXPECT_EQ(manifest->Size(), 137423u);
    EXPECT_EQ(manifest->GetArchives().size(), 27u);
    std::size_t piped = 0;
    for (auto const& [id, location] : manifest->GetLocations())
        if (location.Archive != TemplateManifest::RootArchive)
            ++piped;
    EXPECT_EQ(piped, 12807u);
    ASSERT_NE(manifest->Find(4188), nullptr);
    EXPECT_EQ(*manifest->Find(4188), (TemplateLocation{ "Krokotopia-WorldData.wad", "ObjectData/KT/DynaTrigger_KT_Gate1_Fire.xml" }));
}

TEST_F(ObjectTemplateMgrClientTest, TemplateOneIsThePlayerObjectEveryWizardIsMadeFrom)
{
    TemplateLookup const lookup = _store.Lookup(ObjectTemplateMgr::PlayerTemplateId);
    ASSERT_TRUE(lookup.Template) << lookup.Error;
    ObjectTemplate const& player = *lookup.Template;
    EXPECT_EQ(player.TemplateId, 1u);
    EXPECT_EQ(player.Archive, "Root.wad");
    EXPECT_EQ(player.File, "ObjectData/PlayerObject.xml");
    EXPECT_EQ(player.ObjectName, "Player Object");
    ASSERT_EQ(player.Behaviors.size(), 39u);
    EXPECT_EQ(player.Behaviors.front(), "WizardEquipmentBehavior");
    EXPECT_EQ(player.Behaviors.back(), "EmotesRadialMenuBehavior");
    std::optional<GameObjectTemplateView> const view = player.As<GameObjectTemplateView>();
    ASSERT_TRUE(view) << "the player's template reads through the GameObjectTemplate view";
    EXPECT_EQ(view->GetTemplateId(), 1u);
    EXPECT_EQ(view->GetObjectName(), "Player Object");

    std::vector<std::string> errors;
    ASSERT_TRUE(_store.LoadPlayer(errors)) << errors.front();
    EXPECT_EQ(_store.GetPlayer()->Behaviors, player.Behaviors);
}

TEST_F(ObjectTemplateMgrClientTest, Template1652259IsTheBalanceHat)
{
    std::shared_ptr<ObjectTemplate const> const hat = _store.GetTemplate(1652259);
    ASSERT_TRUE(hat) << _store.Lookup(1652259).Error;
    EXPECT_EQ(hat->Archive, "Root.wad");
    EXPECT_EQ(hat->File, "ObjectData/CrownItems/Series58/Hats/Crowns-S58-Hats-L110-BS-008-01.xml");
    EXPECT_EQ(hat->ObjectName, "Crowns-S58-Hats-L110-BS-008-01");
    EXPECT_EQ(hat->Behaviors, (std::vector<std::string>{ "RenderBehavior", "JewelSocketBehavior" }));
    std::optional<WizItemTemplateView> const item = hat->As<WizItemTemplateView>();
    ASSERT_TRUE(item) << "the hat reads through the WizItemTemplate view";
    EXPECT_EQ(item->GetTemplateId(), 1652259u);
    EXPECT_EQ(item->GetSchool(), "Balance");
    EXPECT_EQ(item->GetBaseCost(), 27250.0f);
    EXPECT_EQ(item->GetItemLimit(), -1);
    std::optional<GameObjectTemplateView> const object = hat->As<GameObjectTemplateView>();
    ASSERT_TRUE(object);
    EXPECT_EQ(object->GetDisplayName(), "Items_00028316");
}

TEST_F(ObjectTemplateMgrClientTest, APipedTemplateIsReadFromItsWorldArchive)
{
    std::shared_ptr<ObjectTemplate const> const gate = _store.GetTemplate(4188);
    ASSERT_TRUE(gate) << _store.Lookup(4188).Error;
    EXPECT_EQ(gate->Archive, "Krokotopia-WorldData.wad");
    EXPECT_EQ(gate->File, "ObjectData/KT/DynaTrigger_KT_Gate1_Fire.xml");
    EXPECT_TRUE(gate->As<GameObjectTemplateView>());
}

TEST_F(ObjectTemplateMgrClientTest, ARecipeIsATemplateNamedByItsRecipeName)
{
    std::shared_ptr<ObjectTemplate const> const recipe = _store.GetTemplate(83998489);
    ASSERT_TRUE(recipe) << _store.Lookup(83998489).Error;
    EXPECT_EQ(recipe->Archive, "Recipes-WorldData.wad");
    EXPECT_EQ(recipe->File, "ObjectData/Equipment_Recipes/Recipe-KR-Robe-L100-MS-007-01.xml");
    EXPECT_EQ(recipe->ObjectName, "Recipe-KR-Robe-L100-MS-007-01");
    EXPECT_EQ(recipe->Object->GetClass().Name, "class RecipeTemplate");
    EXPECT_TRUE(recipe->Behaviors.empty());
    EXPECT_TRUE(recipe->As<CoreTemplateView>());
    EXPECT_FALSE(recipe->As<GameObjectTemplateView>());
}

TEST_F(ObjectTemplateMgrClientTest, AManifestPathTheInstallDoesNotHoldIsNamedWithItsArchiveAndEntry)
{
    std::string error;
    std::unique_ptr<KiwadArchive> const root = KiwadArchive::Open(s_install / "Data" / "GameData" / "Root.wad", error);
    ASSERT_TRUE(root) << error;
    KiwadReadResult const manifest = root->Read(ObjectTemplateMgr::ManifestEntry);
    ASSERT_TRUE(manifest.Succeeded()) << manifest.Error;
    LogTestDirectory directory;
    std::filesystem::path const gameData = directory.Path() / "Data" / "GameData";
    std::filesystem::create_directories(gameData);
    KiwadBuilder builder(2);
    builder.Add(std::string(ObjectTemplateMgr::ManifestEntry), manifest.Data, true);
    std::vector<uint8> const archive = builder.Build();
    std::ofstream(gameData / "Root.wad", std::ios::binary).write(reinterpret_cast<char const*>(archive.data()), static_cast<std::streamsize>(archive.size()));

    ObjectTemplateMgr store;
    store.SetInstall(directory.Path());
    std::vector<std::string> errors;
    ASSERT_TRUE(store.LoadManifest(errors)) << errors.front();
    EXPECT_EQ(store.GetManifest()->Size(), 137423u) << "the install's own manifest, in a Root.wad that holds nothing else";

    TemplateLookup const player = store.Lookup(1);
    EXPECT_FALSE(player.Template);
    EXPECT_NE(player.Error.find("template 1 is ObjectData/PlayerObject.xml in Root.wad, which cannot be read"), std::string::npos) << player.Error;
    TemplateLookup const gate = store.Lookup(4188);
    EXPECT_FALSE(gate.Template);
    EXPECT_NE(gate.Error.find("Krokotopia-WorldData.wad cannot be opened"), std::string::npos) << gate.Error;
    EXPECT_FALSE(store.LoadPlayer(errors));
    EXPECT_NE(errors.back().find("the player's template is ObjectData/PlayerObject.xml in Root.wad, which cannot be read"), std::string::npos) << errors.back();
}

TEST_F(ObjectTemplateMgrClientTest, AFirstAccessTakesUnderFiveMillisecondsOnceItsArchiveIsOpen)
{
    std::shared_ptr<TemplateManifest const> const manifest = _store.GetManifest();
    std::set<std::string> opened;
    std::set<uint32> warmed;
    for (auto const& [id, location] : manifest->GetLocations())
        if (opened.insert(location.Archive).second)
        {
            ASSERT_TRUE(_store.GetTemplate(id)) << _store.Lookup(id).Error;
            warmed.insert(id);
        }
    ASSERT_EQ(opened.size(), 27u);

    std::vector<double> took;
    for (uint32 const id : PickIds(1000 + warmed.size(), 806919))
    {
        if (warmed.contains(id) || took.size() == 1000)
            continue;
        auto const started = std::chrono::steady_clock::now();
        TemplateLookup const lookup = _store.Lookup(id);
        took.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
        ASSERT_TRUE(lookup.Template) << lookup.Error;
    }
    std::sort(took.begin(), took.end());
    double total = 0;
    for (double const each : took)
        total += each;
    double const mean = total / static_cast<double>(took.size());
    double const median = took[took.size() / 2];
    double const slowest = took.back();
    RecordProperty("MeanMilliseconds", fmt::format("{:.3f}", mean));
    RecordProperty("MedianMilliseconds", fmt::format("{:.3f}", median));
    RecordProperty("SlowestMilliseconds", fmt::format("{:.3f}", slowest));
    std::cout << fmt::format("first access to {} templates: mean {:.3f} ms, median {:.3f} ms, slowest {:.3f} ms\n", took.size(), mean, median, slowest);
    EXPECT_LT(mean, 5.0);
    EXPECT_LT(median, 5.0);
#ifdef NDEBUG
    EXPECT_LT(slowest, 5.0) << "in an optimized build every first access takes under 5 ms";
#endif
}

TEST_F(ObjectTemplateMgrClientTest, TenThousandRandomTemplatesStayWithinTheMemoryBudget)
{
    std::size_t constexpr Budget = std::size_t{ 16 } << 20;
    _store.SetBudget(Budget);
    std::vector<uint32> const ids = PickIds(10000, 1610);
    ASSERT_EQ(ids.size(), 10000u);
    std::size_t decoded = 0;
    std::size_t largest = 0;
    std::vector<uint32> mismatched;
    std::set<std::string> classes;
    for (uint32 const id : ids)
    {
        TemplateLookup const lookup = _store.Lookup(id);
        ASSERT_TRUE(lookup.Template) << lookup.Error;
        classes.insert(lookup.Template->Object->GetClass().Name);
        if (std::optional<GameObjectTemplateView> const view = lookup.Template->As<GameObjectTemplateView>(); view && view->GetTemplateId() != id)
            mismatched.push_back(id);
        decoded += lookup.Template->Bytes;
        largest = std::max(largest, lookup.Template->Bytes);
        TemplateCacheStats const stats = _store.GetCacheStats();
        ASSERT_LE(stats.Bytes, Budget) << "after template " << id;
    }
    TemplateCacheStats const stats = _store.GetCacheStats();
    std::cout << fmt::format("10000 templates of {} classes, {} MiB decoded in all, the largest {} KiB: {} kept in {} MiB, {} dropped\n", classes.size(), decoded >> 20, largest >> 10,
        stats.Entries, stats.Bytes >> 20, stats.Evictions);
    EXPECT_TRUE(mismatched.empty()) << "every game object template carries the id the manifest lists it under, but " << mismatched.size() << " do not, the first " << mismatched.front();
    for (std::string const name : { "class WizGameObjectTemplate", "class WizItemTemplate", "class SpellTemplate", "class RecipeTemplate", "class DeckTemplate", "class SoundDefTemplate" })
        EXPECT_TRUE(classes.contains(name)) << name << " is among the templates the manifest lists, and one should have been read";
    EXPECT_GT(decoded, Budget) << "the templates decoded hold more than the budget, so the budget is what kept the cache small";
    EXPECT_GT(stats.Evictions, 0u);
    EXPECT_LE(stats.Bytes, Budget);
    EXPECT_EQ(stats.Misses, 10000u);
}
