/*
 * Project Ambrose by Imjustchico
 * Tests the message registry against the user's own client install (r806919): lookups, declarations of real messages, and their encodings.
 */

#include "Environment.h"
#include "Hex.h"
#include "LogConfig.h"
#include "MessageRegistry.h"

#include <gtest/gtest.h>

#include <tuple>

namespace
{
    struct PingMessage
    {
        static constexpr uint8 ServiceId = 1;
        static constexpr std::string_view Tag = "MSG_PING";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct AttachMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_ATTACH";

        uint64 GameObjectId = 0;
        std::string LoginKey;
        std::string ZoneName;
        int32 Slot = 0;
        bool Reattach = false;
        std::u16string PlatformGamerTag;

        static constexpr auto Fields()
        {
            return std::tuple{ Field("GameObjectID", &AttachMessage::GameObjectId), Field("LoginKey", &AttachMessage::LoginKey), Field("ZoneName", &AttachMessage::ZoneName),
                Field("Slot", &AttachMessage::Slot), Field("Reattach", &AttachMessage::Reattach), Field("PlatformGamerTag", &AttachMessage::PlatformGamerTag) };
        }

        bool operator==(AttachMessage const&) const = default;
    };

    struct UserAuthenV3Message
    {
        static constexpr uint8 ServiceId = 7;
        static constexpr std::string_view Tag = "MSG_USER_AUTHEN_V3";

        std::string Rec1;
        std::string Version;
        uint64 MachineId = 0;
        uint32 IsSteamPatcher = 0;
        uint8 ConsoleType = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ Field("Rec1", &UserAuthenV3Message::Rec1), Field("Version", &UserAuthenV3Message::Version), Field("MachineID", &UserAuthenV3Message::MachineId),
                Field("IsSteamPatcher", &UserAuthenV3Message::IsSteamPatcher), Field("ConsoleType", &UserAuthenV3Message::ConsoleType) };
        }

        bool operator==(UserAuthenV3Message const&) const = default;
    };

    struct CrownBalanceMessage
    {
        static constexpr uint8 ServiceId = 12;
        static constexpr std::string_view Tag = "MSG_CROWNBALANCE";

        uint64 CharacterId = 0;

        static constexpr auto Fields() { return std::tuple{ Field("CharacterID", &CrownBalanceMessage::CharacterId) }; }
    };

    class MessageRegistryClientTest : public testing::Test
    {
    protected:
        static void SetUpTestSuite()
        {
            std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            if (!directory || directory->empty())
                return;
            _registry = std::make_unique<MessageRegistry>();
            _loaded = _registry->LoadFromClient(LogConfig::Utf8Path(*directory));
        }

        static void TearDownTestSuite()
        {
            _registry.reset();
        }

        void SetUp() override
        {
            std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            if (!directory || directory->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install folder to run client data tests";
            ASSERT_NE(_registry, nullptr);
            ASSERT_TRUE(_loaded) << (_registry->GetErrors().empty() ? std::string() : _registry->GetErrors().front().ToString());
        }

        static inline std::unique_ptr<MessageRegistry> _registry;
        static inline bool _loaded = false;
    };
}

