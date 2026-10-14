/*
 * Project Ambrose by Imjustchico
 * With AMBROSE_TEST_DB set, reads a zone's volumes, triggers and events from a fresh world database: a volume's enter event fires the trigger that listens for it, editing that trigger's fire event and running `.reload zone_trigger` changes what fires on the next enter with nothing restarted, and a volume row whose shape Ambrose does not know fails the reload, which names the row and keeps the triggers it had; a trigger's results load in order, one of a class the server does not know left out and one that does not read failing the load, and a ResRemoveDynaMod reads the tag it takes away or refuses; a client may post only the events zone_client_event lists for its zone, and listing an event a volume posts fails the load. Without a database, Ravenwood POI's result as the client data holds it reads as the WizardPOI_00000001 notify text of type 1, and a cut one does not; an AddDynaMod result reads as the tag and state it names, one with no tag or of another class does not, though one with no tag passes the load as a kept result that changes nothing, and a trigger that fires hands back the results it runs, one with requirements firing only for a wizard whose facts meet them.
 */

#include "CharacterTypeFixtures.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ObjectSerializer.h"
#include "PropertyObject.h"
#include "ReloadMgr.h"
#include "ResultFixtures.h"
#include "ResultMgr.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"
#include "ZoneTriggerMgr.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view Hub = "WizardCity/WC_Hub";

    class HeldQuests : public RequirementContext
    {
    public:
        std::set<std::string, std::less<>> Quests;

        std::optional<bool> HasQuest(std::string_view questName) const override { return Quests.contains(questName); }
    };

    class ZoneTriggerMgrDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            _info = *info;
            _info.Database = fmt::format("ambrose_trig_{:08x}", std::random_device()());
            ASSERT_TRUE(DBUpdater::Run(_info, "world", UpdaterSettings{}));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            Insert(fmt::format("INSERT INTO `zone_template` (`zone_path`, `display_name_key`, `soft_limit`) VALUES ('{}', 'WizardCity_WC_Hub', 50)", Hub));
            Insert(fmt::format("INSERT INTO `zone_volume` (`zone_path`, `volume_index`, `name`, `shape`, `position_x`, `position_y`, `position_z`, `radius`) "
                "VALUES ('{}', 0, 'Ravenwood POI', 'SPHERE', 100, 200, 0, 50), ('{}', 2, 'TeleportVol_Unshaped', '', 0, 0, 0, 30)", Hub, Hub));
            Insert(fmt::format("INSERT INTO `zone_trigger` (`zone_path`, `trigger_index`, `name`, `cooldown`) VALUES ('{}', 0, 'Trigger POI Ravenwood', 0)", Hub));
            Insert(fmt::format("INSERT INTO `zone_trigger_event` (`zone_path`, `owner`, `owner_index`, `kind`, `position`, `event_name`) VALUES ('{}', 'volume', 0, 'enter', 0, 'Enter_Ravenwood POI'), "
                "('{}', 'trigger', 0, 'fire', 0, 'Enter_Ravenwood POI')", Hub, Hub));
            sReloadMgr.Clear();
            sZoneTriggerMgr.Clear();
            sZoneTriggerMgr.RegisterReloadTargets();
        }

        void TearDown() override
        {
            sZoneTriggerMgr.Clear();
            sReloadMgr.Clear();
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&sTypedViewRegistry);
            if (_open)
                WorldDatabase.Close();
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
        }

        void Insert(std::string const& sql)
        {
            ASSERT_TRUE(WorldDatabase.DirectExecute(sql)) << sql;
        }

        MySQLConnectionInfo _info;
        bool _open = false;
        TypedViewRegistry _views;
    };
}

