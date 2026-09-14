/*
 * Project Ambrose by Imjustchico
 * Tests the message registry on Ambrose-authored fixtures: loading, lookups, defaults, declaration checks, and golden encodings of declared messages.
 */

#include "Hex.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"
#include "MessageRegistry.h"

#include <gtest/gtest.h>

#include <atomic>
#include <fstream>
#include <memory>
#include <thread>
#include <tuple>

namespace
{
    constexpr std::string_view GameXml = R"(<?xml version="1.0" ?>
<GameFixtureMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">5</ServiceID><ProtocolType TYPE="STR">GAME</ProtocolType><ProtocolVersion TYPE="INT">1</ProtocolVersion><ProtocolDescription TYPE="STR">Game fixture</ProtocolDescription></RECORD></_ProtocolInfo>
<MSG_JOIN><RECORD><_MsgName TYPE="STR" NOXFER="TRUE">MSG_JOIN</_MsgName><_MsgAccessLvl TYPE="UBYT" NOXFER="TRUE">1</_MsgAccessLvl><ObjectID TYPE="GID"></ObjectID><Zone TYPE="STR"></Zone><Slot TYPE="INT">7</Slot><Retry TYPE="UBYT"></Retry><Title TYPE="WSTR"></Title><Speed TYPE="FLT"></Speed><Delta TYPE="BYT"></Delta><Port TYPE="USHRT"></Port><Offset TYPE="SHRT"></Offset><Scale TYPE="DBL"></Scale><Count TYPE="UINT"></Count></RECORD></MSG_JOIN>
<MSG_ALIVE><RECORD></RECORD></MSG_ALIVE>
<MSG_BROKEN_DEFAULT><RECORD><Level TYPE="UBYT">300</Level><Name TYPE="STR">guest</Name></RECORD></MSG_BROKEN_DEFAULT>
</GameFixtureMessages>
)";

    constexpr std::string_view LoginXml = R"(<?xml version="1.0" ?>
<LoginFixtureMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">7</ServiceID><ProtocolType TYPE="STR">LOGIN</ProtocolType></RECORD></_ProtocolInfo>
<MSG_HELLO><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">27</_MsgOrder><Version TYPE="STR"></Version><MachineID TYPE="GID"></MachineID></RECORD></MSG_HELLO>
</LoginFixtureMessages>
)";

    enum class RetryMode : uint8
    {
        None,
        Once
    };

    struct JoinMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        uint64 ObjectId = 0;
        std::string Zone;
        std::u16string Title;
        RetryMode Retry = RetryMode::None;

        static constexpr auto Fields()
        {
            return std::tuple{ Field("ObjectID", &JoinMessage::ObjectId), Field("Zone", &JoinMessage::Zone), Field("Title", &JoinMessage::Title), Field("Retry", &JoinMessage::Retry) };
        }

        bool operator==(JoinMessage const&) const = default;
    };

    struct FullJoinMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        uint64 ObjectId = 0;
        std::string Zone;
        int32 Slot = 0;
        bool Retry = false;
        std::u16string Title;
        float Speed = 0.0f;
        int8 Delta = 0;
        uint16 Port = 0;
        int16 Offset = 0;
        double Scale = 0.0;
        uint32 Count = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ Field("Count", &FullJoinMessage::Count), Field("ObjectID", &FullJoinMessage::ObjectId), Field("Zone", &FullJoinMessage::Zone), Field("Slot", &FullJoinMessage::Slot),
                Field("Retry", &FullJoinMessage::Retry), Field("Title", &FullJoinMessage::Title), Field("Speed", &FullJoinMessage::Speed), Field("Delta", &FullJoinMessage::Delta),
                Field("Port", &FullJoinMessage::Port), Field("Offset", &FullJoinMessage::Offset), Field("Scale", &FullJoinMessage::Scale) };
        }

        bool operator==(FullJoinMessage const&) const = default;
    };

    struct TitleOnlyMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        std::u16string Title;
        uint32 Count = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ Field("Title", &TitleOnlyMessage::Title), Field("Count", &TitleOnlyMessage::Count) };
        }
    };

    struct AliveMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_ALIVE";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct HelloMessage
    {
        static constexpr uint8 ServiceId = 7;
        static constexpr std::string_view Tag = "MSG_HELLO";

        std::string Version;

        static constexpr auto Fields() { return std::tuple{ Field("Version", &HelloMessage::Version) }; }
    };

    struct MissingMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_MISSING";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct WrongServiceMessage
    {
        static constexpr uint8 ServiceId = 7;
        static constexpr std::string_view Tag = "MSG_JOIN";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct MisspelledFieldMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        std::string Zone;

        static constexpr auto Fields() { return std::tuple{ Field("Zoen", &MisspelledFieldMessage::Zone) }; }
    };

    struct WrongTypeMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        uint32 Slot = 0;
        int32 Count = 0;

        static constexpr auto Fields() { return std::tuple{ Field("Slot", &WrongTypeMessage::Slot), Field("Count", &WrongTypeMessage::Count) }; }
    };

    struct TwiceMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        std::string First;
        std::string Second;

        static constexpr auto Fields() { return std::tuple{ Field("Zone", &TwiceMessage::First), Field("Zone", &TwiceMessage::Second) }; }
    };

    struct AfterTitleMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        float Speed = 0.0f;
        uint32 Count = 0;

        static constexpr auto Fields() { return std::tuple{ Field("Speed", &AfterTitleMessage::Speed), Field("Count", &AfterTitleMessage::Count) }; }
    };

    struct JoinHeader
    {
        uint64 ObjectId = 0;
    };

    struct InheritedJoinMessage : JoinHeader
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        std::string Zone;

        static constexpr auto Fields() { return std::tuple{ Field("ObjectID", &InheritedJoinMessage::ObjectId), Field("Zone", &InheritedJoinMessage::Zone) }; }
    };

    struct SpelledIntegersMessage
    {
        static constexpr uint8 ServiceId = 5;
        static constexpr std::string_view Tag = "MSG_JOIN";

        unsigned long long ObjectId = 0;
        int Slot = 0;
        unsigned short Port = 0;
        short Offset = 0;
        signed char Delta = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ Field("ObjectID", &SpelledIntegersMessage::ObjectId), Field("Slot", &SpelledIntegersMessage::Slot), Field("Port", &SpelledIntegersMessage::Port),
                Field("Offset", &SpelledIntegersMessage::Offset), Field("Delta", &SpelledIntegersMessage::Delta) };
        }
    };

    enum UnfixedMode
    {
        UnfixedFirst,
        UnfixedSecond
    };

    enum FixedMode : int16
    {
        FixedFirst = -1,
        FixedSecond
    };

    struct UnrelatedHolder
    {
        std::string Zone;
    };

    static_assert(MessageMember::IsSupported<RetryMode>());
    static_assert(MessageMember::IsSupported<FixedMode>());
    static_assert(!MessageMember::IsSupported<UnfixedMode>());
    static_assert(MessageMember::IsSupported<bool>());
    static_assert(!MessageMember::IsSupported<bool const>());
    static_assert(!MessageMember::IsSupported<int32 const>());
    static_assert(!MessageMember::IsSupported<char>());
    static_assert(!MessageMember::IsSupported<char16_t>());
    static_assert(!MessageMember::IsSupported<long double>());
    static_assert(!MessageMember::IsSupported<std::string_view>());
    static_assert(MessageMember::IsCompatible<unsigned long long>(DmlType::Gid));
    static_assert(!MessageMember::IsCompatible<int64>(DmlType::Gid));
    static_assert(MessageMember::IsCompatible<FixedMode>(DmlType::Shrt));
    static_assert(!MessageMember::IsCompatible<FixedMode>(DmlType::Ushrt));
    static_assert(MessageMember::IsCompatible<bool>(DmlType::Ubyt));
    static_assert(!MessageMember::IsCompatible<float>(DmlType::Dbl));
    static_assert(IsMessageFieldTuple<InheritedJoinMessage, decltype(InheritedJoinMessage::Fields())>::value);
    static_assert(IsMessageFieldTuple<AliveMessage, std::tuple<>>::value);
    static_assert(!IsMessageFieldTuple<JoinMessage, std::tuple<MessageField<UnrelatedHolder, std::string>>>::value);
    static_assert(!IsMessageFieldTuple<JoinMessage, std::tuple<int>>::value);

    MessageDefinitionSet FixtureDefinitions()
    {
        MessageDefinitionSet set;
        EXPECT_TRUE(set.Add(GameXml, "GameFixtureMessages.xml"));
        EXPECT_TRUE(set.Add(LoginXml, "LoginFixtureMessages.xml"));
        return set;
    }

    std::string Encoded(ByteBuffer const& buffer)
    {
        return Hex::Encode(buffer.GetData());
    }

    std::vector<uint8> Bytes(std::string_view hex)
    {
        std::optional<std::vector<uint8>> bytes = Hex::Decode(hex);
        EXPECT_TRUE(bytes.has_value()) << hex;
        return bytes.value_or(std::vector<uint8>());
    }

    constexpr std::string_view JoinGolden = "8877665544332211" "0300487562" "07000000" "01" "020048006900" "00000000" "00" "0000" "0000" "0000000000000000" "00000000";

    void WriteFile(std::filesystem::path const& path, std::vector<uint8> const& bytes)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
}

