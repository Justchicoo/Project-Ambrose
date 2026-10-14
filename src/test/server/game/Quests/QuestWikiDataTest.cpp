/*
 * Project Ambrose by Imjustchico
 * Tests the wiki quest list on lines and classes the test invents: each JSON line is read into a quest with its giver, prequests, gold and experience written as numbers with commas, and rewards, while a line that is no object or names no quest is counted as a problem; names fold case, spacing, markup and curly quotes alike; a quest matched by title is gated on its prequest's completed entry, encoded so it decodes back, and linked to it; a giver naming one NPC template is written as its starter, one naming copies of one stacked NPC is written as the lowest of them, and one naming several NPCs apart is left as a choice for the chaining; a prequest only one school takes neither gates nor links a quest every school takes.
 */

#include "ClientCatalog.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "QuestWikiData.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "ZoneExtractor.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 1 | 2 | 4;

    Json Property(std::string const& type, std::string const& name, uint32 id, std::string container = "Static")
    {
        return Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", container }, { "dynamic", container != "Static" },
            { "singleton", false }, { "pointer", type.ends_with('*') }, { "hash", StringHash::PropertyHash(type, name) } };
    }

    TypeCatalogPtr LoadCatalog()
    {
        Json classes = Json::object();
        auto const add = [&classes](std::string const& name, Json bases, Json properties)
        {
            classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
        };
        add("class PropertyClass", Json::array(), Json::object());
        add("enum RequirementList::Operator", Json::array(), Json::object());
        add("class Requirement", Json::array({ "PropertyClass" }), Json{ { "m_applyNOT", Property("bool", "m_applyNOT", 0) } });
        Json list{ { "m_applyNOT", Property("bool", "m_applyNOT", 0) }, { "m_operator", Property("enum RequirementList::Operator", "m_operator", 1) },
            { "m_requirements", Property("class Requirement*", "m_requirements", 2, "List") } };
        list["m_operator"]["enum_options"] = Json{ { "ROP_AND", 0 }, { "ROP_OR", 1 } };
        add("class RequirementList", Json::array({ "Requirement", "PropertyClass" }), list);
        add("class ReqHasEntry", Json::array({ "Requirement", "PropertyClass" }), Json{ { "m_applyNOT", Property("bool", "m_applyNOT", 0) },
            { "m_isQuestRegistry", Property("bool", "m_isQuestRegistry", 1) }, { "m_questName", Property("std::string", "m_questName", 2) },
            { "m_entryName", Property("std::string", "m_entryName", 3) } });
        TypeRegistry registry;
        EXPECT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", classes } }.dump(), "wiki.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
        return registry.GetCatalog();
    }

    QuestPackageQuest Quest(std::string const& name)
    {
        QuestPackageQuest quest;
        quest.Name = name;
        QuestTemplateRow row;
        row.Name = name;
        row.TitleKey = "Title_" + name;
        quest.Rows.Quests.push_back(row);
        return quest;
    }

    std::optional<ClientCatalog> Npcs(std::filesystem::path const& folder, std::vector<std::tuple<uint32, std::string>> const& npcs)
    {
        std::filesystem::create_directories(folder / "templates");
        Json rows = Json::array();
        for (auto const& [id, display] : npcs)
            rows.push_back(Json{ { "id", id }, { "name", "NPC" + std::to_string(id) }, { "display", display }, { "class", "WizGameObjectTemplate" } });
        std::ofstream(folder / "templates" / "NPCs.json") << rows.dump();
        std::ofstream(folder / "index.json") << Json{ { "revision", "test" }, { "categories", Json::array({ Json{ { "name", "NPCs" }, { "files", Json::array({ "templates/NPCs.json" }) } } }) } }.dump();
        std::string error;
        std::optional<ClientCatalog> catalog = ClientCatalog::Read(folder, error);
        EXPECT_TRUE(catalog) << error;
        return catalog;
    }
}

TEST(QuestWikiDataTest, ReadsEachLineAndCountsTheOnesItCannot)
{
    WikiQuestRead const read = QuestWikiData::Parse(
        "{\"name\":\"Second\",\"giver\":\"Merle Ambrose\",\"prequests\":[\"First\",\"\"],\"rewgold\":\"25\",\"rewxp\":\"1,040\","
        "\"rewards\":[{\"type\":\"Hat\",\"value\":\"Cap\"},{\"type\":\"Wand\",\"per_school\":{\"Fire\":\"Fire Wand\"}},{\"type\":\"Robe\"}]}\r\n"
        "\n"
        "not json\n"
        "{\"giver\":\"Nobody\"}\n");
    EXPECT_EQ(read.Lines, 3u);
    ASSERT_EQ(read.Quests.size(), 1u);
    ASSERT_EQ(read.Problems.size(), 2u);
    WikiQuest const& quest = read.Quests.front();
    EXPECT_EQ(quest.Name, "Second");
    EXPECT_EQ(quest.Giver, "Merle Ambrose");
    EXPECT_EQ(quest.Prequests, std::vector<std::string>{ "First" });
    EXPECT_EQ(quest.Gold, std::optional<uint32>(25));
    EXPECT_EQ(quest.Experience, std::optional<uint32>(1040));
    ASSERT_EQ(quest.Rewards.size(), 2u);
    EXPECT_EQ(quest.Rewards[0].Item, "Cap");
    EXPECT_EQ(quest.Rewards[1].PerSchool.at("Fire"), "Fire Wand");
}