TEST_F(ZoneTriggerMgrDatabaseTest, AnEditedTriggerTakesHoldAtTheReloadAndABadRowKeepsTheOldSet)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(sZoneTriggerMgr.Load(errors)) << (errors.empty() ? std::string() : errors.front());
    std::shared_ptr<ZoneTriggerData const> const data = sZoneTriggerMgr.Find(Hub);
    ASSERT_TRUE(data);
    ASSERT_EQ(data->Volumes.size(), 2u);
    EXPECT_EQ(data->Volumes.front().Shape, VolumeShape::Sphere);
    EXPECT_EQ(data->Volumes.back().Shape, VolumeShape::Sphere) << "a volume that leaves its shape out and gives only a radius is a sphere";
    ASSERT_EQ(data->EnterEvents.at(0).size(), 1u);
    EXPECT_EQ(data->EnterEvents.at(0).front(), "Enter_Ravenwood POI");

    auto const now = ZoneTriggers::Clock::now();
    EXPECT_EQ(sZoneTriggerMgr.Post(1, Hub, "Enter_Ravenwood POI", 7, now), std::vector<std::string>{ "Trigger POI Ravenwood" });

    Insert(fmt::format("UPDATE `zone_trigger_event` SET `event_name` = 'Enter_Something Else' WHERE `zone_path` = '{}' AND `owner` = 'trigger'", Hub));
    ReloadOutcome const reloaded = sReloadMgr.Reload(std::string(ZoneTriggerMgr::ReloadTarget));
    EXPECT_TRUE(reloaded.Ok) << (reloaded.Errors.empty() ? std::string() : reloaded.Errors.front());
    EXPECT_TRUE(sZoneTriggerMgr.Post(1, Hub, "Enter_Ravenwood POI", 8, now).empty()) << "the edited trigger no longer listens for the volume's event";
    EXPECT_EQ(sZoneTriggerMgr.Post(1, Hub, "Enter_Something Else", 8, now), std::vector<std::string>{ "Trigger POI Ravenwood" });

    Insert(fmt::format("INSERT INTO `zone_volume` (`zone_path`, `volume_index`, `name`, `shape`, `radius`) VALUES ('{}', 1, 'Odd', 'TORUS', 5)", Hub));
    errors.clear();
    EXPECT_FALSE(sZoneTriggerMgr.Load(errors));
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors.front().find("volume 1 (Odd) has the shape TORUS"), std::string::npos) << errors.front();
    EXPECT_EQ(sZoneTriggerMgr.Post(1, Hub, "Enter_Something Else", 9, now), std::vector<std::string>{ "Trigger POI Ravenwood" }) << "a failed reload keeps the set it had";
}

TEST_F(ZoneTriggerMgrDatabaseTest, ATriggersResultsLoadInOrderAndOneThatDoesNotReadFailsTheLoad)
{
    sTypeRegistry.Clear();
    sTypeRegistry.SetViews(&_views);
    ASSERT_TRUE(sTypeRegistry.LoadFromText(ResultFixtures::Dump(), "results.json")) << sTypeRegistry.GetErrors().front();
    auto const hex = [](std::vector<uint8> const& bytes)
    {
        std::string text;
        for (uint8 const byte : bytes)
            text += fmt::format("{:02X}", byte);
        return text;
    };
    Insert(fmt::format("INSERT INTO `zone_trigger_result` (`zone_path`, `trigger_index`, `list`, `position`, `class_hash`, `class_name`, `data`) VALUES "
        "('{0}', 0, 'results', 0, 1, '{1}', X'{2}'), ('{0}', 0, 'results', 1, 412163800, NULL, NULL), ('{0}', 0, 'results', 2, 2, '{3}', X'{4}')", Hub,
        ZoneTriggerMgr::NotifyTextClass, hex(ResultFixtures::Encode(ResultFixtures::NotifyText("WizardPOI_00000001", 1))), ResultFixtures::FirstTestClass,
        hex(ResultFixtures::Encode(ResultFixtures::Make(ResultFixtures::FirstTestClass, "after")))));
    std::vector<std::string> errors;
    ASSERT_TRUE(sZoneTriggerMgr.Load(errors)) << (errors.empty() ? std::string() : errors.front());
    std::vector<ResultSource> fired;
    sZoneTriggerMgr.Post(1, Hub, "Enter_Ravenwood POI", 7, ZoneTriggers::Clock::now(), &fired);
    ASSERT_EQ(fired.size(), 1u);
    ASSERT_EQ(fired.front().Results.size(), 2u) << "a result of a class the server does not know is held without bytes and left out";
    EXPECT_EQ(fired.front().Results[0].Class, ZoneTriggerMgr::NotifyTextClass);
    EXPECT_EQ(fired.front().Results[1].Class, ResultFixtures::FirstTestClass);
    EXPECT_EQ(fired.front().Results[1].Position, 2u);

    Insert(fmt::format("INSERT INTO `zone_trigger_result` (`zone_path`, `trigger_index`, `list`, `position`, `class_hash`, `class_name`, `data`) VALUES "
        "('{}', 0, 'results', 3, 1, '{}', X'{}')", Hub, ZoneTriggerMgr::NotifyTextClass, hex(ResultFixtures::Encode(ResultFixtures::NotifyText("", 0)))));
    errors.clear();
    EXPECT_FALSE(sZoneTriggerMgr.Load(errors));
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors.front().find("notify text result at 3 that does not read: its m_text is empty"), std::string::npos) << errors.front();
}

