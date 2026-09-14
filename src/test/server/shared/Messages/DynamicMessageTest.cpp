/*
 * Project Ambrose by Imjustchico
 * Tests dynamic messages on Ambrose-authored fixtures: type-checked access, sizes, decode status, log dumps, and the round-trip suite over every fixture id.
 */

#include "DynamicMessage.h"
#include "Hex.h"
#include "MessageRoundTrip.h"

#include <gtest/gtest.h>

#include <stdexcept>

namespace
{
    constexpr std::string_view FixtureXml = R"(<?xml version="1.0" ?>
<DynamicFixtureMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">9</ServiceID><ProtocolType TYPE="STR">DYNAMIC</ProtocolType></RECORD></_ProtocolInfo>
<MSG_EVERY><RECORD><Signed TYPE="BYT"></Signed><Flag TYPE="UBYT">1</Flag><Short TYPE="SHRT"></Short><Port TYPE="USHRT"></Port><Count TYPE="INT">-5</Count><Mask TYPE="UINT"></Mask><Speed TYPE="FLT">2.5</Speed><Scale TYPE="DBL"></Scale><Target TYPE="GID"></Target><Name TYPE="STR">guest</Name><Title TYPE="WSTR"></Title></RECORD></MSG_EVERY>
<MSG_EMPTY><RECORD></RECORD></MSG_EMPTY>
<MSG_TEXT><RECORD><Body TYPE="STR"></Body><Wide TYPE="WSTR"></Wide></RECORD></MSG_TEXT>
</DynamicFixtureMessages>
)";

    class DynamicMessageTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            MessageDefinitionSet set;
            ASSERT_TRUE(set.Add(FixtureXml, "DynamicFixtureMessages.xml"));
            ASSERT_TRUE(_registry.Load(std::move(set)));
        }

        MessageInfo const& Info(std::string_view tag) const
        {
            MessageInfo const* const info = _registry.Find(9, tag);
            if (!info)
                throw std::runtime_error(std::string(tag) + " is missing");
            return *info;
        }

        MessageRegistry _registry;
    };
}

TEST_F(DynamicMessageTest, StartsFromDefaultsAndChecksTypes)
{
    DynamicMessage message(Info("MSG_EVERY"));
    EXPECT_EQ(message.GetDefinition().Tag, "MSG_EVERY");
    ASSERT_EQ(message.GetValues().size(), 11u);
    EXPECT_EQ(*message.Find("Flag"), DmlValue(uint8(1)));
    EXPECT_EQ(*message.Find("Count"), DmlValue(int32(-5)));
    EXPECT_EQ(*message.Find("Speed"), DmlValue(2.5f));
    EXPECT_EQ(*message.Find("Name"), DmlValue(std::string("guest")));
    EXPECT_EQ(message.Find("Missing"), nullptr);

    EXPECT_TRUE(message.Set("Count", DmlValue(int32(12))));
    EXPECT_FALSE(message.Set("Count", DmlValue(uint32(12))));
    EXPECT_FALSE(message.Set("Count", DmlValue(int8(12))));
    EXPECT_FALSE(message.Set("Missing", DmlValue(int32(1))));
    EXPECT_FALSE(message.Set(11, DmlValue(int32(1))));
    EXPECT_TRUE(message.Set(10, DmlValue(std::u16string(u"Hi"))));
    EXPECT_EQ(*message.Find("Count"), DmlValue(int32(12)));

    message.Reset();
    EXPECT_EQ(*message.Find("Count"), DmlValue(int32(-5)));
    EXPECT_EQ(*message.Find("Title"), DmlValue(std::u16string()));

    for (DmlType type : { DmlType::Byt, DmlType::Ubyt, DmlType::Shrt, DmlType::Ushrt, DmlType::Int, DmlType::Uint, DmlType::Flt, DmlType::Dbl, DmlType::Gid, DmlType::Str, DmlType::Wstr })
    {
        EXPECT_TRUE(DynamicMessage::Holds(Dml::DefaultValue(type), type)) << Dml::GetTypeName(type);
        EXPECT_FALSE(DynamicMessage::Holds(Dml::DefaultValue(type), type == DmlType::Wstr ? DmlType::Str : DmlType::Wstr)) << Dml::GetTypeName(type);
    }
}

