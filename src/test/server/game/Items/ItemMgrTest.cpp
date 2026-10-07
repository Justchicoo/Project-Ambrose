/*
 * Project Ambrose by Imjustchico
 * Tests the item manager on an install the test builds through a type dump it writes: every template under ObjectData/ whose class is a WizItemTemplate becomes an item record with its names, adjectives, school, cost, rank, limit, set bonus and colors, and no other template does; items are found by id, by object name whatever its case and by either as a command gives it; a behavior of a class the dump lacks is counted, not refused; an edited item applies on reload under a new generation; and a reload that meets an item whose equip effect or requirement is of a class the dump lacks keeps the set serving and names the class hash; an item keeps its requirement list and equip effects.
 */

#include "ItemMgr.h"
#include "ItemTemplateFixtures.h"
#include "LogTestDirectory.h"
#include "ObjectTemplateMgr.h"
#include "ObjectViews.h"
#include "ReloadMgr.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
    bool Holds(std::vector<std::string> const& errors, std::string const& text)
    {
        for (std::string const& error : errors)
            if (error.find(text) != std::string::npos)
                return true;
        return false;
    }

    class ItemMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            for (ViewDefinition const* view : { &TemplateManifestView::Definition, &TemplateLocationView::Definition, &CoreTemplateView::Definition,
                     &GameObjectTemplateView::Definition, &WizItemTemplateView::Definition, &RequirementListView::Definition })
                _views.Add(*view);
            sReloadMgr.Clear();
            sObjectTemplateMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            std::vector<std::string> cleared;
            ASSERT_TRUE(sTypeRegistry.ClearSupplement(cleared));
            ASSERT_TRUE(sTypeRegistry.LoadFromText(ItemTemplateFixtures::ReaderDump(), "items.json")) << sTypeRegistry.GetErrors().front();
            _fixtures.Write(GameData());
            sObjectTemplateMgr.SetInstall(_directory.Path());
            std::vector<std::string> errors;
            ASSERT_TRUE(sObjectTemplateMgr.LoadManifest(errors)) << errors.front();
            _items.SetInstall(_directory.Path());
        }

        void TearDown() override
        {
            sReloadMgr.Clear();
            _items.Clear();
            sObjectTemplateMgr.Clear();
            sTypeRegistry.Clear();
            std::vector<std::string> cleared;
            sTypeRegistry.ClearSupplement(cleared);
            sTypeRegistry.SetViews(&sTypedViewRegistry);
        }

        std::filesystem::path GameData() const
        {
            return _directory.Path() / "Data" / "GameData";
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        ItemTemplateFixtures _fixtures;
        ItemMgr _items;
    };
}

TEST_F(ItemMgrTest, EveryItemTemplateUnderObjectDataBecomesARecordAndNoOtherTemplateDoes)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(_items.Load(errors)) << (errors.empty() ? std::string() : errors.front());
    std::shared_ptr<ItemTemplateStore const> const items = _items.GetItems();
    ASSERT_EQ(items->Size(), 2u);
    EXPECT_EQ(items->Find(ItemTemplateFixtures::NpcId), nullptr) << "a game object that is no item is passed over";
    EXPECT_EQ(items->Find(ItemTemplateFixtures::SpellId), nullptr) << "a template outside ObjectData/ is never read";

    ItemTemplateRecord const* const hat = items->Find(ItemTemplateFixtures::HatId);
    ASSERT_NE(hat, nullptr);
    EXPECT_EQ(hat->ClassName, "class WizItemTemplate");
    EXPECT_EQ(hat->File, "ObjectData/Items/Hats/WC-Hat-Fire-01.xml");
    EXPECT_EQ(hat->ObjectName, "WC-Hat-Fire-01");
    EXPECT_EQ(hat->DisplayKey, "Items_00028316");
    EXPECT_EQ(hat->ObjectType, int64{ 3 });
    EXPECT_EQ(hat->Adjectives, (std::vector<std::string>{ "Hat", "Fire" }));
    EXPECT_EQ(hat->School, "Fire");
    EXPECT_FLOAT_EQ(hat->BaseCost, 125.0f);
    EXPECT_EQ(hat->Rank, 1);
    EXPECT_EQ(hat->ItemLimit, 0);
    EXPECT_EQ(hat->ItemSetBonusTemplateId, 0u);
    EXPECT_EQ(hat->NumPrimaryColors, int64{ 2 });
    EXPECT_EQ(hat->NumSecondaryColors, int64{ 1 });
    EXPECT_EQ(hat->UnknownBehaviors, 0u);

    ItemTemplateRecord const* const robe = items->Find(ItemTemplateFixtures::RobeId);
    ASSERT_NE(robe, nullptr);
    EXPECT_EQ(robe->ItemSetBonusTemplateId, 42u);
    EXPECT_EQ(robe->ItemLimit, 1);
    EXPECT_EQ(robe->UnknownBehaviors, 1u) << "a behavior of a class the dump lacks is counted and the item still loads";
    ASSERT_TRUE(robe->EquipRequirements.has_value());
    ASSERT_EQ(robe->EquipRequirements->Requirements.size(), 1u);
    EXPECT_EQ(robe->EquipRequirements->Requirements.front().ClassName, "class ReqMagicLevel");
    ASSERT_EQ(robe->EquipEffects.size(), 1u);
    EXPECT_EQ(robe->EquipEffects.front().ClassName, "class GameEffectInfo");
    EXPECT_EQ(items->CountUnknownBehaviors(), 1u);
    EXPECT_EQ(items->CountByClass().at("class WizItemTemplate"), 2u);
    EXPECT_GT(items->GetMemoryUsage(), 2 * sizeof(ItemTemplateRecord));
}

