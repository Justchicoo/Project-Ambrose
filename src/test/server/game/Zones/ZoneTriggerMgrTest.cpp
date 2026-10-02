/*
 * Project Ambrose by Imjustchico
 * With AMBROSE_TEST_DB set, reads a zone's volumes, triggers and events from a fresh world database: a volume's enter event fires the trigger that listens for it, editing that trigger's fire event and running `.reload zone_trigger` changes what fires on the next enter with nothing restarted, and a volume row whose shape Ambrose does not know fails the reload, which names the row and keeps the triggers it had; a client may post only the events zone_client_event lists for its zone, and listing an event a volume posts fails the load.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ReloadMgr.h"
#include "ZoneTriggerMgr.h"

#include <fmt/format.h>

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