TEST_F(MessageRegistryClientTest, LoadsEveryMessageAndFindsSpotChecks)
{
    EXPECT_EQ(_registry->GetMessageCount(), 1446u);
    EXPECT_EQ(_registry->GetWarnings().size(), 3u);
    EXPECT_TRUE(_registry->GetErrors().empty());

    MessageInfo const* const attach = _registry->Find(5, 7);
    ASSERT_NE(attach, nullptr);
    EXPECT_EQ(attach->Definition->Tag, "MSG_ATTACH");
    EXPECT_EQ(attach->Definition->AccessLevel, std::optional<uint8>(1));
    ASSERT_NE(_registry->Find(7, 27), nullptr);
    EXPECT_EQ(_registry->Find(7, 27)->Definition->Tag, "MSG_USER_AUTHEN_V3");
    ASSERT_NE(_registry->Find(12, 92), nullptr);
    EXPECT_EQ(_registry->Find(12, 92)->Definition->Tag, "MSG_MINIGAMEREWARDS");
    EXPECT_EQ(_registry->Find(5, 254), nullptr);
    EXPECT_EQ(_registry->Find(1, "MSG_PING"), _registry->Find(1, 1));

    std::size_t found = 0;
    for (uint32 service = 0; service < 256; ++service)
        for (uint32 order = 0; order < 256; ++order)
            found += _registry->Find(static_cast<uint8>(service), static_cast<uint8>(order)) != nullptr ? 1 : 0;
    EXPECT_EQ(found, 1446u);
}

TEST_F(MessageRegistryClientTest, RealMessagesDeclareEncodeAndDecode)
{
    std::vector<std::string> errors;
    ASSERT_TRUE((_registry->Declare<PingMessage, AttachMessage, UserAuthenV3Message, CrownBalanceMessage>(errors))) << (errors.empty() ? std::string() : errors.front());

    ByteBuffer ping;
    _registry->Encode(PingMessage{}, ping);
    EXPECT_EQ(ping.GetSize(), 0u);
    PingMessage decodedPing;
    EXPECT_EQ(_registry->Decode(ping.GetData(), decodedPing), MessageDecodeStatus::Ok);

    AttachMessage attach;
    attach.GameObjectId = 0x0102030405060708ull;
    attach.LoginKey = "key";
    attach.ZoneName = "WizardCity/WC_Ravenwood";
    attach.Slot = 2;
    attach.Reattach = true;
    attach.PlatformGamerTag = u"Wizard";
    ByteBuffer attachBuffer;
    _registry->Encode(attach, attachBuffer);
    std::size_t const attachSize = 8 + (2 + 3) + 8 + 8 + (2 + attach.ZoneName.size()) + 2 + 8 + 8 + 4 + 8 + 4 + 2 + 1 + 1 + 2 + 8 + (2 + 2 * attach.PlatformGamerTag.size());
    EXPECT_EQ(attachBuffer.GetSize(), attachSize);
    AttachMessage decodedAttach;
    EXPECT_EQ(_registry->Decode(attachBuffer.GetData(), decodedAttach), MessageDecodeStatus::Ok);
    EXPECT_EQ(decodedAttach, attach);

    UserAuthenV3Message authen;
    authen.Rec1 = std::string("\x01\x02\x00\x03", 4);
    authen.Version = "V_r806919.Wizard_1_610";
    authen.MachineId = 42;
    authen.IsSteamPatcher = 1;
    authen.ConsoleType = 3;
    ByteBuffer authenBuffer;
    _registry->Encode(authen, authenBuffer);
    EXPECT_EQ(authenBuffer.GetSize(), (2u + 4) + (2 + 22) + 2 + 2 + 2 + 8 + 2 + 2 + 4 + 1 + 2 + 2 + 2);
    UserAuthenV3Message decodedAuthen;
    EXPECT_EQ(_registry->Decode(authenBuffer.GetData(), decodedAuthen), MessageDecodeStatus::Ok);
    EXPECT_EQ(decodedAuthen, authen);

    CrownBalanceMessage crowns;
    crowns.CharacterId = 0x1122334455667788ull;
    ByteBuffer crownBuffer;
    _registry->Encode(crowns, crownBuffer);
    EXPECT_EQ(Hex::Encode(crownBuffer.GetData()), "00" "00000000" "8877665544332211" "00");
    CrownBalanceMessage decodedCrowns;
    EXPECT_EQ(_registry->Decode(crownBuffer.GetData(), decodedCrowns), MessageDecodeStatus::Ok);
    EXPECT_EQ(decodedCrowns.CharacterId, crowns.CharacterId);
}
