/*
 * Project Ambrose by Imjustchico
 * Tests what the zone stores promise. Without a database: a place asked for by name is the place that comes back, facing the way its data says, a name the zone does not hold falls back to its Start rather than to nothing, a zone with no Start and no such name says so instead of guessing, a path no template holds is a typed refusal rather than an empty answer, and custom GM teleport points stay distinct by zone. With AMBROSE_TEST_DB set, against a real world database: the zone tables and GM teleport points load into the stores, invalid locations or objects leave the rows already serving exactly where they were, and reloading a target alone swaps in its new data without restarting.
 */

#include "DBUpdater.h"
#include "TestDatabaseUpdates.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ReloadMgr.h"
#include "ZoneMgr.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr char const* Hub = "WizardCity/WC_Hub";
    constexpr char const* Ravenwood = "WizardCity/WC_Ravenwood";

    ZoneTemplates MakeTemplates()
    {
        std::map<std::string, ZoneTemplate, std::less<>> byPath;
        ZoneTemplate hub;
        hub.Path = Hub;
        hub.DisplayNameKey = "WizardCity_WC_Hub";
        hub.SoftLimit = 50;
        byPath.emplace(hub.Path, hub);
        ZoneTemplate ravenwood;
        ravenwood.Path = Ravenwood;
        ravenwood.DisplayNameKey = "WizardCity_WC_Ravenwood";
        byPath.emplace(ravenwood.Path, ravenwood);
        return ZoneTemplates(std::move(byPath));
    }

    ZoneLocations MakeLocations()
    {
        std::map<std::string, std::vector<ZoneLocation>, std::less<>> byZone;
        byZone[Hub] = {
            ZoneLocation{ 1, "Start", 10.0f, 20.0f, 30.0f, 1.5f },
            ZoneLocation{ 2, "Door_Ravenwood", -5.0f, 0.0f, 3.0f, -0.75f },
        };
        byZone[Ravenwood] = { ZoneLocation{ 3, "Door_Hub", 7.0f, 8.0f, 9.0f, 0.25f } };
        return ZoneLocations(std::move(byZone));
    }
}

TEST(ZoneMgrTest, APlaceAskedForByNameIsThePlaceThatComesBack)
{
    ZoneLocations const locations = MakeLocations();

    ZonePlace const start = locations.Find(Hub, "Start");
    ASSERT_TRUE(start.Found());
    EXPECT_EQ(start.Result, ZoneLookup::Ok);
    EXPECT_FLOAT_EQ(start.Location.X, 10.0f);
    EXPECT_FLOAT_EQ(start.Location.Y, 20.0f);
    EXPECT_FLOAT_EQ(start.Location.Z, 30.0f);
    EXPECT_FLOAT_EQ(start.Location.Yaw, 1.5f);

    ZonePlace const door = locations.Find(Hub, "Door_Ravenwood");
    ASSERT_TRUE(door.Found());
    EXPECT_EQ(door.Result, ZoneLookup::Ok);
    EXPECT_FLOAT_EQ(door.Location.Yaw, -0.75f);
}

TEST(ZoneMgrTest, ANameTheZoneDoesNotHoldFallsBackToItsStart)
{
    ZoneLocations const locations = MakeLocations();

    ZonePlace const gone = locations.Find(Hub, "Door_ThatWasRemoved");
    ASSERT_TRUE(gone.Found()) << "a wizard sent to a door that is no longer there must still stand somewhere";
    EXPECT_EQ(gone.Result, ZoneLookup::FellBackToStart);
    EXPECT_FLOAT_EQ(gone.Location.X, 10.0f);
    EXPECT_EQ(gone.Location.Name, "Start");
}

TEST(ZoneMgrTest, AZoneWithNoStartAndNoSuchNameSaysSoRatherThanGuessing)
{
    ZoneLocations const locations = MakeLocations();

    ZonePlace const nowhere = locations.Find(Ravenwood, "Door_ThatWasRemoved");
    EXPECT_FALSE(nowhere.Found());
    EXPECT_EQ(nowhere.Result, ZoneLookup::NoLocations);
    EXPECT_EQ(nowhere.Location.Name, "");
}

TEST(ZoneMgrTest, APathNoTemplateHoldsIsATypedRefusal)
{
    ZoneLocations const locations = MakeLocations();

    ZonePlace const unknown = locations.Find("Krokotopia/KT_Hub", "Start");
    EXPECT_FALSE(unknown.Found());
    EXPECT_EQ(unknown.Result, ZoneLookup::UnknownZone);
    EXPECT_NE(ZoneMgr::GetLookupName(ZoneLookup::UnknownZone).find("no zone"), std::string_view::npos);
}

