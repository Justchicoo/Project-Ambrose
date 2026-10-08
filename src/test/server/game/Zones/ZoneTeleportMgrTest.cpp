/*
 * Project Ambrose by Imjustchico
 * The doors of zone_teleport: when several doors fire on one event only the first in data order that has a destination sends the wizard, a door behind requirements fires only while Zone.DoorsIgnoreRequirements lets doors pass and never a gated trigger that is not a door; and, with AMBROSE_TEST_DB set, the rows load against the zones, a row naming a missing zone, location or the wrong same_zone flag fails the load and keeps the old rows, and an edited destination takes hold at `.reload zone_teleport` with nothing restarted.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ReloadMgr.h"
#include "ZoneMgr.h"
#include "ZoneTeleportMgr.h"
#include "ZoneTriggers.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view Hub = "WizardCity/WC_Hub";
    constexpr std::string_view Ravenwood = "WizardCity/WC_Ravenwood";

    ZoneTeleportMgr::Table TwoGateDoors()
    {
        ZoneTeleportMgr::Table table;
        table.emplace(std::make_pair(std::string(Hub), std::string("TeleportToRavenwoodTrigger")),
            ZoneTeleport{ std::string(Hub), "TeleportToRavenwoodTrigger", std::string(Ravenwood), "Target location (Ravenwood Hub Exit)", 0, false });
        table.emplace(std::make_pair(std::string(Hub), std::string("TeleportToShoppingDistrict")),
            ZoneTeleport{ std::string(Hub), "TeleportToShoppingDistrict", "WizardCity/WC_Shop_Area", "Target location (WC_Shop Hub)", 0, false });
        return table;
    }
}

TEST(ZoneTeleportTest, TwoDoorsOnOneEventSendTheWizardThroughOnlyTheFirstWithADestination)
{
    ZoneTeleportMgr::Table const table = TwoGateDoors();

    std::optional<ZoneTeleport> const gate = ZoneTeleportMgr::FirstWithDestination(table, Hub, { "Trigger-ToRavenwoodLite", "TeleportToRavenwoodTrigger" });
    ASSERT_TRUE(gate) << "a door with no row is passed over for the next one in data order";
    EXPECT_EQ(gate->TriggerName, "TeleportToRavenwoodTrigger");
    EXPECT_EQ(gate->DestZone, Ravenwood);

    std::optional<ZoneTeleport> const both = ZoneTeleportMgr::FirstWithDestination(table, Hub, { "TeleportToShoppingDistrict", "TeleportToRavenwoodTrigger" });
    ASSERT_TRUE(both);
    EXPECT_EQ(both->TriggerName, "TeleportToShoppingDistrict") << "of two doors with destinations only the first in data order sends the wizard";

    EXPECT_FALSE(ZoneTeleportMgr::FirstWithDestination(table, Hub, { "Trigger-ToRavenwoodLite" }));
    EXPECT_FALSE(ZoneTeleportMgr::FirstWithDestination(table, Ravenwood, { "TeleportToRavenwoodTrigger" })) << "a row belongs to its own zone";
}

TEST(ZoneTeleportTest, ADoorBehindRequirementsFiresOnlyWhileDoorsMayIgnoreThem)
{
    ZoneTrigger gate{ 28, "TeleportToRavenwoodTrigger", -1, 0.0f, true, { "Enter_Activator Volume (2)" }, {}, true };
    ZoneTrigger dialog{ 16, "Trigger OnDeathFirstTime", -1, 0.0f, true, { "Enter_Activator Volume (2)" }, {}, false };
    ZoneTrigger library{ 25, "TeleportToLibraryTrigger", -1, 0.0f, false, { "Enter_Activator Volume (5)" }, {}, true };
    ZoneTriggers triggers({ gate, dialog, library });
    auto const now = ZoneTriggers::Clock::now();

    EXPECT_TRUE(triggers.Post("Enter_Activator Volume (2)", 7, now).empty()) << "requirements fail closed until they are checked";
    std::vector<ZoneTrigger const*> const passed = triggers.Post("Enter_Activator Volume (2)", 7, now, true);
    ASSERT_EQ(passed.size(), 1u) << "only a door passes its requirements, never another gated trigger";
    EXPECT_EQ(passed.front()->Name, "TeleportToRavenwoodTrigger");
    ASSERT_EQ(triggers.Post("Enter_Activator Volume (5)", 7, now).size(), 1u) << "a door with no requirements needs no setting";
}

namespace
{
    class ZoneTeleportDatabaseTest : public testing::Test
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
            _info.Database = fmt::format("ambrose_door_{:08x}", std::random_device()());
            UpdaterSettings updates;
            updates.AllowPending = true;
            ASSERT_TRUE(DBUpdater::Run(_info, "world", updates));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            Insert("DELETE FROM `zone_teleport`");
            for (std::string_view zone : { Hub, Ravenwood })
                Insert(fmt::format("INSERT INTO `zone_template` (`zone_path`, `display_name_key`, `soft_limit`) VALUES ('{}', 'Key', 50)", zone));
            Insert(fmt::format("INSERT INTO `zone_location` (`zone_path`, `name`, `position_x`, `position_y`, `position_z`, `direction`) VALUES ('{}', 'Start', 0, 48, -24, 5.1), "
                "('{}', 'Target location(WC_Hub Ravenwood)', 10, 20, 30, 1.5), ('{}', 'Start', -5, -1531, -30, 3.1), ('{}', 'Target location (Ravenwood Hub Exit)', 1, 2, 3, 0)",
                Hub, Hub, Ravenwood, Ravenwood));
            sReloadMgr.Clear();
            sZoneMgr.Clear();
            sZoneTeleportMgr.Clear();
            sZoneMgr.RegisterReloadTargets();
            sZoneTeleportMgr.RegisterReloadTargets();
            ASSERT_TRUE(sZoneMgr.LoadAll().Loaded);
        }

        void TearDown() override
        {
            sZoneTeleportMgr.Clear();
            sZoneMgr.Clear();
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

TEST_F(ZoneTeleportDatabaseTest, AnEditedDestinationTakesHoldAtTheReloadAndABadRowKeepsTheOldDoors)
{
    Insert(fmt::format("INSERT INTO `zone_teleport` (`zone`, `trigger_name`, `dest_zone`, `dest_location`) VALUES ('{}', 'TeleportToRavenwoodTrigger', '{}', "
        "'Target location (Ravenwood Hub Exit)'), ('{}', 'Teleport location (to Commons)', '{}', 'Target location(WC_Hub Ravenwood)')", Hub, Ravenwood, Ravenwood, Hub));
    std::vector<std::string> errors;
    ASSERT_TRUE(sZoneTeleportMgr.Load(errors)) << (errors.empty() ? std::string() : errors.front());
    ASSERT_TRUE(sZoneTeleportMgr.Find(Hub, "TeleportToRavenwoodTrigger"));
    EXPECT_EQ(sZoneTeleportMgr.Find(Ravenwood, "Teleport location (to Commons)")->DestLocation, "Target location(WC_Hub Ravenwood)");
    EXPECT_EQ(sZoneTeleportMgr.InZone(Hub).size(), 1u);

    Insert(fmt::format("UPDATE `zone_teleport` SET `dest_location` = '' WHERE `zone` = '{}'", Hub));
    ReloadOutcome const edited = sReloadMgr.Reload(ZoneTeleportMgr::ReloadTarget);
    ASSERT_TRUE(edited.Ok) << (edited.Errors.empty() ? std::string() : edited.Errors.front());
    EXPECT_EQ(sZoneTeleportMgr.Find(Hub, "TeleportToRavenwoodTrigger")->DestLocation, ZoneLocations::StartName) << "an empty destination location means Start";

    Insert(fmt::format("INSERT INTO `zone_teleport` (`zone`, `trigger_name`, `dest_zone`, `dest_location`, `transition_id`, `same_zone`) VALUES "
        "('{}', 'Lost door', 'Nowhere/NoSuchZone', '', 0, 0), ('{}', 'Bad place', '{}', 'No such place', 0, 0), ('{}', 'Wrong flag', '{}', '', 0, 1)", Hub, Hub, Ravenwood, Hub, Ravenwood));
    ReloadOutcome const bad = sReloadMgr.Reload(ZoneTeleportMgr::ReloadTarget);
    EXPECT_FALSE(bad.Ok);
    EXPECT_EQ(bad.Errors.size(), 3u) << "every bad row is reported, not only the first";
    EXPECT_FALSE(sZoneTeleportMgr.Find(Hub, "Lost door")) << "a failed reload keeps the doors that were serving";
    EXPECT_EQ(sZoneTeleportMgr.Find(Hub, "TeleportToRavenwoodTrigger")->DestLocation, ZoneLocations::StartName);
}
