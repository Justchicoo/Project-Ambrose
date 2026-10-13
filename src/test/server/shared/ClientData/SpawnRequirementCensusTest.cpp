/*
 * Project Ambrose by Imjustchico
 * Tests the spawn requirement census through a type dump the test writes: requirement classes are counted once per placement and once per zone, nested lists included, an object without requirements counts only as a placement, bytes that do not decode are counted rather than guessed at, a list that holds nothing is counted, spawn items join the placements only when asked, and the placements that use only some classes are picked out.
 */

#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "SpawnRequirementCensus.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "ZoneExtractor.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;

    constexpr uint32 Saved = 7;

    void AddClass(Json& classes, std::string const& name, std::vector<std::pair<std::string, std::string>> const& fields)
    {
        Json properties = Json::object();
        uint32 id = 0;
        for (auto const& [type, field] : fields)
        {
            bool const list = field == "m_requirements";
            properties[field] = Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", list ? "List" : "Static" },
                { "dynamic", list }, { "singleton", false }, { "pointer", type.ends_with('*') }, { "hash", StringHash::PropertyHash(type, field) } };
            ++id;
        }
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", { "class Requirement", "class PropertyClass" } },
            { "hash", StringHash::KiStringHash(name) }, { "properties", std::move(properties) } };
    }

    std::string RequirementDump()
    {
        Json classes = Json::object();
        classes[std::to_string(StringHash::KiStringHash("class PropertyClass"))] = Json{ { "name", "class PropertyClass" }, { "bases", Json::array() },
            { "hash", StringHash::KiStringHash("class PropertyClass") }, { "properties", Json::object() } };
        std::pair<std::string, std::string> const applyNot{ "bool", "m_applyNOT" };
        AddClass(classes, "class Requirement", { applyNot });
        classes[std::to_string(StringHash::KiStringHash("class Requirement"))]["bases"] = { "class PropertyClass" };
        AddClass(classes, "class RequirementList", { applyNot, { "class Requirement*", "m_requirements" } });
        AddClass(classes, "class ReqHasQuest", { applyNot, { "std::string", "m_questName" } });
        AddClass(classes, "class ReqMagicLevel", { applyNot });
        AddClass(classes, "class ReqHasBadge", { applyNot, { "std::string", "m_badgeName" } });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    class SpawnRequirementCensusTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(_types.LoadFromText(RequirementDump(), "requirements.json")) << _types.GetErrors().front();
        }

        template <typename... Parts>
        PropertyObjectPtr List(Parts... requirements)
        {
            PropertyObjectPtr list = PropertyObject::Create(_types.GetCatalog(), "class RequirementList");
            PropertyValue::List entries;
            (entries.emplace_back(std::move(requirements)), ...);
            EXPECT_EQ(list->Set("m_requirements", std::move(entries)), PropertySetResult::Ok);
            return list;
        }

        PropertyObjectPtr Requirement(std::string const& type)
        {
            PropertyObjectPtr object = PropertyObject::Create(_types.GetCatalog(), type);
            EXPECT_TRUE(object) << type;
            return object;
        }

        ExtractedObject Placed(uint32 objectId, PropertyObjectPtr const& requirements)
        {
            ExtractedObject object;
            object.ClassName = "class WizZoneObject";
            object.ObjectId = objectId;
            object.TemplateId = 1000 + objectId;
            if (requirements)
            {
                EncodeResult const encoded = ObjectSerializer::Encode(requirements.get(), ZoneExtractor::DataOptions());
                EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
                object.SpawnRequirements = encoded.Bytes;
            }
            return object;
        }

        ZoneExtraction Zones()
        {
            ExtractedZone first;
            first.Path = "WizardCity/WC_Hub";
            first.Objects.push_back(Placed(1, List(Requirement("class ReqHasQuest"))));
            first.Objects.push_back(Placed(2, List(Requirement("class ReqHasQuest"), List(Requirement("class ReqMagicLevel")))));
            first.Objects.push_back(Placed(3, nullptr));
            ExtractedZone second;
            second.Path = "WizardCity/WC_Streets";
            second.Objects.push_back(Placed(4, List(Requirement("class ReqHasBadge"), Requirement("class ReqHasQuest"))));
            ExtractedObject broken = Placed(5, nullptr);
            broken.SpawnRequirements = std::vector<uint8>{ 0xFF, 0x01 };
            second.Objects.push_back(std::move(broken));
            second.Objects.push_back(Placed(7, List()));
            ExtractedZone quiet;
            quiet.Path = "WizardCity/WC_Quiet";
            quiet.Objects.push_back(Placed(6, nullptr));
            ExtractedSpawner guards;
            guards.Name = "GateGuards";
            guards.Items.push_back(ExtractedSpawnItem{ 100, Placed(8, List(Requirement("class ReqHasQuest"))) });
            quiet.Spawners.push_back(std::move(guards));
            ZoneExtraction zones;
            zones.Zones = { std::move(first), std::move(second), std::move(quiet) };
            return zones;
        }

        static RequirementUsage UsageOf(SpawnRequirementCensus const& census, std::string const& type)
        {
            auto const found = std::find_if(census.GetUsage().begin(), census.GetUsage().end(), [&type](RequirementUsage const& usage) { return usage.Type == type; });
            return found == census.GetUsage().end() ? RequirementUsage{} : *found;
        }

        TypeRegistry _types;
    };
}