TEST(ZoneMgrTest, TheStoresCountWhatTheyHold)
{
    ZoneTemplates const templates = MakeTemplates();
    ZoneLocations const locations = MakeLocations();

    EXPECT_EQ(templates.Count(), 2u);
    EXPECT_TRUE(templates.Has(Hub));
    EXPECT_FALSE(templates.Has("Krokotopia/KT_Hub"));
    ASSERT_NE(templates.Find(Hub), nullptr);
    EXPECT_EQ(templates.Find(Hub)->DisplayNameKey, "WizardCity_WC_Hub");
    EXPECT_EQ(locations.Count(), 3u);
    EXPECT_EQ(locations.ZoneCount(), 2u);
    ASSERT_NE(locations.In(Hub), nullptr);
    EXPECT_EQ(locations.In(Hub)->size(), 2u);
    EXPECT_EQ(locations.In("Krokotopia/KT_Hub"), nullptr);
}

TEST(ZoneMgrTest, GameTeleportsAreNamedWithinTheirZoneAndCanBeAddedAndRemoved)
{
    GameTeleports teleports;
    ASSERT_TRUE(teleports.Add(Hub, GameTelePoint{ "Fountain", 4.0f, 8.0f, 12.0f, 0.5f }));
    ASSERT_TRUE(teleports.Add(Ravenwood, GameTelePoint{ "Fountain", 20.0f, 24.0f, 28.0f, 1.0f }));
    EXPECT_FALSE(teleports.Add(Hub, GameTelePoint{ "Fountain", 0.0f, 0.0f, 0.0f, 0.0f }));
    ASSERT_NE(teleports.Find(Hub, "Fountain"), nullptr);
    EXPECT_FLOAT_EQ(teleports.Find(Hub, "Fountain")->X, 4.0f);
    ASSERT_NE(teleports.Find(Ravenwood, "Fountain"), nullptr);
    EXPECT_FLOAT_EQ(teleports.Find(Ravenwood, "Fountain")->X, 20.0f);
    EXPECT_EQ(teleports.Count(), 2u);

    EXPECT_TRUE(teleports.Remove(Hub, "Fountain"));
    EXPECT_FALSE(teleports.Remove(Hub, "Fountain"));
    EXPECT_EQ(teleports.Find(Hub, "Fountain"), nullptr);
    EXPECT_NE(teleports.Find(Ravenwood, "Fountain"), nullptr);
    EXPECT_EQ(teleports.Count(), 1u);
}

namespace
{
    class ZoneMgrDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            _worldInfo = *info;
            _worldInfo.Database = fmt::format("ambrose_zone_{:08x}", std::random_device()());
            ASSERT_TRUE(AmbroseTestDatabase::RunWorldUpdates(_worldInfo));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_worldInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;

            Insert(fmt::format("INSERT INTO `zone_template` (`zone_path`, `display_name_key`, `soft_limit`) VALUES ('{}', 'WizardCity_WC_Hub', 50)", Hub));
            Insert(fmt::format("INSERT INTO `zone_location` (`zone_path`, `name`, `position_x`, `position_y`, `position_z`, `direction`) VALUES ('{}', 'Start', 10, 20, 30, 1.5)", Hub));
            Insert(fmt::format("INSERT INTO `zone_object` (`zone_path`, `class_name`, `object_id`, `template_id`, `position_x`, `position_y`, `position_z`, `orientation_x`, `orientation_y`, "
                "`orientation_z`, `scale`, `zone_tag`, `start_state`, `loading_type`, `spawn_requirements`) VALUES ('{}', 'class CoreObjectInfo', 7, 4242, 1, 2, 3, 0, 0, 0.5, 1.25, 'Hub', 'Idle', 3, "
                "X'00')", Hub));
            sReloadMgr.Clear();
            sZoneMgr.Clear();
            sZoneMgr.RegisterReloadTargets();
        }

        void TearDown() override
        {
            sZoneMgr.Clear();
            sReloadMgr.Clear();
            if (_open)
                WorldDatabase.Close();
            if (_worldInfo.Database.empty())
                return;
            MySQLConnectionInfo server = _worldInfo;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_worldInfo.Database)));
        }

        void Insert(std::string const& sql)
        {
            ASSERT_TRUE(WorldDatabase.DirectExecute(sql)) << sql;
        }

        MySQLConnectionInfo _worldInfo;
        bool _open = false;
    };
}

