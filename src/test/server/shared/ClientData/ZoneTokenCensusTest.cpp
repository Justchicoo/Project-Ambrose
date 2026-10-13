/*
 * Project Ambrose by Imjustchico
 * Tests the zone token census through a type dump the test writes: tokens are counted by class once per token and once per zone with the names they give themselves, a hook is counted only when its ResultList holds results, a time trigger's hook is named through its list, a result the dump lacks is counted by its hash from the skipped parts, the triggers' result, cooldown result and requirement classes are counted, and a requirement or result string that names a token is counted by the class and property holding it.
 */

#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "ZoneExtractor.h"
#include "ZoneTokenCensus.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Json = nlohmann::json;
    using Fields = std::vector<std::pair<std::string, std::string>>;

    constexpr uint32 Saved = 7;
    constexpr uint32 UnknownResultHash = 123456789;

    void AddClass(Json& classes, std::string const& name, std::string const& base, Fields const& fields)
    {
        Json properties = Json::object();
        uint32 id = 0;
        for (auto const& [type, field] : fields)
        {
            bool const list = type.ends_with("[]");
            std::string const held = list ? type.substr(0, type.size() - 2) : type;
            properties[field] = Json{ { "type", held }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", Saved }, { "container", list ? "List" : "Static" },
                { "dynamic", list }, { "singleton", false }, { "pointer", held.ends_with('*') }, { "hash", StringHash::PropertyHash(held, field) } };
            ++id;
        }
        Json bases = Json::array();
        if (!base.empty())
            bases = { base, "class PropertyClass" };
        classes[std::to_string(StringHash::KiStringHash(name))] = Json{ { "name", name }, { "bases", std::move(bases) }, { "hash", StringHash::KiStringHash(name) },
            { "properties", std::move(properties) } };
    }

    std::string TokenDump()
    {
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", "", {});
        AddClass(classes, "class Result", "class PropertyClass", {});
        AddClass(classes, "class ResultList", "class PropertyClass", { { "class Result*[]", "m_results" } });
        AddClass(classes, "class ResSetZoneFlag", "class Result", { { "std::string", "m_flagName" } });
        AddClass(classes, "class ResPlaySound", "class Result", { { "std::string", "m_sound" } });
        AddClass(classes, "class Requirement", "class PropertyClass", { { "bool", "m_applyNOT" } });
        AddClass(classes, "class RequirementList", "class Requirement", { { "bool", "m_applyNOT" }, { "class Requirement*[]", "m_requirements" } });
        AddClass(classes, "class ReqZoneFlag", "class Requirement", { { "bool", "m_applyNOT" }, { "std::string", "m_flagName" } });
        Fields const base{ { "std::string", "m_debugName" }, { "class ResultList*", "m_onEnable" } };
        AddClass(classes, "class ZoneTokenBase", "class PropertyClass", base);
        Fields boolean = base;
        boolean.push_back({ "class ResultList*", "m_onSet" });
        boolean.push_back({ "class ResultList*", "m_onClear" });
        AddClass(classes, "class ZoneTokenBoolean", "class ZoneTokenBase", boolean);
        AddClass(classes, "class ZoneTokenTimeTrigger", "class PropertyClass", { { "std::string", "m_triggerAt" }, { "class ResultList*", "m_onTimeLeft" } });
        Fields timer = base;
        timer.push_back({ "class ResultList*", "m_onExpire" });
        timer.push_back({ "class ZoneTokenTimeTrigger*[]", "m_timeTriggers" });
        AddClass(classes, "class ZoneTokenTimer", "class ZoneTokenBase", timer);
        AddClass(classes, "class ZoneTokenContainer", "class PropertyClass", { { "class ZoneTokenBase*[]", "m_tokens" } });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    class ZoneTokenCensusTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(_types.LoadFromText(TokenDump(), "tokens.json")) << _types.GetErrors().front();
        }

        PropertyObjectPtr Make(std::string const& type)
        {
            PropertyObjectPtr object = PropertyObject::Create(_types.GetCatalog(), type);
            EXPECT_TRUE(object) << type;
            return object;
        }

        PropertyObjectPtr Named(std::string const& type, std::string const& field, std::string const& text)
        {
            PropertyObjectPtr object = Make(type);
            EXPECT_EQ(object->Set(field, std::string(text)), PropertySetResult::Ok);
            return object;
        }

        template <typename... Parts>
        PropertyObjectPtr Holding(std::string const& type, std::string const& field, Parts... entries)
        {
            PropertyObjectPtr object = Make(type);
            PropertyValue::List list;
            (list.emplace_back(std::move(entries)), ...);
            EXPECT_EQ(object->Set(field, std::move(list)), PropertySetResult::Ok);
            return object;
        }

        template <typename... Parts>
        PropertyObjectPtr Results(Parts... results)
        {
            return Holding("class ResultList", "m_results", std::move(results)...);
        }

        std::vector<uint8> Bytes(PropertyObjectPtr const& object)
        {
            EncodeResult const encoded = ObjectSerializer::Encode(object.get(), ZoneExtractor::DataOptions());
            EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
            return encoded.Bytes;
        }

        template <typename... Parts>
        ExtractedObject Placed(uint32 objectId, Parts... tokens)
        {
            ExtractedObject object;
            object.ClassName = "class ZoneTokenInfo";
            object.ObjectId = objectId;
            object.TokenContainer = Bytes(Holding("class ZoneTokenContainer", "m_tokens", std::move(tokens)...));
            return object;
        }

        ExtractedTriggerResult TriggerResult(PropertyObjectPtr const& result)
        {
            return { result->GetClass().Hash, result->GetClass().Name, Bytes(result) };
        }

        ZoneExtraction Zones()
        {
            PropertyObjectPtr gate = Named("class ZoneTokenBoolean", "m_debugName", "GateOpen");
            EXPECT_EQ(gate->Set("m_onSet", Results(Named("class ResPlaySound", "m_sound", "Gate.wav"), Named("class ResSetZoneFlag", "m_flagName", "Other"))),
                PropertySetResult::Ok);
            EXPECT_EQ(gate->Set("m_onClear", Results()), PropertySetResult::Ok);
            PropertyObjectPtr timer = Named("class ZoneTokenTimer", "m_debugName", "Countdown");
            EXPECT_EQ(timer->Set("m_onExpire", Results(Named("class ResSetZoneFlag", "m_flagName", "GateOpen"))), PropertySetResult::Ok);
            PropertyObjectPtr warning = Named("class ZoneTokenTimeTrigger", "m_triggerAt", "0:30");
            EXPECT_EQ(warning->Set("m_onTimeLeft", Results(Named("class ResPlaySound", "m_sound", "Tick.wav"))), PropertySetResult::Ok);
            PropertyValue::List warnings;
            warnings.emplace_back(std::move(warning));
            EXPECT_EQ(timer->Set("m_timeTriggers", std::move(warnings)), PropertySetResult::Ok);

            ExtractedZone first;
            first.Path = "WizardCity/WC_Tower";
            first.Objects.push_back(Placed(1, std::move(gate), std::move(timer)));
            first.Objects.push_back(ExtractedObject{});
            ExtractedTrigger opener;
            opener.Name = "OpenGate";
            opener.Requirements = Bytes(Holding("class RequirementList", "m_requirements", Named("class ReqZoneFlag", "m_flagName", "Countdown")));
            opener.Results.push_back(TriggerResult(Named("class ResSetZoneFlag", "m_flagName", "GateOpen")));
            opener.CooldownResults.push_back(TriggerResult(Named("class ResPlaySound", "m_sound", "Done.wav")));
            opener.Results.push_back({ UnknownResultHash, std::nullopt, std::nullopt });
            first.Triggers.push_back(std::move(opener));

            ExtractedZone second;
            second.Path = "Krokotopia/KT_Tomb";
            PropertyObjectPtr torch = Named("class ZoneTokenBoolean", "m_debugName", "TorchLit");
            EXPECT_EQ(torch->Set("m_onClear", Results(PropertyObjectPtr())), PropertySetResult::Ok) << "a result the dump lacks decodes as null";
            second.Objects.push_back(Placed(9, std::move(torch)));
            ExtractedTrigger lighter;
            lighter.Results.push_back(TriggerResult(Named("class ResSetZoneFlag", "m_flagName", "TorchLit")));
            second.Triggers.push_back(std::move(lighter));

            ExtractedZone quiet;
            quiet.Path = "WizardCity/WC_Quiet";

            ZoneExtraction zones;
            zones.Zones = { std::move(first), std::move(second), std::move(quiet) };
            zones.Skipped.push_back({ "Krokotopia/KT_Tomb", "class WizZoneData.m_objectList[0].m_pContainer.m_tokens[0].m_onClear.m_results[0]", UnknownResultHash, false });
            return zones;
        }

        template <typename Row>
        static Row Find(std::vector<Row> const& rows, std::string const& type)
        {
            auto const found = std::find_if(rows.begin(), rows.end(), [&type](Row const& row) { return row.Type == type; });
            return found == rows.end() ? Row{} : *found;
        }

        static TokenHookUsage Hook(ZoneTokenCensus const& census, std::string const& name)
        {
            auto const found = std::find_if(census.GetHooks().begin(), census.GetHooks().end(), [&name](TokenHookUsage const& hook) { return hook.Hook == name; });
            return found == census.GetHooks().end() ? TokenHookUsage{} : *found;
        }

        TypeRegistry _types;
    };
}