TEST(QuestWikiDataTest, NamesFoldCaseSpacingMarkupAndQuotes)
{
    EXPECT_EQ(QuestWikiData::Normalize("  The   <b>Bat</b>  Cave "), "the bat cave");
    EXPECT_EQ(QuestWikiData::Normalize("Ambrose\xE2\x80\x99s Quest"), QuestWikiData::Normalize("ambrose's quest"));
}

TEST(QuestWikiDataTest, GatesOnPrequestsAndWritesTheGiverItCanName)
{
    TypeCatalogPtr const catalog = LoadCatalog();
    ASSERT_TRUE(catalog);
    std::filesystem::path const folder = std::filesystem::temp_directory_path() / "ambrose-wiki-test";
    std::filesystem::remove_all(folder);
    std::optional<ClientCatalog> const npcs = Npcs(folder, { { 10, "Merle Ambrose" }, { 20, "Gamma" }, { 21, "Gamma" }, { 30, "Malistaire" }, { 31, "Malistaire" } });
    ASSERT_TRUE(npcs);
    QuestNpcStacks const stacks = QuestNpcStacks::Build({ { "WC_Hub", 20, 0.0f, 0.0f, 0.0f }, { "WC_Hub", 21, 1.0f, 0.0f, 0.0f },
        { "WC_Hub", 30, 0.0f, 0.0f, 0.0f }, { "DS_Hub", 31, 0.0f, 0.0f, 0.0f } });

    QuestPackageRead read;
    for (std::string const name : { "First", "Second", "Third", "Fourth", "FireOnly", "Fifth" })
        read.Quests.push_back(Quest(name));
    read.Quests[4].SchoolGated = true;
    std::vector<WikiQuest> wiki(6);
    wiki[0].Name = "First";
    wiki[0].Giver = "Merle Ambrose";
    wiki[1].Name = "Second";
    wiki[1].Prequests = { "First" };
    wiki[1].Giver = "Gamma";
    wiki[2].Name = "Third";
    wiki[2].Giver = "Malistaire";
    wiki[3].Name = "fourth";
    wiki[3].Giver = "Nobody";
    wiki[4].Name = "FireOnly";
    wiki[5].Name = "Fifth";
    wiki[5].Prequests = { "FireOnly" };
    QuestWikiData::TextLookup const title = [](std::string_view key) -> std::optional<std::string>
    {
        return key.starts_with("Title_") ? std::optional<std::string>(std::string(key.substr(6))) : std::nullopt;
    };

    QuestWikiMatch const match = QuestWikiData::Apply(catalog, wiki, read, title, *npcs, {}, &stacks);
    std::filesystem::remove_all(folder);
    EXPECT_EQ(match.Matched, 6u);
    EXPECT_EQ(match.PrequestsResolved, 2u);
    EXPECT_EQ(match.PrequestsOfOneSchool, 1u);
    EXPECT_TRUE(read.Quests[5].Rows.Quests.front().Requirements.empty());
    EXPECT_EQ(match.Gated, 1u);
    EXPECT_EQ(match.Links, (std::vector<QuestLink>{ { "First", "Second" } }));
    EXPECT_EQ(match.GiversResolved, 1u);
    EXPECT_EQ(match.GiversStacked, 1u);
    EXPECT_EQ(match.GiversAmbiguous, 1u);
    EXPECT_EQ(match.GiversUnknown, 1u);
    EXPECT_EQ(read.Quests[0].Rows.Quests.front().StarterTemplateId, 10u);
    EXPECT_EQ(read.Quests[1].Rows.Quests.front().StarterTemplateId, 20u);
    EXPECT_EQ(read.Quests[2].Rows.Quests.front().StarterTemplateId, 0u);
    EXPECT_EQ(match.GiverChoices, (QuestGiverChoices{ { "Third", { 30, 31 } } }));
    EXPECT_TRUE(read.Quests[0].Rows.Quests.front().Requirements.empty());

    DecodeResult const gate = ObjectSerializer::Decode(catalog, read.Quests[1].Rows.Quests.front().Requirements, ZoneExtractor::DataOptions());
    ASSERT_TRUE(gate.Ok());
    PropertyValue const* const children = gate.Object->Get("m_requirements");
    ASSERT_TRUE(children && children->GetList());
    ASSERT_EQ(children->GetList()->size(), 1u);
}