TEST_F(ZoneTriggerMgrDatabaseTest, ClientsMayPostOnlyTheEventsTheirZoneListsAndNeverAVolumesOwn)
{
    Insert(fmt::format("INSERT INTO `zone_client_event` (`zone_path`, `event_name`) VALUES ('{}', 'CinematicDone')", Hub));
    std::vector<std::string> errors;
    ASSERT_TRUE(sZoneTriggerMgr.Load(errors)) << (errors.empty() ? std::string() : errors.front());
    std::shared_ptr<ZoneTriggerData const> const data = sZoneTriggerMgr.Find(Hub);
    ASSERT_TRUE(data);
    EXPECT_TRUE(data->ClientEvents.contains("CinematicDone"));
    EXPECT_FALSE(data->ClientEvents.contains("Enter_Ravenwood POI"));

    Insert(fmt::format("INSERT INTO `zone_client_event` (`zone_path`, `event_name`) VALUES ('{}', 'Enter_Ravenwood POI')", Hub));
    errors.clear();
    EXPECT_FALSE(sZoneTriggerMgr.Load(errors));
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors.front().find("volume 0 posts"), std::string::npos) << errors.front();
    EXPECT_FALSE(sZoneTriggerMgr.Find(Hub)->ClientEvents.contains("Enter_Ravenwood POI")) << "a failed reload keeps the set it had";
}