TEST_F(ZoneMgrDatabaseTest, TheZoneAndTeleportTablesLoadIntoTheirStores)
{
    ZoneLoadResult const loaded = sZoneMgr.LoadAll();
    ASSERT_TRUE(loaded.Loaded) << (loaded.Errors.empty() ? std::string() : loaded.Errors.front());
    EXPECT_EQ(loaded.Zones, 1u);
    EXPECT_EQ(loaded.Locations, 1u);
    EXPECT_EQ(loaded.Objects, 1u);
    EXPECT_EQ(loaded.GameTeles, 0u);

    ZonePlace const start = sZoneMgr.FindPlace(Hub, "Start");
    ASSERT_TRUE(start.Found());
    EXPECT_FLOAT_EQ(start.Location.X, 10.0f);
    EXPECT_FLOAT_EQ(start.Location.Z, 30.0f);
    EXPECT_FLOAT_EQ(start.Location.Yaw, 1.5f);

    std::vector<ZoneObjectSpawn> const* const objects = sZoneMgr.GetObjects()->In(Hub);
    ASSERT_NE(objects, nullptr);
    ASSERT_EQ(objects->size(), 1u);
    ZoneObjectSpawn const& object = objects->front();
    EXPECT_EQ(object.ClassName, "class CoreObjectInfo");
    EXPECT_EQ(object.ObjectId, 7u);
    EXPECT_EQ(object.TemplateId, 4242u);
    EXPECT_EQ(object.Position, (PropertyTypes::Vector3D{ 1.0f, 2.0f, 3.0f }));
    EXPECT_EQ(object.Orientation, (PropertyTypes::Vector3D{ 0.0f, 0.0f, 0.5f })) << "the orientation is the vector the zone data gives, which is what the client is sent";
    EXPECT_FLOAT_EQ(object.Scale, 1.25f);
    EXPECT_EQ(object.Tag, "Hub");
    EXPECT_EQ(object.StartState, "Idle");
    EXPECT_EQ(object.Loading, ZoneObjectLoading::DynamicServer);
    EXPECT_TRUE(object.IsSentByServer());
    EXPECT_TRUE(object.HasSpawnRequirements);

    std::optional<std::string> const described = sZoneMgr.Describe(Hub);
    ASSERT_TRUE(described);
    EXPECT_NE(described->find("1 named place(s)"), std::string::npos) << *described;
    EXPECT_NE(described->find("1 placed object(s)"), std::string::npos) << *described;
    EXPECT_NE(described->find("WizardCity_WC_Hub"), std::string::npos) << *described;
    EXPECT_FALSE(sZoneMgr.Describe("Krokotopia/KT_Hub").has_value());
}

TEST_F(ZoneMgrDatabaseTest, AGameTeleReloadSwapsTheNamedPointAndKeepsTheServingListOnFailure)
{
    ASSERT_TRUE(sZoneMgr.LoadAll().Loaded);
    Insert(fmt::format("INSERT INTO `game_tele` (`zone_path`, `name`, `position_x`, `position_y`, `position_z`, `direction`) VALUES ('{}', 'Fountain', 4, 8, 12, 0.5)", Hub));
    EXPECT_FALSE(sZoneMgr.FindGameTele(Hub, "Fountain")) << "a row is not live before its reload target runs";

    ReloadOutcome const first = sReloadMgr.Reload(ZoneMgr::GameTeleTarget);
    ASSERT_TRUE(first.Ok) << (first.Errors.empty() ? std::string() : first.Errors.front());
    std::optional<GameTelePoint> const loaded = sZoneMgr.FindGameTele(Hub, "Fountain");
    ASSERT_TRUE(loaded);
    EXPECT_FLOAT_EQ(loaded->X, 4.0f);

    ASSERT_TRUE(WorldDatabase.DirectExecute(fmt::format("UPDATE `game_tele` SET `position_x` = 99 WHERE `zone_path` = '{}' AND `name` = 'Fountain'", Hub)));
    EXPECT_FLOAT_EQ(sZoneMgr.FindGameTele(Hub, "Fountain")->X, 4.0f) << "the current snapshot remains live until reload";
    ReloadOutcome const updated = sReloadMgr.Reload(ZoneMgr::GameTeleTarget);
    ASSERT_TRUE(updated.Ok) << (updated.Errors.empty() ? std::string() : updated.Errors.front());
    EXPECT_FLOAT_EQ(sZoneMgr.FindGameTele(Hub, "Fountain")->X, 99.0f);

    ASSERT_TRUE(WorldDatabase.DirectExecute("SET FOREIGN_KEY_CHECKS = 0"));
    Insert("INSERT INTO `game_tele` (`zone_path`, `name`, `position_x`, `position_y`, `position_z`, `direction`) VALUES ('Nowhere/Unknown', 'Broken', 1, 2, 3, 0)");
    ReloadOutcome const rejected = sReloadMgr.Reload(ZoneMgr::GameTeleTarget);
    EXPECT_FALSE(rejected.Ok);
    ASSERT_FALSE(rejected.Errors.empty());
    EXPECT_NE(rejected.Errors.front().find("Nowhere/Unknown"), std::string::npos);
    EXPECT_FLOAT_EQ(sZoneMgr.FindGameTele(Hub, "Fountain")->X, 99.0f) << "a failed build leaves the serving generation in place";
    EXPECT_TRUE(WorldDatabase.DirectExecute("SET FOREIGN_KEY_CHECKS = 1"));
}