TEST_F(DynamicMessageTest, EncodesDefaultsAndReportsMinimumSize)
{
    DynamicMessage const message(Info("MSG_EVERY"));
    ByteBuffer buffer;
    message.Encode(buffer);
    EXPECT_EQ(Hex::Encode(buffer.GetData()), "00" "01" "0000" "0000" "fbffffff" "00000000" "00002040" "0000000000000000" "0000000000000000" "0500" "6775657374" "0000");
    EXPECT_EQ(message.GetEncodedSize(), buffer.GetSize());
    EXPECT_EQ(Info("MSG_EVERY").MinSize, 1u + 1 + 2 + 2 + 4 + 4 + 4 + 8 + 8 + 2 + 2);
    EXPECT_EQ(Info("MSG_EMPTY").MinSize, 0u);
    EXPECT_EQ(Info("MSG_TEXT").MinSize, 4u);
    EXPECT_EQ(DynamicMessage::GetEncodedSize(DmlValue(std::u16string(u"abc"))), 8u);
    EXPECT_EQ(DynamicMessage::GetEncodedSize(DmlValue(std::string("abc"))), 5u);
    EXPECT_EQ(DynamicMessage::GetEncodedSize(DmlValue(uint64(1))), 8u);
}

TEST_F(DynamicMessageTest, DecodeReportsStatusAndKeepsValuesOnFailure)
{
    DynamicMessage message(Info("MSG_TEXT"));
    std::optional<std::vector<uint8>> const body = Hex::Decode("0200" "6869" "0100" "4100");
    ASSERT_TRUE(body.has_value());
    ASSERT_EQ(message.Decode(*body), MessageDecodeStatus::Ok);
    EXPECT_EQ(*message.Find("Body"), DmlValue(std::string("hi")));
    EXPECT_EQ(*message.Find("Wide"), DmlValue(std::u16string(u"A")));

    std::vector<uint8> truncated(body->begin(), body->end() - 1);
    ASSERT_TRUE(message.Set("Body", DmlValue(std::string("kept"))));
    ASSERT_TRUE(message.Set("Wide", DmlValue(std::u16string(u"Q"))));
    EXPECT_EQ(message.Decode(truncated), MessageDecodeStatus::Truncated);
    EXPECT_EQ(*message.Find("Body"), DmlValue(std::string("kept")));
    EXPECT_EQ(*message.Find("Wide"), DmlValue(std::u16string(u"Q")));

    std::vector<uint8> trailing = *body;
    trailing.push_back(0);
    EXPECT_EQ(message.Decode(trailing), MessageDecodeStatus::TrailingBytes);

    std::vector<uint8> framed{ 0xEE };
    framed.insert(framed.end(), body->begin(), body->end());
    ByteBuffer buffer(framed);
    buffer.SetReadPosition(1);
    EXPECT_TRUE(message.Decode(buffer));
    EXPECT_EQ(buffer.GetRemaining(), 0u);
    ByteBuffer shortBuffer(truncated);
    ASSERT_TRUE(message.Set("Body", DmlValue(std::string("again"))));
    EXPECT_FALSE(message.Decode(shortBuffer));
    EXPECT_EQ(shortBuffer.GetReadPosition(), 0u);
    EXPECT_EQ(*message.Find("Body"), DmlValue(std::string("again")));

    DynamicMessage empty(Info("MSG_EMPTY"));
    EXPECT_EQ(empty.Decode(std::span<uint8 const>()), MessageDecodeStatus::Ok);
    EXPECT_EQ(empty.Decode(*body), MessageDecodeStatus::TrailingBytes);
}