TEST(MessageRegistryTest, LoadsDefinitionsAndFindsMessages)
{
    MessageRegistry registry;
    EXPECT_FALSE(registry.IsLoaded());
    EXPECT_EQ(registry.Find(5, 1), nullptr);
    EXPECT_EQ(registry.Find(5, "MSG_ALIVE"), nullptr);

    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    EXPECT_TRUE(registry.IsLoaded());
    EXPECT_TRUE(registry.GetErrors().empty());
    EXPECT_EQ(registry.GetMessageCount(), 4u);
    ASSERT_NE(registry.GetCatalog(), nullptr);
    EXPECT_EQ(registry.GetCatalog()->GetDefinitions().GetProtocols().size(), 2u);

    MessageInfoPtr const join = registry.Find(5, 3);
    ASSERT_NE(join, nullptr);
    EXPECT_EQ(join->Definition->Tag, "MSG_JOIN");
    EXPECT_EQ(join->Definition->AccessLevel, std::optional<uint8>(1));
    EXPECT_EQ(join->Protocol->ServiceId, 5);
    ASSERT_EQ(join->Defaults.size(), 11u);
    EXPECT_EQ(join->Defaults[2], DmlValue(int32(7)));
    EXPECT_EQ(join->Defaults[4], DmlValue(std::u16string()));
    EXPECT_EQ(registry.Find(5, "MSG_ALIVE"), registry.Find(5, 1));
    EXPECT_EQ(registry.Find(7, 27)->Definition->Tag, "MSG_HELLO");
    EXPECT_EQ(registry.Find(5, 4), nullptr);
    EXPECT_EQ(registry.Find(6, 1), nullptr);
    EXPECT_EQ(registry.Find(7, "MSG_JOIN"), nullptr);
    EXPECT_EQ(registry.Find(255, 255), nullptr);
    EXPECT_EQ(&MessageRegistry::Instance(), &sMessageRegistry);
}

TEST(MessageRegistryTest, InvalidDefaultsWarnAndUseZero)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    ASSERT_EQ(registry.GetWarnings().size(), 1u);
    EXPECT_NE(registry.GetWarnings().front().Message.find("MSG_BROKEN_DEFAULT.Level has default '300', which is not a valid UBYT"), std::string::npos) << registry.GetWarnings().front().ToString();
    MessageInfoPtr const broken = registry.Find(5, "MSG_BROKEN_DEFAULT");
    ASSERT_NE(broken, nullptr);
    EXPECT_EQ(broken->Defaults[0], DmlValue(uint8(0)));
    EXPECT_EQ(broken->Defaults[1], DmlValue(std::string("guest")));
}

