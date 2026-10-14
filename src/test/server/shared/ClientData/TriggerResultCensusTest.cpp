/*
 * Project Ambrose by Imjustchico
 * Tests the trigger result census through a type dump the test writes: results and cooldown results are counted once per result and once per zone by their class, bytes that do not decode are counted under the class the zone names, and the property values of the classes asked about are tallied, most used first, while other classes keep none.
 */

#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "TriggerResultCensus.h"
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

    void AddClass(Json& classes, std::string const& name, std::vector<std::pair<std::string, std::string>> const& fields)
    {
        Json properties = Json::object();
        uint32 id = 0;
        for (auto const& [type, field] : fields)
        {
            properties[field] = Json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", 7 }, { "container", "Static" }, { "dynamic", false },
                { "singleton", false }, { "pointer", false }, { "hash", StringHash::PropertyHash(type, field) } };
            ++id;
        }
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", { "class PropertyClass" } }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    class TriggerResultCensusTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            Json classes = Json::object();
            classes[std::to_string(StringHash::KiStringHash("class PropertyClass"))] = Json{ { "name", "class PropertyClass" }, { "bases", Json::array() },
                { "hash", StringHash::KiStringHash("class PropertyClass") }, { "properties", Json::object() } };
            AddClass(classes, "class ResAddDynaMod", { { "std::string", "m_dynaModClientTag" }, { "std::string", "m_dynaModState" }, { "bool", "m_bShowsObject" } });
            AddClass(classes, "class ResClientNotifyText", { { "std::string", "m_text" } });
            ASSERT_TRUE(_types.LoadFromText(Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump(), "results.json")) << _types.GetErrors().front();
        }

        ExtractedTriggerResult Mod(std::string const& tag, bool shows)
        {
            PropertyObjectPtr const object = PropertyObject::Create(_types.GetCatalog(), "class ResAddDynaMod");
            EXPECT_EQ(object->Set("m_dynaModClientTag", tag), PropertySetResult::Ok);
            EXPECT_EQ(object->Set("m_dynaModState", std::string("IdleOpen")), PropertySetResult::Ok);
            EXPECT_EQ(object->Set("m_bShowsObject", shows), PropertySetResult::Ok);
            return Encoded(*object);
        }

        ExtractedTriggerResult Text()
        {
            PropertyObjectPtr const object = PropertyObject::Create(_types.GetCatalog(), "class ResClientNotifyText");
            return Encoded(*object);
        }

        ExtractedTriggerResult Encoded(PropertyObject const& object)
        {
            EncodeResult const encoded = ObjectSerializer::Encode(&object, ZoneExtractor::DataOptions());
            EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
            return ExtractedTriggerResult{ object.GetClass().Hash, object.GetClass().Name, encoded.Bytes };
        }

        TypeRegistry _types;
    };
}

TEST_F(TriggerResultCensusTest, ResultsAreCountedByClassAndTheAskedClassesValuesAreTallied)
{
    ExtractedZone hub;
    hub.Path = "WizardCity/WC_Hub";
    ExtractedTrigger gate;
    gate.Name = "Trigger OpenGateToUW";
    gate.Results = { Mod("WC_GateCommons_ToUnicornWay", true), Text() };
    gate.CooldownResults = { Mod("PetParkGate", false) };
    hub.Triggers.push_back(std::move(gate));
    ExtractedZone streets;
    streets.Path = "WizardCity/WC_Streets";
    ExtractedTrigger lights;
    lights.Name = "Lights";
    lights.Results = { Mod("Light", true), ExtractedTriggerResult{ StringHash::KiStringHash("class ResAddDynaMod"), "class ResAddDynaMod", std::vector<uint8>{ 0xFF } } };
    streets.Triggers.push_back(std::move(lights));
    ZoneExtraction zones;
    zones.Zones = { std::move(hub), std::move(streets) };

    TriggerResultCensus const census = TriggerResultCensus::Take(zones, _types.GetCatalog(), { "ResAddDynaMod" });
    EXPECT_EQ(census.GetTriggerCount(), 2u);
    EXPECT_EQ(census.GetResultCount(), 5u) << "cooldown results are counted with the rest";
    ASSERT_EQ(census.GetUsage().size(), 2u);
    ResultClassUsage const& mods = census.GetUsage()[0];
    EXPECT_EQ(mods.ClassName, "ResAddDynaMod");
    EXPECT_EQ(mods.Results, 4u);
    EXPECT_EQ(mods.Zones, 2u);
    EXPECT_EQ(mods.Undecoded, 1u) << "bytes that do not decode are counted under the class the zone names";
    EXPECT_EQ(census.GetUsage()[1].ClassName, "ResClientNotifyText");

    auto const count = [&census](std::string const& property, std::string const& value)
    {
        auto const found = std::ranges::find_if(census.GetValues(), [&](ResultValueCount const& counted) { return counted.Property == property && counted.Value == value; });
        return found == census.GetValues().end() ? 0u : found->Results;
    };
    EXPECT_EQ(count("m_bShowsObject", "true"), 2u);
    EXPECT_EQ(count("m_bShowsObject", "false"), 1u);
    EXPECT_EQ(count("m_dynaModState", "\"IdleOpen\""), 3u);
    auto const shows = std::ranges::find(census.GetValues(), std::string("m_bShowsObject"), &ResultValueCount::Property);
    ASSERT_NE(shows, census.GetValues().end());
    EXPECT_EQ(shows->Value, "true") << "the most used value comes first";
    EXPECT_TRUE(std::ranges::none_of(census.GetValues(), [](ResultValueCount const& counted) { return counted.ClassName == "ResClientNotifyText"; }))
        << "only the classes asked about keep their values";
}
