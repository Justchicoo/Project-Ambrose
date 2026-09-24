/*
 * Project Ambrose by Imjustchico
 * Runs the message round-trip suite over every id in the user's own client install (r806919) and prints a real message dump.
 */

#include "Environment.h"
#include "LogConfig.h"
#include "MessageRoundTrip.h"

#include <gtest/gtest.h>

TEST(MessageRoundTripClientTest, EveryClientMessageRoundTrips)
{
    std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    if (!directory || directory->empty())
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install folder to run client data tests";
    MessageRegistry registry;
    ASSERT_TRUE(registry.LoadFromClient(LogConfig::Utf8Path(*directory)));
    MessageCatalogPtr const catalog = registry.GetCatalog();
    ASSERT_NE(catalog, nullptr);
    ASSERT_EQ(catalog->GetMessages().size(), 1446u);

    std::size_t checked = 0;
    std::size_t withStrings = 0;
    for (MessageInfo const& info : catalog->GetMessages())
    {
        std::string const failure = MessageRoundTrip::Check(catalog, info, MessageRoundTrip::SeedFor(info));
        EXPECT_TRUE(failure.empty()) << failure;
        std::string const second = MessageRoundTrip::Check(catalog, info, ~MessageRoundTrip::SeedFor(info));
        EXPECT_TRUE(second.empty()) << second;
        ++checked;
        withStrings += DynamicMessage(catalog, info).GetEncodedSize() != info.MinSize ? 1 : 0;
    }
    EXPECT_EQ(checked, 1446u);
    EXPECT_EQ(withStrings, 0u);

    MessageInfo const* const crowns = catalog->Find(12, "MSG_CROWNBALANCE");
    ASSERT_NE(crowns, nullptr);
    DynamicMessage message(catalog, *crowns);
    ASSERT_TRUE(message.Set("TotalCrowns", DmlValue(int32(1250))));
    ASSERT_TRUE(message.Set("CharacterID", DmlValue(uint64(0x1122334455667788ull))));
    EXPECT_EQ(message.ToString(), "MSG_CROWNBALANCE (12:" + std::to_string(crowns->Definition->Order) + ") { Failure=0, TotalCrowns=1250, CharacterID=0x1122334455667788, CacheBalanceForCSSegmentation=0 }");
    EXPECT_EQ(crowns->MinSize, 14u);
}