TEST_F(ZoneMgrDatabaseTest, ARowNamingAZoneNoTemplateHoldsFailsTheBuildAndKeepsWhatWasServing)
{
    ASSERT_TRUE(sZoneMgr.LoadAll().Loaded);
    ASSERT_TRUE(WorldDatabase.DirectExecute("SET FOREIGN_KEY_CHECKS = 0"));
    Insert(fmt::format("INSERT INTO `zone_location` (`zone_path`, `name`, `position_x`, `position_y`, `position_z`) VALUES ('{}', 'Start', 1, 1, 1)", Ravenwood));

    ReloadOutcome const outcome = sReloadMgr.Reload(ZoneMgr::LocationTarget);
    EXPECT_FALSE(outcome.Ok);
    ASSERT_FALSE(outcome.Errors.empty());
    EXPECT_NE(outcome.Errors.front().find(Ravenwood), std::string::npos) << outcome.Errors.front();

    ZonePlace const start = sZoneMgr.FindPlace(Hub, "Start");
    ASSERT_TRUE(start.Found()) << "a build that failed leaves the places already loaded exactly where they were";
    EXPECT_FLOAT_EQ(start.Location.X, 10.0f);
    EXPECT_EQ(sZoneMgr.GetLocations()->Count(), 1u);
}

TEST_F(ZoneMgrDatabaseTest, AnObjectWithALoadingTypeTheClientDoesNotHaveFailsTheBuild)
{
    ASSERT_TRUE(sZoneMgr.LoadAll().Loaded);
    Insert(fmt::format("INSERT INTO `zone_object` (`zone_path`, `class_name`, `object_id`, `template_id`, `loading_type`) VALUES ('{}', 'class CoreObjectInfo', 8, 4243, 7)", Hub));

    ReloadOutcome const outcome = sReloadMgr.Reload(ZoneMgr::ObjectTarget);
    EXPECT_FALSE(outcome.Ok);
    ASSERT_FALSE(outcome.Errors.empty());
    EXPECT_NE(outcome.Errors.front().find("loading type 7"), std::string::npos) << outcome.Errors.front();
    EXPECT_EQ(sZoneMgr.GetObjects()->Count(), 1u) << "a build that failed leaves the objects already loaded where they were";
}

TEST_F(ZoneMgrDatabaseTest, EditingARowAndReloadingThatTargetReturnsTheNewCoordinates)
{
    ASSERT_TRUE(sZoneMgr.LoadAll().Loaded);
    ASSERT_TRUE(WorldDatabase.DirectExecute(fmt::format("UPDATE `zone_location` SET `position_x` = 99, `position_y` = 98, `position_z` = 97 WHERE `zone_path` = '{}' AND `name` = 'Start'", Hub)));

    ZonePlace const before = sZoneMgr.FindPlace(Hub, "Start");
    ASSERT_TRUE(before.Found());
    EXPECT_FLOAT_EQ(before.Location.X, 10.0f) << "nothing changes until the target is reloaded";

    ReloadOutcome const outcome = sReloadMgr.Reload(ZoneMgr::LocationTarget);
    EXPECT_TRUE(outcome.Ok) << (outcome.Errors.empty() ? std::string() : outcome.Errors.front());

    ZonePlace const after = sZoneMgr.FindPlace(Hub, "Start");
    ASSERT_TRUE(after.Found());
    EXPECT_FLOAT_EQ(after.Location.X, 99.0f);
    EXPECT_FLOAT_EQ(after.Location.Z, 97.0f);
}
