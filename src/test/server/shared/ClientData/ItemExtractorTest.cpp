/*
 * Project Ambrose by Imjustchico
 * Tests the template extractor's item rows over an install the test writes through a type dump it declares: a synthetic WizItemTemplate becomes one object_template row and one item_template row with its school, cost, rank, limit, set bonus and colors, a template that is no item gives no item row, a behavior of a class the reader's dump lacks keeps the item, and an item whose equip effect is of a class the reader's dump lacks fails the extraction, naming the class hash, rather than being skipped.
 */

#include "ItemTemplateFixtures.h"
#include "LogTestDirectory.h"
#include "ObjectViews.h"
#include "StringHash.h"
#include "TemplateExtractor.h"
#include "TemplateScript.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace
{
    class ItemExtractorTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            for (ViewDefinition const* view : { &TemplateManifestView::Definition, &TemplateLocationView::Definition, &CoreTemplateView::Definition,
                     &GameObjectTemplateView::Definition, &WizItemTemplateView::Definition })
                _views.Add(*view);
            _reader.SetViews(&_views);
            ASSERT_TRUE(_reader.LoadFromText(ItemTemplateFixtures::ReaderDump(), "items.json")) << _reader.GetErrors().front();
        }

        std::filesystem::path GameData() const
        {
            return _directory.Path() / "Data" / "GameData";
        }

        LogTestDirectory _directory;
        TypedViewRegistry _views;
        TypeRegistry _reader;
        ItemTemplateFixtures _fixtures;
    };
}

TEST_F(ItemExtractorTest, ASyntheticWizItemTemplateMapsToOneItemTemplateRow)
{
    _fixtures.Write(GameData());
    TemplateExtraction const extraction = TemplateExtractor::Extract(GameData(), _reader.GetCatalog());
    ASSERT_TRUE(extraction.Ok()) << extraction.Errors.front();
    EXPECT_EQ(extraction.GetItemCount(), 2u);

    ExtractedTemplate const* const hat = extraction.Find(ItemTemplateFixtures::HatId);
    ASSERT_NE(hat, nullptr);
    ASSERT_TRUE(hat->Item.has_value());
    EXPECT_EQ(hat->ClassName, "class WizItemTemplate");
    EXPECT_EQ(hat->ObjectName, "WC-Hat-Fire-01");
    EXPECT_EQ(hat->DisplayKey, "Items_00028316");
    EXPECT_EQ(hat->Item->School, "Fire");
    EXPECT_FLOAT_EQ(hat->Item->BaseCost, 125.0f);
    EXPECT_EQ(hat->Item->NumPrimaryColors, int64{ 2 });
    ExtractedTemplate const* const npc = extraction.Find(ItemTemplateFixtures::NpcId);
    ASSERT_NE(npc, nullptr);
    EXPECT_FALSE(npc->Item.has_value()) << "a game object that is no item gives no item row";
    ExtractedTemplate const* const robe = extraction.Find(ItemTemplateFixtures::RobeId);
    ASSERT_NE(robe, nullptr);
    ASSERT_TRUE(robe->Item.has_value()) << "a behavior of a class the dump lacks keeps the item";
    EXPECT_EQ(robe->Item->UnknownBehaviors, 1u);

    WorldSqlScript const script = TemplateScript::Build(extraction);
    std::vector<std::string> const& statements = script.GetStatements();
    ASSERT_EQ(statements.size(), 8u);
    EXPECT_EQ(statements[6], "DELETE FROM `item_template`");
    EXPECT_NE(statements[7].find(fmt::format("({}, {}, 125, 1, 0, 0, 2, 1)", ItemTemplateFixtures::HatId, WorldSqlScript::Literal(std::string("Fire")))), std::string::npos)
        << statements[7];
    EXPECT_NE(statements[7].find(fmt::format("({}, {}, 300, 5, 1, 42, 2, 1)", ItemTemplateFixtures::RobeId, WorldSqlScript::Literal(std::string("Ice")))), std::string::npos)
        << statements[7];
    EXPECT_EQ(statements[7].find(fmt::format("({},", ItemTemplateFixtures::NpcId)), std::string::npos) << statements[7];
}

TEST_F(ItemExtractorTest, AnEquipEffectOfAClassTheDumpLacksFailsTheExtractionAndNamesTheClass)
{
    _fixtures.Write(GameData(), 125.0f, ItemTemplateFixtures::Robe::UnknownEffect);
    TemplateExtraction const extraction = TemplateExtractor::Extract(GameData(), _reader.GetCatalog());
    EXPECT_FALSE(extraction.Ok());
    ASSERT_EQ(extraction.Errors.size(), 1u);
    EXPECT_NE(extraction.Errors.front().find(fmt::format("class hash {}", StringHash::KiStringHash(ItemTemplateFixtures::UnknownEffect))), std::string::npos)
        << extraction.Errors.front();
    EXPECT_NE(extraction.Errors.front().find("WC-Robe-Ice-02.xml"), std::string::npos) << extraction.Errors.front();
}