TEST_F(ZoneTokenCensusTest, CountsTokensByClassWithTheNamesTheyGiveThemselves)
{
    ZoneTokenCensus const census = ZoneTokenCensus::Take(Zones(), _types.GetCatalog());
    EXPECT_EQ(census.GetPlacementCount(), 2u) << "an object without a token container is not a placement";
    EXPECT_EQ(census.GetTokenCount(), 3u);
    EXPECT_EQ(census.GetUndecodedCount(), 0u);
    ASSERT_EQ(census.GetZones().size(), 2u) << "a zone without tokens is not listed";
    EXPECT_EQ(census.GetZones()[0].Zone, "WizardCity/WC_Tower");
    ASSERT_EQ(census.GetZones()[0].Tokens.size(), 2u);
    EXPECT_EQ(census.GetZones()[0].Tokens[0].Name, "GateOpen");
    EXPECT_EQ(census.GetZones()[0].Tokens[0].ObjectId, 1u);
    EXPECT_EQ(census.GetZones()[0].Tokens[0].Hooks, std::vector<std::string>{ "m_onSet" }) << "a hook whose list holds nothing is not counted";

    ClassUsage const boolean = Find(census.GetTokenClasses(), "ZoneTokenBoolean");
    EXPECT_EQ(boolean.Count, 2u);
    EXPECT_EQ(boolean.Zones, 2u);
    EXPECT_EQ(Find(census.GetTokenClasses(), "ZoneTokenTimer").Count, 1u);
    ASSERT_FALSE(census.GetTokenClasses().empty());
    EXPECT_EQ(census.GetTokenClasses().front().Type, "ZoneTokenBoolean") << "the most used class comes first";
}