TEST(MessageRegistryTest, FailedReloadKeepsTheActiveCatalog)
{
    MessageRegistry registry;
    EXPECT_FALSE(registry.Load(MessageDefinitionSet()));
    EXPECT_FALSE(registry.IsLoaded());
    ASSERT_EQ(registry.GetErrors().size(), 1u);
    EXPECT_EQ(registry.GetErrors().front().Message, "no message protocols were loaded");

    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    EXPECT_TRUE(registry.GetErrors().empty());
    EXPECT_EQ(registry.GetGeneration(), 1u);
    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<AliveMessage>(errors));
    MessageCatalogPtr const active = registry.GetCatalog();

    MessageDefinitionSet broken;
    EXPECT_FALSE(broken.Add("<Broken>", "BrokenMessages.xml"));
    EXPECT_FALSE(registry.Load(std::move(broken)));
    EXPECT_FALSE(registry.GetErrors().empty());
    EXPECT_EQ(registry.GetCatalog(), active);
    EXPECT_EQ(registry.GetGeneration(), 1u);
    EXPECT_NE(registry.Find(5, 1), nullptr);
    EXPECT_TRUE(registry.IsDeclared<AliveMessage>());

    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    EXPECT_TRUE(registry.GetErrors().empty());
    EXPECT_EQ(registry.GetGeneration(), 2u);
    EXPECT_NE(registry.GetCatalog(), active);
    EXPECT_TRUE(registry.IsDeclared<AliveMessage>());
    EXPECT_EQ(active->GetGeneration(), 1u);
    EXPECT_NE(active->Find(5, 1), nullptr);

    MessageInfoPtr const pinned = registry.Find(5, "MSG_JOIN");
    ASSERT_NE(pinned, nullptr);
    std::vector<std::string> joinErrors;
    ASSERT_TRUE(registry.Declare<JoinMessage>(joinErrors));
    MessageInfoPtr const declared = registry.GetInfo<JoinMessage>();
    registry.Clear();
    EXPECT_EQ(pinned->Definition->Tag, "MSG_JOIN");
    EXPECT_EQ(declared->Definition->Fields.size(), 11u);
    EXPECT_FALSE(registry.IsLoaded());
    EXPECT_EQ(registry.GetGeneration(), 0u);
    EXPECT_EQ(registry.Find(5, 1), nullptr);
    EXPECT_EQ(registry.GetMessageCount(), 0u);
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    EXPECT_FALSE(registry.IsDeclared<AliveMessage>());
}

TEST(MessageRegistryTest, ReloadThatBreaksADeclarationIsRejected)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<JoinMessage>(errors));
    MessageCatalogPtr const active = registry.GetCatalog();

    std::string withoutZone(GameXml);
    std::string const zone = "<Zone TYPE=\"STR\"></Zone>";
    withoutZone.erase(withoutZone.find(zone), zone.size());
    MessageDefinitionSet changed;
    ASSERT_TRUE(changed.Add(withoutZone, "GameFixtureMessages.xml"));
    ASSERT_TRUE(changed.Add(LoginXml, "LoginFixtureMessages.xml"));
    EXPECT_FALSE(registry.Load(std::move(changed)));
    ASSERT_EQ(registry.GetErrors().size(), 1u);
    EXPECT_EQ(registry.GetErrors().front().ToString(), "message declarations: MSG_JOIN (service 5) has no field Zone");
    EXPECT_EQ(registry.GetCatalog(), active);
    ByteBuffer buffer;
    registry.Encode(JoinMessage{}, buffer);
    EXPECT_EQ(buffer.GetSize(), 38u);

    std::string extended(GameXml);
    std::string const count = "<Count TYPE=\"UINT\"></Count>";
    extended.insert(extended.find(count) + count.size(), "<Extra TYPE=\"UBYT\">9</Extra>");
    MessageDefinitionSet grown;
    ASSERT_TRUE(grown.Add(extended, "GameFixtureMessages.xml"));
    ASSERT_TRUE(grown.Add(LoginXml, "LoginFixtureMessages.xml"));
    ASSERT_TRUE(registry.Load(std::move(grown)));
    ByteBuffer grownBuffer;
    registry.Encode(JoinMessage{}, grownBuffer);
    EXPECT_EQ(grownBuffer.GetSize(), 39u);
    EXPECT_EQ(grownBuffer.GetData().back(), 9);
    ByteBuffer oldBuffer;
    active->Encode(JoinMessage{}, oldBuffer);
    EXPECT_EQ(oldBuffer.GetSize(), 38u);

    MessageRegistry pending;
    EXPECT_TRUE(pending.Declare<MissingMessage>(errors));
    EXPECT_FALSE(pending.Load(FixtureDefinitions()));
    EXPECT_FALSE(pending.IsLoaded());
    ASSERT_EQ(pending.GetErrors().size(), 1u);
    EXPECT_EQ(pending.GetErrors().front().ToString(), "message declarations: MSG_MISSING is not a message of service 5");
}