TEST_F(SpawnRequirementCensusTest, CountsEachClassOncePerPlacementAndZoneWithNestedListsIncluded)
{
    SpawnRequirementCensus const census = SpawnRequirementCensus::Take(Zones(), _types.GetCatalog());
    EXPECT_EQ(census.GetPlacementCount(), 7u) << "spawn items join only when asked";
    EXPECT_EQ(census.GetZoneCount(), 3u);
    EXPECT_EQ(census.GetGatedZoneCount(), 2u) << "a zone whose objects carry no requirements is not gated";
    EXPECT_EQ(census.GetGated().size(), 5u);
    EXPECT_EQ(census.GetEmptyCount(), 1u) << "a list that holds nothing is counted, not dropped";

    RequirementUsage const quest = UsageOf(census, "ReqHasQuest");
    EXPECT_EQ(quest.Placements, 3u);
    EXPECT_EQ(quest.Zones, 2u);
    RequirementUsage const level = UsageOf(census, "ReqMagicLevel");
    EXPECT_EQ(level.Placements, 1u) << "a requirement inside a nested list is counted";
    EXPECT_EQ(level.Zones, 1u);
    EXPECT_EQ(UsageOf(census, "RequirementList").Placements, 1u) << "a nested list is itself a class the requirements use";
    EXPECT_EQ(UsageOf(census, "ReqHasBadge").Zones, 1u);
    ASSERT_FALSE(census.GetUsage().empty());
    EXPECT_EQ(census.GetUsage().front().Type, "ReqHasQuest") << "the most used class comes first";
}

TEST_F(SpawnRequirementCensusTest, BytesThatDoNotDecodeAreCountedRatherThanGuessedAt)
{
    SpawnRequirementCensus const census = SpawnRequirementCensus::Take(Zones(), _types.GetCatalog());
    EXPECT_EQ(census.GetUndecodedCount(), 1u);
    RequirementUsage const undecoded = UsageOf(census, std::string(SpawnRequirementCensus::UnnamedRequirement));
    EXPECT_EQ(undecoded.Placements, 1u);
    EXPECT_EQ(undecoded.Zones, 1u);
}

TEST_F(SpawnRequirementCensusTest, PicksThePlacementsThatUseOnlyTheClassesGiven)
{
    SpawnRequirementCensus const census = SpawnRequirementCensus::Take(Zones(), _types.GetCatalog());
    std::vector<GatedPlacement const*> const quests = census.UsingOnly({ "ReqHasQuest" });
    ASSERT_EQ(quests.size(), 1u);
    EXPECT_EQ(quests[0]->ObjectId, 1u);
    EXPECT_EQ(quests[0]->Zone, "WizardCity/WC_Hub");
    EXPECT_EQ(quests[0]->TemplateId, 1001u);

    std::vector<GatedPlacement const*> const known = census.UsingOnly({ "ReqHasQuest", "ReqMagicLevel" });
    ASSERT_EQ(known.size(), 2u) << "a nested list does not stop a placement from using only the classes given";
    EXPECT_EQ(known[1]->ObjectId, 2u);
    EXPECT_TRUE(census.UsingOnly({ "ReqHasBadge" }).empty());
}

TEST_F(SpawnRequirementCensusTest, SpawnItemsJoinThePlacementsWhenAskedWithTheirSpawnerNamed)
{
    SpawnRequirementCensus const census = SpawnRequirementCensus::Take(Zones(), _types.GetCatalog(), true);
    EXPECT_EQ(census.GetPlacementCount(), 8u);
    EXPECT_EQ(census.GetGatedZoneCount(), 3u);
    RequirementUsage const quest = UsageOf(census, "ReqHasQuest");
    EXPECT_EQ(quest.Placements, 4u);
    EXPECT_EQ(quest.Zones, 3u);
    std::vector<GatedPlacement const*> const quests = census.UsingOnly({ "ReqHasQuest" });
    ASSERT_EQ(quests.size(), 2u);
    EXPECT_EQ(quests[1]->Spawner, "GateGuards");
    EXPECT_EQ(quests[1]->Zone, "WizardCity/WC_Quiet");
    EXPECT_TRUE(quests[0]->Spawner.empty()) << "an entry of the zone's object list names no spawner";
}