TEST(ZoneTriggerResultTest, RavenwoodsPoiResultReadsAsTheNotifyTextItShows)
{
    nlohmann::json properties = nlohmann::json::object();
    auto const add = [&properties](std::string const& name, std::string const& type, uint32 id, uint32 hash)
    {
        properties[name] = nlohmann::json{ { "type", type }, { "id", id }, { "offset", 8 * (id + 1) }, { "flags", 7 }, { "container", "Static" }, { "dynamic", false },
            { "singleton", false }, { "pointer", false }, { "hash", hash } };
    };
    add("#783721823", "bool", 0, 783721823);
    add("m_text", "std::string", 1, 1717580128);
    add("m_type", "int", 2, 219902012);
    add("#2122593183", "bool", 3, 2122593183);
    add("m_radius", "float", 4, 989410271);
    add("m_volumeX", "float", 5, 1125210087);
    add("m_volumeY", "float", 6, 1125210088);
    add("m_volumeZ", "float", 7, 1125210089);
    add("#1475192380", "bool", 8, 1475192380);
    nlohmann::json classes = nlohmann::json::object();
    uint32 const base = StringHash::KiStringHash("class PropertyClass");
    classes[std::to_string(base)] = nlohmann::json{ { "name", "class PropertyClass" }, { "bases", nlohmann::json::array() }, { "hash", base }, { "properties", nlohmann::json::object() } };
    classes["2001472307"] = nlohmann::json{ { "name", "class ResClientNotifyText" }, { "bases", nlohmann::json::array({ "class PropertyClass" }) }, { "hash", 2001472307 },
        { "properties", properties } };
    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromText(nlohmann::json{ { "version", 2 }, { "classes", classes } }.dump(), "notify.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());

    std::string const hex = "330B4C77B1030000410000005FA5B62E00E700000060316066120057697A617264504F495F3030303030303031600000003C701B0D01000000410000009F33847E0067000000"
                            "DF33F93A0000000060000000E75711430000000060000000E85711430000000060000000E957114300000000410000003CA6ED5700";
    std::vector<uint8> bytes;
    for (std::size_t at = 0; at + 1 < hex.size(); at += 2)
        bytes.push_back(static_cast<uint8>(std::stoi(hex.substr(at, 2), nullptr, 16)));

    std::string error;
    std::shared_ptr<PropertyObject const> const result = ResultMgr::Decode(registry.GetCatalog(), bytes, error);
    ASSERT_TRUE(result) << error;
    std::optional<ZoneNotifyText> const text = ZoneTriggerMgr::NotifyTextOf(*result, error);
    ASSERT_TRUE(text) << error;
    EXPECT_EQ(text->Text, "WizardPOI_00000001");
    EXPECT_EQ(text->Type, 1);

    std::vector<uint8> const whole = bytes;
    bytes.resize(bytes.size() / 2);
    EXPECT_FALSE(ResultMgr::Decode(registry.GetCatalog(), bytes, error)) << "a cut result must not read";
    EXPECT_FALSE(ResultMgr::Decode(nullptr, whole, error));
}

TEST(ZoneTriggerResultTest, AnAddDynaModResultReadsAsTheModItMakesAndAFiredTriggerHandsItBack)
{
    using CharacterTypeFixtures::Detail::AddClass;
    using CharacterTypeFixtures::Detail::Json;
    using CharacterTypeFixtures::Detail::Property;
    Json classes = Json::object();
    AddClass(classes, "class PropertyClass", Json::array(), {});
    AddClass(classes, "class ResAddDynaMod", Json::array({ "PropertyClass" }), { { "m_dynaModClientTag", Property("std::string", "m_dynaModClientTag", 0, 7) },
        { "m_dynaModRemove", Property("bool", "m_dynaModRemove", 1, 7) }, { "m_dynaModState", Property("std::string", "m_dynaModState", 2, 7) },
        { "m_zoneName", Property("std::string", "m_zoneName", 3, 7) }, { "m_bShowsObject", Property("bool", "m_bShowsObject", 4, 7) } });
    AddClass(classes, "class ResRemoveDynaMod", Json::array({ "PropertyClass" }), { { "m_dynaModClientTag", Property("std::string", "m_dynaModClientTag", 0, 7) },
        { "m_useQuestAsOriginator", Property("bool", "m_useQuestAsOriginator", 1, 7) } });
    AddClass(classes, "class ResClientNotifyText", Json::array({ "PropertyClass" }), { { "m_text", Property("std::string", "m_text", 0, 7) } });
    TypeRegistry registry;
    ASSERT_TRUE(registry.LoadFromText(Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump(), "dynamod.json")) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front());
    TypeCatalogPtr const catalog = registry.GetCatalog();

    SerializerOptions options;
    options.Versionable = true;
    options.Mask = 0;
    auto const encode = [&](std::string_view className, std::string const& tag, bool shows)
    {
        PropertyObjectPtr const result = PropertyObject::Create(catalog, className);
        EXPECT_TRUE(result);
        if (className == ZoneTriggerMgr::DynaModClass)
        {
            EXPECT_EQ(result->Set("m_dynaModClientTag", std::string(tag)), PropertySetResult::Ok);
            EXPECT_EQ(result->Set("m_dynaModState", std::string("IdleOpen")), PropertySetResult::Ok);
            EXPECT_EQ(result->Set("m_bShowsObject", shows), PropertySetResult::Ok);
        }
        else if (className == ZoneTriggerMgr::RemoveDynaModClass)
        {
            EXPECT_EQ(result->Set("m_dynaModClientTag", std::string(tag)), PropertySetResult::Ok);
        }
        EncodeResult const encoded = ObjectSerializer::Encode(result.get(), options);
        EXPECT_TRUE(encoded.Ok()) << encoded.Detail;
        return encoded.Bytes;
    };

    auto const read = [&](std::string_view className, std::string const& tag, bool shows = true)
    {
        std::string problem;
        std::shared_ptr<PropertyObject const> result = ResultMgr::Decode(catalog, encode(className, tag, shows), problem);
        EXPECT_TRUE(result) << problem;
        return ResultEntry{ std::string(className), std::move(result), 0 };
    };
    std::string error;
    ResultEntry const gate = read(ZoneTriggerMgr::DynaModClass, "WC_GateCommons_ToUnicornWay");
    std::optional<ZoneDynaMod> const mod = ZoneTriggerMgr::DynaModOf(*gate.Object, error);
    ASSERT_TRUE(mod) << error;
    EXPECT_EQ(*mod, (ZoneDynaMod{ "WC_GateCommons_ToUnicornWay", "IdleOpen", false, "", true }));
    std::optional<ZoneDynaMod> const stateOnly = ZoneTriggerMgr::DynaModOf(*read(ZoneTriggerMgr::DynaModClass, "AV-QuestLight-KnightsHall", false).Object, error);
    ASSERT_TRUE(stateOnly) << error;
    EXPECT_FALSE(stateOnly->ShowsObject) << "a mod whose m_bShowsObject is clear only sets its objects' state";
    ResultEntry const untagged = read(ZoneTriggerMgr::DynaModClass, "");
    EXPECT_FALSE(ZoneTriggerMgr::DynaModOf(*untagged.Object, error)) << "a mod must name a tag";
    EXPECT_FALSE(ZoneTriggerMgr::DynaModOf(*read(ZoneTriggerMgr::NotifyTextClass, "").Object, error));
    EXPECT_NE(error.find("ResClientNotifyText"), std::string::npos) << error;
    EXPECT_EQ(ZoneTriggerMgr::CheckResult(gate, error), ZoneTriggerMgr::ResultCheck::Good);
    EXPECT_EQ(ZoneTriggerMgr::CheckResult(untagged, error), ZoneTriggerMgr::ResultCheck::Untagged) << "the client data holds mods that name no tag, and one must not fail the load";
    EXPECT_EQ(ZoneTriggerMgr::CheckResult(read(ZoneTriggerMgr::NotifyTextClass, ""), error), ZoneTriggerMgr::ResultCheck::Refused);

    ResultEntry const dropped = read(ZoneTriggerMgr::RemoveDynaModClass, "AV-QuestLight-KnightsHall");
    EXPECT_EQ(ZoneTriggerMgr::RemoveDynaModTagOf(*dropped.Object, error), "AV-QuestLight-KnightsHall");
    EXPECT_FALSE(ZoneTriggerMgr::RemoveDynaModTagOf(*read(ZoneTriggerMgr::RemoveDynaModClass, "").Object, error)) << "a removal must name the tag it takes away";
    EXPECT_FALSE(ZoneTriggerMgr::RemoveDynaModTagOf(*gate.Object, error));
    EXPECT_NE(error.find("ResAddDynaMod"), std::string::npos) << error;

    ZoneTriggerData data;
    data.Triggers.push_back({ 3, "Trigger OpenGateToUW", -1, 0.0f, false, { "GoalComplete_WC-MAIN-C01-003_WC-MAIN-C01-003_Goal0" }, false, { gate } });
    data.Triggers.push_back({ 4, "Trigger-OpenPetPark", -1, 0.0f, true, { "EnterZone" }, false, { read(ZoneTriggerMgr::DynaModClass, "PetParkCollision") } });
    RequirementTree petQuest;
    RequirementRow holds;
    holds.Type = "ReqHasQuest";
    holds.QuestName = "WC-PET-C08-001";
    petQuest.Requirements.push_back(std::move(holds));
    data.Requirements.emplace(4, std::make_shared<RequirementTree const>(std::move(petQuest)));
    std::map<std::string, ZoneTriggerData, std::less<>> zones;
    zones.emplace(std::string(Hub), std::move(data));
    sZoneTriggerMgr.Replace(std::move(zones));
    std::vector<ResultSource> fired;
    EXPECT_EQ(sZoneTriggerMgr.Post(1, Hub, "GoalComplete_WC-MAIN-C01-003_WC-MAIN-C01-003_Goal0", 7, ZoneTriggers::Clock::now(), &fired),
        std::vector<std::string>{ "Trigger OpenGateToUW" });
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired.front().Name, "Trigger OpenGateToUW");
    ASSERT_EQ(fired.front().Results.size(), 1u);
    EXPECT_EQ(ZoneTriggerMgr::DynaModOf(*fired.front().Results.front().Object, error)->Tag, "WC_GateCommons_ToUnicornWay");

    HeldQuests wizard;
    fired.clear();
    EXPECT_TRUE(sZoneTriggerMgr.Post(1, Hub, "EnterZone", 7, ZoneTriggers::Clock::now(), &fired).empty()) << "requirements fail closed with no facts to check";
    EXPECT_TRUE(sZoneTriggerMgr.Post(1, Hub, "EnterZone", 7, ZoneTriggers::Clock::now(), &fired, false, &wizard).empty()) << "the wizard lacks the quest";
    wizard.Quests.insert("WC-PET-C08-001");
    EXPECT_EQ(sZoneTriggerMgr.Post(1, Hub, "EnterZone", 7, ZoneTriggers::Clock::now(), &fired, false, &wizard), std::vector<std::string>{ "Trigger-OpenPetPark" });
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(ZoneTriggerMgr::DynaModOf(*fired.front().Results.front().Object, error)->Tag, "PetParkCollision");
    sZoneTriggerMgr.Clear();
}