TEST(MessageRegistryTest, ReaderThreadsSeeWholeCatalogsDuringReloads)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<JoinMessage>(errors));

    std::atomic<bool> stop{ false };
    std::atomic<bool> failed{ false };
    std::atomic<uint64> encodes{ 0 };
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i)
    {
        readers.emplace_back([&]
        {
            JoinMessage message;
            message.Zone = "Hub";
            while (!stop.load())
            {
                MessageCatalogPtr const catalog = registry.GetCatalog();
                ByteBuffer buffer;
                catalog->Encode(message, buffer);
                JoinMessage decoded;
                if (catalog->Decode(buffer.GetData(), decoded) != MessageDecodeStatus::Ok || decoded.Zone != "Hub" || registry.Find(5, 3) == nullptr)
                    failed = true;
                ++encodes;
            }
        });
    }
    int reloaded = 0;
    for (int i = 0; i < 50; ++i)
        reloaded += registry.Load(FixtureDefinitions()) ? 1 : 0;
    while (encodes.load() == 0)
        std::this_thread::yield();
    stop = true;
    for (std::thread& reader : readers)
        reader.join();
    EXPECT_EQ(reloaded, 50);
    EXPECT_FALSE(failed.load());
    EXPECT_EQ(registry.GetGeneration(), 51u);
    EXPECT_TRUE(registry.IsDeclared<JoinMessage>());
}

TEST(MessageRegistryTest, DeclaredSubsetEncodesWithDefaultsForOmittedFields)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<JoinMessage>(errors)) << errors.front();
    EXPECT_TRUE(errors.empty());
    EXPECT_TRUE(registry.IsDeclared<JoinMessage>());
    EXPECT_EQ(registry.GetInfo<JoinMessage>()->Definition->Order, 3);

    JoinMessage message;
    message.ObjectId = 0x1122334455667788ull;
    message.Zone = "Hub";
    message.Title = u"Hi";
    message.Retry = RetryMode::Once;
    ByteBuffer buffer;
    registry.Encode(message, buffer);
    EXPECT_EQ(Encoded(buffer), JoinGolden);

    JoinMessage decoded;
    EXPECT_EQ(registry.Decode(buffer.GetData(), decoded), MessageDecodeStatus::Ok);
    EXPECT_EQ(decoded, message);
}

TEST(MessageRegistryTest, EveryDmlTypeRoundTripsThroughAFullDeclaration)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<FullJoinMessage>(errors)) << errors.front();

    FullJoinMessage message;
    message.ObjectId = 0x0102030405060708ull;
    message.Zone = "";
    message.Slot = -1;
    message.Retry = true;
    message.Title = u"é";
    message.Speed = 1.5f;
    message.Delta = -2;
    message.Port = 0xABCD;
    message.Offset = -3;
    message.Scale = 0.25;
    message.Count = 0xDEADBEEF;
    ByteBuffer buffer;
    registry.Encode(message, buffer);
    EXPECT_EQ(Encoded(buffer), "0807060504030201" "0000" "ffffffff" "01" "0100e900" "0000c03f" "fe" "cdab" "fdff" "000000000000d03f" "efbeadde");

    FullJoinMessage decoded;
    EXPECT_EQ(registry.Decode(buffer.GetData(), decoded), MessageDecodeStatus::Ok);
    EXPECT_EQ(decoded, message);

    std::vector<uint8> retryTwo = Bytes("0807060504030201" "0000" "ffffffff" "02" "0100e900" "0000c03f" "fe" "cdab" "fdff" "000000000000d03f" "efbeadde");
    EXPECT_EQ(registry.Decode(retryTwo, decoded), MessageDecodeStatus::Ok);
    EXPECT_TRUE(decoded.Retry);
}

