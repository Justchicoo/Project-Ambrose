/*
 * Project Ambrose by Imjustchico
 * Tests the zone paths: a set of rows builds each path with its nodes in order, and a set with a path in no zone, a node of a path that is not there, a gap in a path's nodes, a node that is not a number or a path with no nodes fails its build and names each row; and with AMBROSE_TEST_DB set, `.reload zone_path` serves a moved node and a reload over a broken row keeps the old paths serving.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ReloadMgr.h"
#include "ZonePathMgr.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
    constexpr char const* Hub = "WizardCity/WC_Hub";

    ZonePathNodeRow Node(std::string zone, uint64 path, uint32 position, uint64 id, float x)
    {
        return { std::move(zone), path, position, { id, { x, 0.0f, 0.0f } } };
    }

    ZonePath Path(uint64 id, std::string name)
    {
        ZonePath path;
        path.Id = id;
        path.Name = std::move(name);
        return path;
    }

    class ZonePathDatabaseTest : public testing::Test
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
            _info.Database = fmt::format("ambrose_path_{:08x}", std::random_device()());
            UpdaterSettings updates;
            updates.AllowPending = true;
            ASSERT_TRUE(DBUpdater::Run(_info, "world", updates));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            Execute(fmt::format("INSERT INTO `zone_template` (`zone_path`, `display_name_key`) VALUES ('{}', 'WizardCity_WC_Hub')", Hub));
            Execute(fmt::format("INSERT INTO `zone_path` (`zone_path`, `path_id`, `name`) VALUES ('{}', 8021750, 'Path_Flax_01')", Hub));
            Execute(fmt::format("INSERT INTO `zone_path_node` (`zone_path`, `path_id`, `position`, `node_id`, `position_x`, `position_y`, `position_z`) "
                "VALUES ('{}', 8021750, 0, 109, 10, 20, 0), ('{}', 8021750, 1, 110, 30, 40, 0)", Hub, Hub));
            sReloadMgr.Clear();
            sZonePathMgr.Clear();
            sZonePathMgr.RegisterReloadTargets();
        }

        void TearDown() override
        {
            sZonePathMgr.Clear();
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

        void Execute(std::string const& sql)
        {
            ASSERT_TRUE(WorldDatabase.DirectExecute(sql)) << sql;
        }

        MySQLConnectionInfo _info;
        bool _open = false;
    };
}

TEST(ZonePathMgrTest, ASetOfRowsBuildsEachPathWithItsNodesInOrder)
{
    std::vector<std::string> errors;
    std::optional<ZonePaths> const built = ZonePathMgr::Build({ { Hub, Path(8021750, "Path_Flax_01") } },
        { Node(Hub, 8021750, 1, 110, 2.0f), Node(Hub, 8021750, 0, 109, 1.0f), Node(Hub, 8021750, 2, 111, 3.0f) }, { Hub }, errors);
    ASSERT_TRUE(built) << errors.front();
    ZonePath const* const path = built->Find(Hub, 8021750);
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->Name, "Path_Flax_01");
    ASSERT_EQ(path->Nodes.size(), 3u);
    EXPECT_EQ(path->Nodes[0].Id, 109u);
    EXPECT_EQ(path->Nodes[1].Id, 110u);
    EXPECT_EQ(path->Nodes[2].Id, 111u);
    EXPECT_EQ(built->Find(Hub, 1), nullptr);
    EXPECT_EQ(built->Find("WizardCity/Nowhere", 8021750), nullptr);
    EXPECT_EQ(built->Count(), 1u);
    EXPECT_EQ(built->NodeCount(), 3u);
}

TEST(ZonePathMgrTest, ASetWithABrokenRowFailsItsBuildAndNamesEachRow)
{
    std::vector<std::string> errors;
    float const broken = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(ZonePathMgr::Build({ { Hub, Path(1, "Gapped") }, { "WizardCity/Nowhere", Path(2, "Lost") }, { Hub, Path(3, "Empty") }, { Hub, Path(4, "Broken") } },
        { Node(Hub, 1, 0, 10, 0.0f), Node(Hub, 1, 2, 12, 0.0f), Node(Hub, 9, 0, 90, 0.0f), Node(Hub, 4, 0, 40, broken) }, { Hub }, errors));
    std::string const report = fmt::format("{}", fmt::join(errors, "\n"));
    EXPECT_NE(report.find("WizardCity/Nowhere 2 (Lost) names a zone"), std::string::npos) << report;
    EXPECT_NE(report.find("1 2 does not follow"), std::string::npos) << report;
    EXPECT_NE(report.find("9 0 belongs to a path that is not there"), std::string::npos) << report;
    EXPECT_NE(report.find("4 0 has a place or direction that is not a number"), std::string::npos) << report;
    EXPECT_NE(report.find("3 (Empty) has no nodes"), std::string::npos) << report;
}

TEST_F(ZonePathDatabaseTest, AReloadServesAMovedNodeAndABrokenRowKeepsTheOldPaths)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(sZonePathMgr.Load(errors)) << errors.front();
    EXPECT_EQ(ZonePathMgr::CountRows(), std::optional<uint64>(1));
    ZonePath const* const loaded = sZonePathMgr.Get()->Find(Hub, 8021750);
    ASSERT_NE(loaded, nullptr);
    ASSERT_EQ(loaded->Nodes.size(), 2u);
    EXPECT_EQ(loaded->Nodes[1].Position, (PropertyTypes::Vector3D{ 30.0f, 40.0f, 0.0f }));

    Execute(fmt::format("UPDATE `zone_path_node` SET `position_x` = 300 WHERE `zone_path` = '{}' AND `position` = 1", Hub));
    uint64 const before = sZonePathMgr.GetGeneration();
    ReloadOutcome const moved = sReloadMgr.Reload(ZonePathMgr::ReloadTarget);
    ASSERT_TRUE(moved.Ok) << (moved.Errors.empty() ? std::string() : moved.Errors.front());
    EXPECT_GT(sZonePathMgr.GetGeneration(), before);
    EXPECT_EQ(sZonePathMgr.Get()->Find(Hub, 8021750)->Nodes[1].Position.X, 300.0f) << "the moved node is served without a restart";

    uint64 const served = sZonePathMgr.GetGeneration();
    Execute(fmt::format("INSERT INTO `zone_path` (`zone_path`, `path_id`, `name`) VALUES ('{}', 8021751, 'Path_Flax_02')", Hub));
    ReloadOutcome const broken = sReloadMgr.Reload(ZonePathMgr::ReloadTarget);
    EXPECT_FALSE(broken.Ok);
    std::string const report = fmt::format("{}", fmt::join(broken.Errors, "\n"));
    EXPECT_NE(report.find("8021751 (Path_Flax_02) has no nodes"), std::string::npos) << report;
    EXPECT_EQ(sZonePathMgr.GetGeneration(), served) << "the old paths keep serving";
    EXPECT_EQ(sZonePathMgr.Get()->Find(Hub, 8021751), nullptr);
}