TEST_F(ItemMgrTest, AnItemIsFoundByIdByNameWhateverItsCaseAndByEitherAsACommandGivesIt)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(_items.Load(errors));
    std::shared_ptr<ItemTemplateStore const> const items = _items.GetItems();
    ASSERT_NE(items->FindByName("wc-hat-fire-01"), nullptr);
    EXPECT_EQ(items->FindByName("wc-hat-fire-01")->TemplateId, ItemTemplateFixtures::HatId);
    ASSERT_NE(items->FindByIdOrName(fmt::format("{}", ItemTemplateFixtures::RobeId)), nullptr);
    EXPECT_EQ(items->FindByIdOrName(fmt::format("{}", ItemTemplateFixtures::RobeId))->ObjectName, "WC-Robe-Ice-02");
    EXPECT_EQ(items->FindByIdOrName("WC-Robe-Ice-02")->TemplateId, ItemTemplateFixtures::RobeId);
    ASSERT_EQ(items->Search("robe").size(), 1u);
    EXPECT_EQ(items->FindByName("WC-Nothing"), nullptr);
}

TEST_F(ItemMgrTest, AnEditedItemAppliesOnReloadUnderANewGeneration)
{
    sObjectTemplateMgr.RegisterReloadTargets();
    _items.RegisterReloadTargets();
    ReloadOutcome const first = sReloadMgr.Reload(ItemMgr::Target);
    ASSERT_TRUE(first.Ok) << first.Errors.front();
    std::shared_ptr<ItemTemplateStore const> const held = _items.GetItems();
    uint64 const generation = _items.GetGeneration();

    _fixtures.Write(GameData(), 990.0f);
    ASSERT_TRUE(sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget).Ok);
    ReloadOutcome const edited = sReloadMgr.Reload(ItemMgr::Target);
    ASSERT_TRUE(edited.Ok) << edited.Errors.front();
    EXPECT_GT(_items.GetGeneration(), generation);
    EXPECT_FLOAT_EQ(_items.GetItems()->Find(ItemTemplateFixtures::HatId)->BaseCost, 990.0f);
    EXPECT_FLOAT_EQ(held->Find(ItemTemplateFixtures::HatId)->BaseCost, 125.0f) << "a caller keeps the set it was handed";
}

TEST_F(ItemMgrTest, AReloadThatMeetsAnEquipEffectOfAClassTheDumpLacksKeepsTheSetServingAndNamesTheClass)
{
    sObjectTemplateMgr.RegisterReloadTargets();
    _items.RegisterReloadTargets();
    ReloadOutcome const first = sReloadMgr.Reload(ItemMgr::Target);
    ASSERT_TRUE(first.Ok) << first.Errors.front();
    uint64 const generation = _items.GetGeneration();

    _fixtures.Write(GameData(), 990.0f, ItemTemplateFixtures::Robe::UnknownEffect);
    ASSERT_TRUE(sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget).Ok);
    ReloadOutcome const broken = sReloadMgr.Reload(ItemMgr::Target);
    EXPECT_FALSE(broken.Ok);
    EXPECT_TRUE(Holds(broken.Errors, fmt::format("class hash {}", StringHash::KiStringHash(ItemTemplateFixtures::UnknownEffect))));
    EXPECT_TRUE(Holds(broken.Errors, "WC-Robe-Ice-02.xml"));
    EXPECT_EQ(_items.GetGeneration(), generation);
    EXPECT_FLOAT_EQ(_items.GetItems()->Find(ItemTemplateFixtures::HatId)->BaseCost, 125.0f) << "the set serving before the reload keeps serving";
}

TEST_F(ItemMgrTest, NoInstallMeansNoItemsAndAnError)
{
    ItemMgr none;
    std::vector<std::string> errors;
    EXPECT_FALSE(none.Load(errors));
    EXPECT_TRUE(Holds(errors, "no Wizard101 install is in use"));
    EXPECT_EQ(none.GetItems()->Size(), 0u);
}

TEST_F(ItemMgrTest, AReloadThatMeetsAnEquipRequirementOfAClassTheDumpLacksKeepsTheSetServing)
{
    sObjectTemplateMgr.RegisterReloadTargets();
    _items.RegisterReloadTargets();
    ReloadOutcome const first = sReloadMgr.Reload(ItemMgr::Target);
    ASSERT_TRUE(first.Ok) << first.Errors.front();

    _fixtures.Write(GameData(), 990.0f, ItemTemplateFixtures::Robe::UnknownRequirement);
    ASSERT_TRUE(sReloadMgr.Reload(ObjectTemplateMgr::ManifestTarget).Ok);
    ReloadOutcome const broken = sReloadMgr.Reload(ItemMgr::Target);
    EXPECT_FALSE(broken.Ok);
    EXPECT_TRUE(Holds(broken.Errors, fmt::format("class hash {}", StringHash::KiStringHash(ItemTemplateFixtures::UnknownRequirement))));
    EXPECT_FLOAT_EQ(_items.GetItems()->Find(ItemTemplateFixtures::HatId)->BaseCost, 125.0f);
}