TEST(MessageRegistryTest, DecodeSkipsUndeclaredFieldsAndFlagsSizeProblems)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<TitleOnlyMessage>(errors)) << errors.front();

    std::vector<uint8> const golden = Bytes(JoinGolden);
    TitleOnlyMessage titleOnly;
    ASSERT_EQ(registry.Decode(golden, titleOnly), MessageDecodeStatus::Ok);
    EXPECT_EQ(titleOnly.Title, u"Hi");
    EXPECT_EQ(titleOnly.Count, 0u);

    std::vector<uint8> truncated(golden.begin(), golden.end() - 1);
    titleOnly.Title = u"unchanged";
    EXPECT_EQ(registry.Decode(truncated, titleOnly), MessageDecodeStatus::Truncated);
    EXPECT_EQ(titleOnly.Title, u"unchanged");

    std::vector<uint8> trailing = golden;
    trailing.push_back(0x99);
    EXPECT_EQ(registry.Decode(trailing, titleOnly), MessageDecodeStatus::TrailingBytes);
    EXPECT_EQ(titleOnly.Title, u"Hi");

    std::vector<uint8> hugeString = Bytes("8877665544332211" "ffff4875");
    EXPECT_EQ(registry.Decode(hugeString, titleOnly), MessageDecodeStatus::Truncated);
    EXPECT_EQ(registry.Decode(std::span<uint8 const>(), titleOnly), MessageDecodeStatus::Truncated);

    std::vector<uint8> framed{ 0xAA, 0xBB };
    framed.insert(framed.end(), golden.begin(), golden.end());
    framed.push_back(0xCC);
    ByteBuffer buffer(framed);
    buffer.SetReadPosition(2);
    TitleOnlyMessage fromBuffer;
    ASSERT_TRUE(registry.Decode(buffer, fromBuffer));
    EXPECT_EQ(fromBuffer.Title, u"Hi");
    EXPECT_EQ(buffer.GetRemaining(), 1u);

    ByteBuffer shortBuffer(truncated);
    EXPECT_FALSE(registry.Decode(shortBuffer, fromBuffer));
    EXPECT_EQ(shortBuffer.GetReadPosition(), 0u);
}

TEST(MessageRegistryTest, EmptyMessagesAndOtherServicesEncode)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE((registry.Declare<AliveMessage, HelloMessage>(errors)));

    ByteBuffer alive;
    registry.Encode(AliveMessage{}, alive);
    EXPECT_EQ(alive.GetSize(), 0u);
    AliveMessage decodedAlive;
    EXPECT_EQ(registry.Decode(std::span<uint8 const>(), decodedAlive), MessageDecodeStatus::Ok);

    HelloMessage hello;
    hello.Version = "1.0";
    ByteBuffer helloBuffer;
    registry.Encode(hello, helloBuffer);
    EXPECT_EQ(Encoded(helloBuffer), "0300312e30" "0000000000000000");
}