TEST_F(DynamicMessageTest, ToStringNamesFieldsAndEscapesText)
{
    DynamicMessage message(Info("MSG_EVERY"));
    ASSERT_TRUE(message.Set("Signed", DmlValue(int8(-3))));
    ASSERT_TRUE(message.Set("Target", DmlValue(uint64(0x1122334455667788ull))));
    ASSERT_TRUE(message.Set("Name", DmlValue(std::string("a\"b\\c\n\x01"))));
    ASSERT_TRUE(message.Set("Title", DmlValue(std::u16string(u"café"))));
    EXPECT_EQ(message.ToString(), "MSG_EVERY (9:2) { Signed=-3, Flag=1, Short=0, Port=0, Count=-5, Mask=0, Speed=2.5, Scale=0, Target=0x1122334455667788, Name=\"a\\\"b\\\\c\\x0a\\x01\", Title=u\"caf\xC3\xA9\" }");

    DynamicMessage const empty(Info("MSG_EMPTY"));
    EXPECT_EQ(empty.ToString(), "MSG_EMPTY (9:1) {}");

    DynamicMessage text(Info("MSG_TEXT"));
    ASSERT_TRUE(text.Set("Body", DmlValue(std::string(100, 'x'))));
    ASSERT_TRUE(text.Set("Wide", DmlValue(std::u16string(70, u'y'))));
    std::string const dump = text.ToString(4);
    EXPECT_EQ(dump, "MSG_TEXT (9:3) { Body=\"xxxx\"...(100 bytes), Wide=u\"yyyy\"...(70 units) }");
    EXPECT_EQ(DynamicMessage::FormatValue(DmlValue(std::u16string(1, char16_t(0xD800)))), "u\"\xEF\xBF\xBD\"");
    EXPECT_EQ(DynamicMessage::FormatValue(DmlValue(std::string("a\xC3\xA9\xFF"))), "\"a\\xc3\\xa9\\xff\"");
    std::u16string const pair = u"abc\U0001F600x";
    EXPECT_EQ(DynamicMessage::FormatValue(DmlValue(pair), 4), "u\"abc\"...(6 units)");
    EXPECT_EQ(DynamicMessage::FormatValue(DmlValue(pair), 5), "u\"abc\xF0\x9F\x98\x80\"...(6 units)");
    EXPECT_EQ(DynamicMessage::FormatValue(DmlValue(std::u16string(u"ab\xD83D")), 3), "u\"ab\xEF\xBF\xBD\"");
}

TEST_F(DynamicMessageTest, SetRejectsStringsLongerThanTheWireLimit)
{
    DynamicMessage message(Info("MSG_TEXT"));
    EXPECT_FALSE(message.Set("Body", DmlValue(std::string(Dml::MaxStringLength + 1, 'a'))));
    EXPECT_FALSE(message.Set("Wide", DmlValue(std::u16string(Dml::MaxStringLength + 1, u'a'))));
    EXPECT_EQ(*message.Find("Body"), DmlValue(std::string()));
    ASSERT_TRUE(message.Set("Body", DmlValue(std::string(Dml::MaxStringLength, 'a'))));
    ASSERT_TRUE(message.Set("Wide", DmlValue(std::u16string(Dml::MaxStringLength, u'b'))));
    ByteBuffer buffer;
    message.Encode(buffer);
    EXPECT_EQ(buffer.GetSize(), message.GetEncodedSize());
    EXPECT_EQ(buffer.GetSize(), 2u + Dml::MaxStringLength + 2 + 2 * Dml::MaxStringLength);
    DynamicMessage decoded(Info("MSG_TEXT"));
    EXPECT_EQ(decoded.Decode(buffer.GetData()), MessageDecodeStatus::Ok);
    EXPECT_EQ(decoded.GetValues(), message.GetValues());
}

TEST_F(DynamicMessageTest, EveryFixtureMessageRoundTrips)
{
    ASSERT_EQ(_registry.GetMessages().size(), 3u);
    for (MessageInfo const& info : _registry.GetMessages())
    {
        for (uint64 seed : { MessageRoundTrip::SeedFor(info), uint64(1), uint64(0xFFFFFFFFFFFFFFFFull) })
        {
            std::string const failure = MessageRoundTrip::Check(info, seed);
            EXPECT_TRUE(failure.empty()) << failure;
        }
    }
    DynamicMessage const random = MessageRoundTrip::MakeRandom(Info("MSG_EVERY"), 7);
    DynamicMessage const same = MessageRoundTrip::MakeRandom(Info("MSG_EVERY"), 7);
    for (std::size_t i = 0; i < random.GetValues().size(); ++i)
        EXPECT_TRUE(MessageRoundTrip::SameValue(random.GetValues()[i], same.GetValues()[i]));
    EXPECT_FALSE(MessageRoundTrip::SameValue(DmlValue(0.0f), DmlValue(-0.0f)));
    EXPECT_FALSE(MessageRoundTrip::SameValue(DmlValue(int32(1)), DmlValue(uint32(1))));
}
