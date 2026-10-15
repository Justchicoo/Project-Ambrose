/*
 * Project Ambrose by Imjustchico
 * Holds the 12.22 same-connection zone transfer spike to its contract: Zone.TransferMode is declared, defaults to reconnect and is live, so a change applies to the next transfer without a restart; the game message table declares MSG_ZONETRANSFER and MSG_UPDATEZONECOUNTER as messages the server sends, with the fields the retail client declares for them.
 */

#include "ConfigMgr.h"
#include "Environment.h"
#include "GameMessageTable.h"
#include "GameMessages.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "MessageRegistry.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    class ZoneTransferModeTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sSettings.Clear();
            _configFile = _directory.Write("gameserver.conf", "Zone.TransferMode = reconnect\n");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_configFile).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
        }

        void TearDown() override
        {
            sSettings.Clear();
        }

        LogTestDirectory _directory;
        std::string _configFile;
        std::unique_ptr<ConfigMgr> _config;
    };
}

TEST_F(ZoneTransferModeTest, TheModeDefaultsToReconnectAndIsLive)
{
    EXPECT_EQ(sSettings.Get<std::string>("Zone.TransferMode"), "reconnect");
    ASSERT_TRUE(sSettings.Set("Zone.TransferMode", "inprocess", { "test", 1, "unit_test" }, "spike the inprocess path").Ok());
    EXPECT_EQ(sSettings.Get<std::string>("Zone.TransferMode"), "inprocess") << "a live change applies to the next transfer without a restart";
    ASSERT_TRUE(sSettings.Set("Zone.TransferMode", "reconnect", { "test", 1, "unit_test" }, "back to the proven path").Ok());
    EXPECT_EQ(sSettings.Get<std::string>("Zone.TransferMode"), "reconnect");
}

TEST_F(ZoneTransferModeTest, TheSpikesMessagesAreDeclaredAsSent)
{
    MessageRegistry registry;
    std::vector<std::string> errors;
    ASSERT_TRUE(GameMessageTable::Get().Declare(registry, errors)) << (errors.empty() ? "" : errors.front());

    EXPECT_EQ(GameMessages::ZoneTransfer::Tag, "MSG_ZONETRANSFER");
    EXPECT_EQ(GameMessages::ZoneTransfer::ServiceId, GameMessages::GameService);
    EXPECT_EQ(std::tuple_size_v<decltype(GameMessages::ZoneTransfer::Fields())>, 9u)
        << "ZoneName, ZoneID, Slot, DynamicZoneID, DynamicServerProcID, ZoneCounter, TransitionID, SessionID, SessionSlot";

    EXPECT_EQ(GameMessages::UpdateZoneCounter::Tag, "MSG_UPDATEZONECOUNTER");
    EXPECT_EQ(GameMessages::UpdateZoneCounter::ServiceId, GameMessages::GameService);
    EXPECT_EQ(std::tuple_size_v<decltype(GameMessages::UpdateZoneCounter::Fields())>, 1u) << "ZoneCounter";
}
