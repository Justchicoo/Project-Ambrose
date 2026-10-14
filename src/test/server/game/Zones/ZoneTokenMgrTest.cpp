/*
 * Project Ambrose by Imjustchico
 * Tests that a token container reads into its zone's token definitions and is refused whole when one token does not read, that the manager loads every zone's tokens from zone_object and refuses a name a zone holds twice, keeping the set it had, and that each instance keeps its own states until its map is forgotten.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ReloadMgr.h"
#include "TypeRegistry.h"
#include "TypedView.h"
#include "ZoneTokenFixtures.h"
#include "ZoneTokenMgr.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr std::string_view Hub = "WizardCity/WC_Hub";

    std::string Hex(std::vector<uint8> const& bytes)
    {
        std::string text = "X'";
        for (uint8 const byte : bytes)
            text += fmt::format("{:02X}", byte);
        return text + "'";
    }

    class ZoneTokenMgrDatabaseTest : public testing::Test
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
            _info.Database = fmt::format("ambrose_tok_{:08x}", std::random_device()());
            UpdaterSettings updates;
            updates.AllowPending = true;
            ASSERT_TRUE(DBUpdater::Run(_info, "world", updates));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&_views);
            ASSERT_TRUE(sTypeRegistry.LoadFromText(ZoneTokenFixtures::Dump(), "tokens.json")) << sTypeRegistry.GetErrors().front();
            Insert(fmt::format("INSERT INTO `zone_template` (`zone_path`, `display_name_key`, `soft_limit`) VALUES ('{}', 'WizardCity_WC_Hub', 50)", Hub));
            sReloadMgr.Clear();
            sZoneTokenMgr.Clear();
            sZoneTokenMgr.RegisterReloadTargets();
        }

        void TearDown() override
        {
            sZoneTokenMgr.Clear();
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

        void Place(uint32 objectId, std::vector<uint8> const& container)
        {
            Insert(fmt::format("INSERT INTO `zone_object` (`zone_path`, `class_name`, `object_id`, `template_id`, `position_x`, `position_y`, `position_z`, `orientation_x`, `orientation_y`, "
                "`orientation_z`, `scale`, `zone_tag`, `start_state`, `loading_type`, `token_container`) VALUES ('{}', 'class ZoneTokenInfo', {}, 1567019, 1, 2, 3, 0, 0, 0, 1, '', '', 3, {})",
                Hub, objectId, Hex(container)));
        }

        MySQLConnectionInfo _info;
        bool _open = false;
        TypedViewRegistry _views;
    };
}

TEST(ZoneTokenMgrTest, AContainerReadsIntoItsTokensAndIsRefusedWholeWhenOneDoesNotRead)
{
    std::string error;
    std::vector<ZoneTokenDefinition> tokens = ZoneTokenMgr::Read(ZoneTokenFixtures::Catalog(),
        ZoneTokenFixtures::Encode(ZoneTokenFixtures::Container(ZoneTokenFixtures::Boolean("GateOpen", ZoneTokenFixtures::ZoneStorage), ZoneTokenFixtures::Count("PageCount", 0, 0, 7)).get()), 9, error);
    EXPECT_TRUE(error.empty()) << error;
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].Name, "GateOpen");
    EXPECT_EQ(tokens[0].Storage, ZoneTokenStorage::Zone);
    EXPECT_EQ(tokens[0].ObjectId, 9u);
    EXPECT_EQ(tokens[1].Name, "PageCount");
    EXPECT_EQ(tokens[1].MaxValue, 7);

    tokens = ZoneTokenMgr::Read(ZoneTokenFixtures::Catalog(), ZoneTokenFixtures::Encode(ZoneTokenFixtures::Container(ZoneTokenFixtures::Boolean("GateOpen"), ZoneTokenFixtures::Boolean("")).get()), 9, error);
    EXPECT_TRUE(tokens.empty());
    EXPECT_NE(error.find("no m_debugName"), std::string::npos) << error;
    tokens = ZoneTokenMgr::Read(ZoneTokenFixtures::Catalog(), ZoneTokenFixtures::Encode(ZoneTokenFixtures::Boolean("Loose").get()), 9, error);
    EXPECT_TRUE(tokens.empty());
    EXPECT_NE(error.find("no m_tokens list"), std::string::npos) << error;
    tokens = ZoneTokenMgr::Read(ZoneTokenFixtures::Catalog(), std::vector<uint8>{ 1, 2, 3 }, 9, error);
    EXPECT_TRUE(tokens.empty());
    EXPECT_FALSE(error.empty());
}

TEST_F(ZoneTokenMgrDatabaseTest, TokensLoadByZoneAndATwiceNamedTokenKeepsTheOldSet)
{
    Place(9, ZoneTokenFixtures::Encode(ZoneTokenFixtures::Container(ZoneTokenFixtures::Boolean("GateOpen", ZoneTokenFixtures::ZoneStorage), ZoneTokenFixtures::Count("PageCount", 0, 0, 7)).get()));
    std::vector<std::string> errors;
    ASSERT_TRUE(sZoneTokenMgr.Load(errors)) << (errors.empty() ? std::string() : errors.front());
    std::shared_ptr<ZoneTokenSet const> const tokens = sZoneTokenMgr.Find(Hub);
    ASSERT_TRUE(tokens);
    ASSERT_EQ(tokens->Tokens.size(), 2u);
    EXPECT_EQ(tokens->Find("PageCount")->ObjectId, 9u);
    EXPECT_EQ(sZoneTokenMgr.Find("Krokotopia/KT_Hub"), nullptr);

    ZoneTokenDefinition const& gate = *tokens->Find("GateOpen");
    EXPECT_FALSE(sZoneTokenMgr.GetInstanceState(1, gate).Enabled);
    EXPECT_TRUE(sZoneTokenMgr.EnableInInstance(1, gate));
    EXPECT_TRUE(sZoneTokenMgr.ModifyInInstance(1, gate, 1));
    EXPECT_EQ(sZoneTokenMgr.GetInstanceState(1, gate), (ZoneTokenState{ true, true, 0 }));
    EXPECT_FALSE(sZoneTokenMgr.GetInstanceState(2, gate).Enabled) << "another instance keeps its own states";
    EXPECT_EQ(sZoneTokenMgr.GetInstanceChangeCount(1), 2u);
    EXPECT_TRUE(sZoneTokenMgr.DisableInInstance(1, gate));
    sZoneTokenMgr.ForgetMap(1);
    EXPECT_EQ(sZoneTokenMgr.GetInstanceChangeCount(1), 0u);
    EXPECT_FALSE(sZoneTokenMgr.GetInstanceState(1, gate).Set);

    Place(10, ZoneTokenFixtures::Encode(ZoneTokenFixtures::Container(ZoneTokenFixtures::Boolean("GateOpen")).get()));
    ReloadOutcome const reloaded = sReloadMgr.Reload(std::string(ZoneTokenMgr::ReloadTarget));
    EXPECT_FALSE(reloaded.Ok);
    ASSERT_EQ(reloaded.Errors.size(), 1u);
    EXPECT_NE(reloaded.Errors.front().find("holds the token GateOpen twice"), std::string::npos) << reloaded.Errors.front();
    EXPECT_EQ(sZoneTokenMgr.Find(Hub), tokens) << "a failed reload keeps the set it had";
}
