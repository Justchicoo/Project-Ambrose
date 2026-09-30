/*
 * Project Ambrose by Imjustchico
 * Tests the packet log: an authentication is named by its protocol, tag, service and order with every field value redacted, an ordinary message is written with its fields, a message the definitions do not hold is written by service and order, a filter keeps only what it names and a suppressed message is left out whatever the filter says, and a change to the live settings applies from the next message with no restart.
 */

#include "ConfigMgr.h"
#include "DynamicMessage.h"
#include "LogTestDirectory.h"
#include "LoginMessageFixtures.h"
#include "MemorySettingStore.h"
#include "MessageRegistry.h"
#include "PacketLog.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace
{
    class PacketLogTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sMessageRegistry.Clear();
            MessageDefinitionSet definitions;
            ASSERT_TRUE(LoginMessageFixtures::AddTo(definitions, true));
            ASSERT_TRUE(sMessageRegistry.Load(std::move(definitions)));
        }

        void TearDown() override
        {
            sMessageRegistry.Clear();
            sSettings.Clear();
        }

        std::vector<uint8> Encode(uint8 serviceId, uint8 order, std::string_view field, std::string const& value)
        {
            std::optional<DynamicMessage> message = DynamicMessage::Create(sMessageRegistry.GetCatalog(), serviceId, order);
            EXPECT_TRUE(message);
            if (!message)
                return {};
            EXPECT_TRUE(message->Set(field, DmlValue(value)));
            ByteBuffer body;
            message->Encode(body);
            std::span<uint8 const> const data = body.GetData();
            return { data.begin(), data.begin() + static_cast<std::ptrdiff_t>(std::min(body.GetSize(), data.size())) };
        }
    };
}

TEST_F(PacketLogTest, AnAuthenticationIsNamedByProtocolTagServiceAndOrderWithEveryFieldRedacted)
{
    std::vector<uint8> const body = Encode(7, 27, "Rec1", "hunter2-credential");
    std::string const line = PacketLog::Format(PacketLog::Direction::ClientToServer, 3, sMessageRegistry.GetCatalog(), 7, 27, body);
    EXPECT_TRUE(line.starts_with("session 3 C->S LOGIN MSG_USER_AUTHEN_V3 (7:27)")) << line;
    EXPECT_NE(line.find("Rec1=<redacted>"), std::string::npos) << line;
    EXPECT_EQ(line.find("hunter2"), std::string::npos) << "a credential never reaches the log: " << line;
    EXPECT_TRUE(PacketLog::IsRedacted("MSG_USER_AUTHEN_V3"));
    EXPECT_FALSE(PacketLog::IsRedacted("MSG_CLIENTMOVE"));
}

TEST_F(PacketLogTest, AMessageTheDefinitionsDoNotHoldIsWrittenByServiceAndOrder)
{
    std::vector<uint8> const body{ 1, 2, 3 };
    std::string const line = PacketLog::Format(PacketLog::Direction::ServerToClient, 9, sMessageRegistry.GetCatalog(), 99, 1, body);
    EXPECT_EQ(line, "session 9 S->C unknown message (99:1), 3 byte(s)");
}

TEST_F(PacketLogTest, AFilterKeepsWhatItNamesAndASuppressedMessageIsLeftOut)
{
    PacketLog::Options options;
    EXPECT_FALSE(options.Wants("MSG_ATTACH")) << "nothing is written while the log is off";
    options.Enabled = true;
    options.Suppress = PacketLog::Options::ParseNames("MSG_CLIENTMOVE, msg_servermove");
    EXPECT_EQ(options.Suppress, (std::vector<std::string>{ "MSG_CLIENTMOVE", "MSG_SERVERMOVE" }));
    EXPECT_TRUE(options.Wants("MSG_ATTACH"));
    EXPECT_FALSE(options.Wants("MSG_CLIENTMOVE"));
    EXPECT_FALSE(options.Wants("msg_servermove"));
    options.Filter = PacketLog::Options::ParseNames("MSG_ATTACH MSG_CLIENTMOVE");
    EXPECT_TRUE(options.Wants("MSG_ATTACH"));
    EXPECT_FALSE(options.Wants("MSG_PING")) << "a filter keeps only what it names";
    EXPECT_FALSE(options.Wants("MSG_CLIENTMOVE")) << "a suppressed message stays out even when a filter names it";
}

TEST_F(PacketLogTest, AChangedLiveSettingAppliesFromTheNextMessage)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("gameserver.conf", "Realm.Name = Test\n");
    ConfigMgr config([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    sSettings.Clear();
    std::vector<std::string> errors;
    ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
    std::vector<std::string> warnings;
    ASSERT_TRUE(sSettings.Start(config, std::make_shared<MemorySettingStore>(), warnings));

    EXPECT_FALSE(PacketLog::Options::FromSettings().Enabled) << "the log is off until an operator turns it on";
    SettingAuthor const author{ "test", 1, "unit_test" };
    ASSERT_TRUE(sSettings.Set("Network.PacketLog.Enable", "true", author, "turn the log on").Ok());
    PacketLog::Options const on = PacketLog::Options::FromSettings();
    EXPECT_TRUE(on.Enabled);
    EXPECT_FALSE(on.Wants("MSG_CLIENTMOVE")) << "moves are suppressed by default";
    EXPECT_TRUE(on.Wants("MSG_USER_AUTHEN_V3"));

    ASSERT_TRUE(sSettings.Set("Network.PacketLog.Filter", "MSG_CLIENTMOVE", author, "watch moves").Ok());
    ASSERT_TRUE(sSettings.Set("Network.PacketLog.Suppress", "MSG_SERVERMOVE", author, "keep client moves").Ok());
    PacketLog::Options const filtered = PacketLog::Options::FromSettings();
    EXPECT_TRUE(filtered.Wants("MSG_CLIENTMOVE"));
    EXPECT_FALSE(filtered.Wants("MSG_USER_AUTHEN_V3"));
}
