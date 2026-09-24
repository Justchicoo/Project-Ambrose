/*
 * Project Ambrose by Imjustchico
 * Verifies the service-5 GAME message ordinals against the user's own message XML catalog.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "MessageRegistry.h"

#include <gtest/gtest.h>

#include <array>

TEST(GameOrdinalClientTest, ServiceFiveOrdinalsMatchTheClientCatalog)
{
    std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    if (!directory || directory->empty())
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install folder to run client data tests";

    MessageRegistry registry;
    ASSERT_TRUE(registry.LoadFromClient(LogConfig::Utf8Path(*directory))) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front().ToString());
    for (auto const& [tag, expected] : std::array<std::pair<std::string_view, uint8>, 8>{
             { { "MSG_ADDEFFECT", 2 }, { "MSG_ATTACH", 7 }, { "MSG_CLIENTMOVE", 36 }, { "MSG_ENTERSTATE", 72 },
                 { "MSG_LOGINCOMPLETE", 108 }, { "MSG_NEWOBJECT", 122 }, { "MSG_SERVERMOVE", 218 }, { "MSG_WIZBANG", 247 } } })
    {
        MessageInfoPtr const info = registry.Find(5, tag);
        ASSERT_NE(info, nullptr) << tag;
        EXPECT_EQ(info->Definition->Order, expected) << tag;
    }
}