TEST_F(ZoneTokenCensusTest, CountsTheHooksThatHoldResultsWithTheirClasses)
{
    ZoneTokenCensus const census = ZoneTokenCensus::Take(Zones(), _types.GetCatalog());
    TokenHookUsage const set = Hook(census, "m_onSet");
    EXPECT_EQ(set.Lists, 1u);
    EXPECT_EQ(Find(set.Results, "ResPlaySound").Count, 1u);
    EXPECT_EQ(Find(set.Results, "ResSetZoneFlag").Count, 1u);
    EXPECT_EQ(Hook(census, "m_onExpire").Lists, 1u);
    TokenHookUsage const timeLeft = Hook(census, "m_timeTriggers.m_onTimeLeft");
    EXPECT_EQ(timeLeft.Lists, 1u) << "a time trigger's hook is named through its list";
    EXPECT_EQ(Find(timeLeft.Results, "ResPlaySound").Count, 1u);
}

TEST_F(ZoneTokenCensusTest, CountsTheTriggersResultsAndRequirementsWithUnknownClassesByHash)
{
    ZoneTokenCensus const census = ZoneTokenCensus::Take(Zones(), _types.GetCatalog());
    EXPECT_EQ(census.GetTriggerCount(), 2u);
    TriggerResultUsage const flag = Find(census.GetTriggerResults(), "ResSetZoneFlag");
    EXPECT_EQ(flag.Results, 2u);
    EXPECT_EQ(flag.Zones, 2u);
    EXPECT_EQ(Find(census.GetTriggerResults(), "ResPlaySound").CooldownResults, 1u);
    EXPECT_EQ(Find(census.GetTriggerResults(), "#123456789").Results, 1u) << "a result the dump lacks is counted by its hash";
    EXPECT_EQ(Find(census.GetTriggerRequirements(), "ReqZoneFlag").Count, 1u);
}

TEST_F(ZoneTokenCensusTest, CountsTheRequirementAndResultPropertiesThatNameAToken)
{
    ZoneTokenCensus const census = ZoneTokenCensus::Take(Zones(), _types.GetCatalog());
    EXPECT_EQ(Find(census.GetReferences(), "trigger result  ResSetZoneFlag.m_flagName").Count, 2u);
    EXPECT_EQ(Find(census.GetReferences(), "trigger result  ResSetZoneFlag.m_flagName").Zones, 2u);
    EXPECT_EQ(Find(census.GetReferences(), "trigger requirement  ReqZoneFlag.m_flagName").Count, 1u);
    EXPECT_EQ(Find(census.GetReferences(), "token result  ResSetZoneFlag.m_flagName").Count, 1u) << "only the flag that names a token counts, not Other";
    EXPECT_EQ(Find(census.GetReferences(), "trigger cooldown result  ResPlaySound.m_sound").Count, 0u);
}

TEST_F(ZoneTokenCensusTest, CountsAHookResultTheDumpLacksByItsHashFromTheSkippedParts)
{
    ZoneTokenCensus const census = ZoneTokenCensus::Take(Zones(), _types.GetCatalog());
    TokenHookUsage const clear = Hook(census, "m_onClear");
    EXPECT_EQ(clear.Lists, 1u) << "the gate's empty list does not count, the torch's does";
    EXPECT_EQ(clear.Zones, 1u);
    ASSERT_EQ(clear.Results.size(), 1u) << "the null entry is counted once, by its hash, not also as undecoded";
    EXPECT_EQ(clear.Results[0].Type, "#123456789");
    EXPECT_EQ(clear.Results[0].Count, 1u);
}
