/*
 * Project Ambrose by Imjustchico
 * Tests the quest loot writer on classes and rewards the test invents: a quest's wiki gold is left out when its own results already add gold while its experience and a school's item are written, the item gated on that school's list of the package, a ResLoot result naming the table is added after the quest's other results and decodes back to that name, a quest the package does not import or whose own results give items gets nothing, and the script replaces only the package's tables and lists.
 */

#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "QuestLoot.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "ZoneExtractor.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        auto const add = [&classes](std::string const& name, Json bases, Json properties)
        {
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
        };
        add("class PropertyClass", Json::array(), Json::object());
        add("class Result", Json::array({ "PropertyClass" }), Json::object());
        add("class ResLoot", Json::array({ "Result", "PropertyClass" }), Json{ { "m_lootTable", Json{ { "type", "std::string" }, { "id", 0 }, { "offset", 8 }, { "flags", 7 },
            { "container", "Static" }, { "dynamic", false }, { "singleton", false }, { "pointer", false }, { "hash", StringHash::PropertyHash("std::string", "m_lootTable") } } } });
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "loot.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    QuestPackageQuest Quest(std::string const& name, std::vector<std::string> const& results)
    {
        QuestPackageQuest quest;
        quest.Name = name;
        QuestTemplateRow row;
        row.Name = name;
        quest.Rows.Quests.push_back(row);
        uint32 position = 0;
        for (std::string const& result : results)
            quest.Rows.Results.push_back(QuestResultRow{ name, {}, position++, result, {} });
        return quest;
    }
}

TEST(QuestLootTest, WritesWhatThePackageDoesNotGiveAndRollsItOnCompletion)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    QuestPackageRead read;
    read.Quests.push_back(Quest("Paid", { "class ResAddGold", "class ResActorDialog" }));
    read.Quests.push_back(Quest("Stocked", { "class ResItemLoot" }));
    read.Quests.push_back(Quest("Broken", {}));
    read.Quests.back().Category = "unknown class";
    std::vector<QuestWikiReward> const rewards = { { "Paid", 100, 250 }, { "Broken", 5, 5 }, { "Elsewhere", 5, 5 } };
    std::vector<QuestWikiRewardItem> const items = { { "Paid", 0, "fire", "Hat", 900, 1 }, { "Stocked", 0, "", "Robe", 901, 1 } };

    QuestLootRows const loot = QuestLoot::Apply(catalog, rewards, items, read, "package:Test.wad");
    EXPECT_EQ(loot.Tables, std::vector<std::string>{ "package:Test.wad:quest:Paid" });
    EXPECT_EQ(loot.GoldFromPackage, 1u);
    EXPECT_EQ(loot.GoldWritten, 0u);
    EXPECT_EQ(loot.ExperienceWritten, 1u);
    EXPECT_EQ(loot.ItemsWritten, 1u);
    EXPECT_EQ(loot.ItemsFromPackage, 1u);
    EXPECT_EQ(loot.Schools, std::vector<std::string>{ "Fire" });
    ASSERT_EQ(loot.Entries.size(), 2u);
    EXPECT_EQ(loot.Entries[0].Type, "XP");
    EXPECT_EQ(loot.Entries[0].Count, 250u);
    EXPECT_EQ(loot.Entries[1].Type, "ITEM");
    EXPECT_EQ(loot.Entries[1].Position, 1u);
    EXPECT_EQ(loot.Entries[1].TemplateId, 900u);
    EXPECT_EQ(loot.Entries[1].SchoolList, "package:Test.wad:school:Fire");
    EXPECT_EQ(QuestLoot::LootTypeOf("Treasure Card"), "TREASURE_CARD");
    EXPECT_EQ(QuestLoot::LootTypeOf("Spellement"), "SPELL");

    std::vector<QuestResultRow> const& results = read.Quests[0].Rows.Results;
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results.back().Class, QuestLoot::ResultClass);
    EXPECT_EQ(results.back().Position, 2u);
    DecodeResult const decoded = ObjectSerializer::Decode(catalog, results.back().Data, ZoneExtractor::DataOptions());
    ASSERT_TRUE(decoded.Ok());
    PropertyValue const* const table = decoded.Object->Get("m_lootTable");
    ASSERT_TRUE(table && table->GetIf<std::string>());
    EXPECT_EQ(*table->GetIf<std::string>(), "package:Test.wad:quest:Paid");
    EXPECT_EQ(read.Quests[1].Rows.Results.size(), 1u);

    WorldSqlScript const script = QuestLoot::BuildScript(loot, "package:Test.wad");
    std::vector<std::string> const& statements = script.GetStatements();
    ASSERT_GE(statements.size(), 4u);
    EXPECT_TRUE(statements[0].starts_with("DELETE FROM `loot_table` WHERE `source` = "));
    EXPECT_TRUE(std::any_of(statements.begin(), statements.end(), [](std::string const& statement) { return statement.starts_with("DELETE FROM `requirement_list` WHERE `source` = "); }));
    EXPECT_TRUE(statements.back().starts_with("INSERT INTO `loot_entry`"));
}