TEST(MessageRegistryTest, InvalidDeclarationsAreReported)
{
    MessageRegistry registry;
    std::vector<std::string> errors;
    EXPECT_TRUE(registry.Declare<AliveMessage>(errors));
    EXPECT_TRUE(errors.empty());
    EXPECT_FALSE(registry.IsDeclared<AliveMessage>());

    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    EXPECT_TRUE(registry.IsDeclared<AliveMessage>());
    errors.clear();
    EXPECT_FALSE(registry.Declare<MissingMessage>(errors));
    EXPECT_FALSE(registry.Declare<WrongServiceMessage>(errors));
    EXPECT_FALSE(registry.Declare<MisspelledFieldMessage>(errors));
    EXPECT_FALSE(registry.Declare<WrongTypeMessage>(errors));
    EXPECT_FALSE(registry.Declare<TwiceMessage>(errors));
    std::vector<std::string> const expected{
        "MSG_MISSING is not a message of service 5",
        "MSG_JOIN is not a message of service 7",
        "MSG_JOIN (service 5) has no field Zoen",
        "MSG_JOIN.Slot is INT in the client definition, which the declared C++ member type cannot hold exactly",
        "MSG_JOIN.Count is UINT in the client definition, which the declared C++ member type cannot hold exactly",
        "MSG_JOIN.Zone is declared by more than one member"
    };
    EXPECT_EQ(errors, expected);
    EXPECT_FALSE(registry.IsDeclared<WrongTypeMessage>());
    ByteBuffer unused;
    EXPECT_THROW(registry.Encode(WrongTypeMessage{}, unused), std::logic_error);

    errors.clear();
    EXPECT_FALSE((registry.Declare<AliveMessage, MissingMessage, HelloMessage>(errors)));
    EXPECT_EQ(errors.size(), 1u);
    EXPECT_TRUE(registry.IsDeclared<AliveMessage>());
    EXPECT_TRUE(registry.IsDeclared<HelloMessage>());
    EXPECT_FALSE(registry.IsDeclared<MissingMessage>());
}

TEST(MessageRegistryTest, UndeclaredUseThrowsAndReloadKeepsDeclarations)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    ByteBuffer buffer;
    JoinMessage message;
    EXPECT_THROW(registry.Encode(message, buffer), std::logic_error);
    EXPECT_THROW(registry.Decode(buffer, message), std::logic_error);
    EXPECT_THROW(registry.GetInfo<JoinMessage>(), std::logic_error);

    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<JoinMessage>(errors));
    registry.Encode(message, buffer);
    EXPECT_FALSE(buffer.GetData().empty());

    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    EXPECT_TRUE(registry.IsDeclared<JoinMessage>());
    registry.Encode(message, buffer);

    MessageRegistry unloaded;
    EXPECT_THROW(unloaded.Encode(message, buffer), std::logic_error);
    EXPECT_THROW(unloaded.GetInfo<JoinMessage>(), std::logic_error);
    EXPECT_FALSE(unloaded.IsDeclared<JoinMessage>());

    MessageRegistry other;
    ASSERT_TRUE(other.Load(FixtureDefinitions()));
    EXPECT_FALSE(other.IsDeclared<JoinMessage>());
    ASSERT_TRUE(registry.Declare<JoinMessage>(errors));
    EXPECT_FALSE(other.IsDeclared<JoinMessage>());
    EXPECT_TRUE(registry.IsDeclared<JoinMessage>());
}

TEST(MessageRegistryTest, LoadsFromAnArchiveAndAClientFolder)
{
    LogTestDirectory directory;
    std::vector<uint8> const bytes = KiwadBuilder(1).Add("GameFixtureMessages.xml", GameXml, true).Add("Messages/LoginFixtureMessages.xml", LoginXml, false).Build();
    std::filesystem::path const client = directory.Path() / "Client";
    WriteFile(MessageRegistry::GetClientArchivePath(client), bytes);

    MessageRegistry registry;
    ASSERT_TRUE(registry.LoadFromClient(client)) << (registry.GetErrors().empty() ? std::string() : registry.GetErrors().front().ToString());
    EXPECT_EQ(registry.GetMessageCount(), 4u);
    EXPECT_EQ(registry.Find(7, 27)->Protocol->SourceFile, "Messages/LoginFixtureMessages.xml");

    EXPECT_FALSE(registry.LoadFromClient(directory.Path() / "Missing"));
    EXPECT_TRUE(registry.IsLoaded());
    EXPECT_EQ(registry.GetMessageCount(), 4u);
    ASSERT_EQ(registry.GetErrors().size(), 1u);
    EXPECT_NE(registry.GetErrors().front().SourceFile.find("Root.wad"), std::string::npos);

    std::filesystem::path const noMessages = directory.Path() / "Empty.wad";
    WriteFile(noMessages, KiwadBuilder(1).Add("Readme.txt", "nothing", false).Build());
    EXPECT_FALSE(registry.LoadFromArchive(noMessages));
    EXPECT_FALSE(registry.GetErrors().empty());
}

