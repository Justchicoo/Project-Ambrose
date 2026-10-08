/*
 * Project Ambrose by Imjustchico
 * With AMBROSE_TEST_DB set, reads a zone's volumes, triggers and events from a fresh world database: a volume's enter event fires the trigger that listens for it, editing that trigger's fire event and running `.reload zone_trigger` changes what fires on the next enter with nothing restarted, and a volume row whose shape Ambrose does not know fails the reload, which names the row and keeps the triggers it had; a client may post only the events zone_client_event lists for its zone, and listing an event a volume posts fails the load. Without a database, Ravenwood POI's result as the client data holds it reads as the WizardPOI_00000001 notify text of type 1, and a cut one does not.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ReloadMgr.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "ZoneTriggerMgr.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view Hub = "WizardCity/WC_Hub";

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
    std::optional<ZoneNotifyText> const text = ZoneTriggerMgr::ReadNotifyText(registry.GetCatalog(), bytes, error);
    ASSERT_TRUE(text) << error;
    EXPECT_EQ(text->Text, "WizardPOI_00000001");
    EXPECT_EQ(text->Type, 1);

    bytes.resize(bytes.size() / 2);
    EXPECT_FALSE(ZoneTriggerMgr::ReadNotifyText(registry.GetCatalog(), bytes, error)) << "a cut result must not read";
    EXPECT_FALSE(ZoneTriggerMgr::ReadNotifyText(nullptr, bytes, error));
}