TEST(MessageRegistryTest, DecodeSkipsUndeclaredWideStrings)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<AfterTitleMessage>(errors));

    std::vector<uint8> const full = Bytes("0807060504030201" "0000" "ffffffff" "01" "0200e9004100" "0000c03f" "fe" "cdab" "fdff" "000000000000d03f" "efbeadde");
    AfterTitleMessage message;
    ASSERT_EQ(registry.Decode(full, message), MessageDecodeStatus::Ok);
    EXPECT_EQ(message.Speed, 1.5f);
    EXPECT_EQ(message.Count, 0xDEADBEEFu);

    std::vector<uint8> const insideTitle = Bytes("0807060504030201" "0000" "ffffffff" "01" "0200e900");
    EXPECT_EQ(registry.Decode(insideTitle, message), MessageDecodeStatus::Truncated);
    EXPECT_EQ(message.Count, 0xDEADBEEFu);
}

TEST(MessageRegistryTest, InheritedAndDifferentlySpelledMembersDeclare)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE((registry.Declare<InheritedJoinMessage, SpelledIntegersMessage>(errors))) << (errors.empty() ? std::string() : errors.front());

    InheritedJoinMessage inherited;
    inherited.ObjectId = 0x1122334455667788ull;
    inherited.Zone = "Hub";
    ByteBuffer buffer;
    registry.Encode(inherited, buffer);
    EXPECT_EQ(Encoded(buffer), "8877665544332211" "0300487562" "07000000" "00" "0000" "00000000" "00" "0000" "0000" "0000000000000000" "00000000");
    InheritedJoinMessage decoded;
    ASSERT_EQ(registry.Decode(buffer.GetData(), decoded), MessageDecodeStatus::Ok);
    EXPECT_EQ(decoded.ObjectId, inherited.ObjectId);
    EXPECT_EQ(decoded.Zone, "Hub");

    SpelledIntegersMessage spelled;
    spelled.ObjectId = 1;
    spelled.Slot = -2;
    spelled.Port = 3;
    spelled.Offset = -4;
    spelled.Delta = -5;
    ByteBuffer spelledBuffer;
    registry.Encode(spelled, spelledBuffer);
    EXPECT_EQ(Encoded(spelledBuffer), "0100000000000000" "0000" "feffffff" "00" "0000" "00000000" "fb" "0300" "fcff" "0000000000000000" "00000000");
}

TEST(MessageRegistryTest, OversizedStringsThrowBeforeWritingAnything)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(FixtureDefinitions()));
    std::vector<std::string> errors;
    ASSERT_TRUE(registry.Declare<JoinMessage>(errors));

    JoinMessage message;
    message.Zone = std::string(Dml::MaxStringLength + 1, 'z');
    ByteBuffer buffer;
    EXPECT_THROW(registry.Encode(message, buffer), std::length_error);
    EXPECT_EQ(buffer.GetSize(), 0u);

    message.Zone.clear();
    message.Title = std::u16string(Dml::MaxStringLength + 1, u'z');
    EXPECT_THROW(registry.Encode(message, buffer), std::length_error);
    EXPECT_EQ(buffer.GetSize(), 0u);

    message.Title = std::u16string(Dml::MaxStringLength, u'z');
    registry.Encode(message, buffer);
    EXPECT_EQ(buffer.GetSize(), 8u + 2 + 4 + 1 + 2 + 2 * Dml::MaxStringLength + 4 + 1 + 2 + 2 + 8 + 4);
}
