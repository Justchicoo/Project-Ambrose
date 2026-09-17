/*
 * Project Ambrose by Imjustchico
 * Tests control messages against hand-written byte vectors, trailing offer and accept bytes, strict keepalive sizes, a client keepalive frame without its trailing byte, timestamps, and framing.
 */

#include "ControlMessages.h"
#include "FrameReassembler.h"
#include "FrameWriter.h"
#include "Hex.h"

#include <gtest/gtest.h>

namespace
{
    std::vector<uint8> Bytes(std::string_view hex)
    {
        std::optional<std::vector<uint8>> bytes = Hex::Decode(hex);
        EXPECT_TRUE(bytes.has_value()) << hex;
        return bytes.value_or(std::vector<uint8>());
    }

    SessionTimestamp FixtureTime()
    {
        SessionTimestamp time;
        time.TimeHigh = 0;
        time.TimeLow = 0x66E5A1B2;
        time.Milliseconds = 0x0123;
        return time;
    }
}

TEST(ControlMessagesTest, SessionOfferMatchesItsVector)
{
    SessionOffer offer;
    offer.SessionId = 0x1234;
    offer.Time = FixtureTime();
    EXPECT_EQ(Hex::Encode(ControlMessages::Encode(offer)), "3412" "00000000" "b2a1e566" "23010000");

    ByteBuffer frame;
    ControlMessages::WriteFrame(frame, offer);
    EXPECT_EQ(Hex::Encode(frame.GetData()), "0df0" "1300" "01" "00" "0000" "3412" "00000000" "b2a1e566" "23010000" "00");
    EXPECT_EQ(frame.GetSize(), 23u);

    std::optional<SessionOffer> const decoded = ControlMessages::DecodeSessionOffer(ControlMessages::Encode(offer));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, offer);

    SessionOffer longer = offer;
    longer.Trailing = Bytes("0102030405");
    ByteBuffer longerFrame;
    ControlMessages::WriteFrame(longerFrame, longer);
    EXPECT_EQ(longerFrame.GetSize(), 28u);
    std::optional<SessionOffer> const longerDecoded = ControlMessages::DecodeSessionOffer(ControlMessages::Encode(longer));
    ASSERT_TRUE(longerDecoded.has_value());
    EXPECT_EQ(longerDecoded->Trailing, Bytes("0102030405"));
    EXPECT_FALSE(ControlMessages::DecodeSessionOffer(Bytes("3412" "00000000" "b2a1e566" "230100")).has_value());
}

TEST(ControlMessagesTest, SessionAcceptMatchesItsVector)
{
    SessionAccept accept;
    accept.Time = FixtureTime();
    accept.SessionId = 0x1234;
    EXPECT_EQ(Hex::Encode(ControlMessages::Encode(accept)), "0000" "00000000" "b2a1e566" "23010000" "3412");

    ByteBuffer frame;
    ControlMessages::WriteFrame(frame, accept);
    EXPECT_EQ(Hex::Encode(frame.GetData()), "0df0" "1500" "01" "05" "0000" "0000" "00000000" "b2a1e566" "23010000" "3412" "00");
    EXPECT_EQ(frame.GetSize(), 25u);

    std::optional<SessionAccept> const decoded = ControlMessages::DecodeSessionAccept(Bytes("beef" "00000000" "b2a1e566" "23010000" "3412" "aabb"));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->Reserved, 0xEFBE);
    EXPECT_EQ(decoded->SessionId, 0x1234);
    EXPECT_EQ(decoded->Time, FixtureTime());
    EXPECT_EQ(decoded->Trailing, Bytes("aabb"));
    EXPECT_EQ(ControlMessages::Encode(*decoded), Bytes("beef" "00000000" "b2a1e566" "23010000" "3412" "aabb"));
    EXPECT_FALSE(ControlMessages::DecodeSessionAccept(Bytes("0000" "00000000" "b2a1e566" "23010000" "34")).has_value());
}

TEST(ControlMessagesTest, KeepAlivesMatchTheirVectorsAndSizes)
{
    ClientKeepAlive client{ 0x1234, 500, 3 };
    ByteBuffer clientFrame;
    ControlMessages::WriteFrame(clientFrame, client);
    EXPECT_EQ(Hex::Encode(clientFrame.GetData()), "0df0" "0b00" "01" "03" "0000" "3412" "f401" "0300" "00");

    ServerKeepAlive server{ 0x1234, 0x0001E240 };
    ByteBuffer serverFrame;
    ControlMessages::WriteFrame(serverFrame, server);
    EXPECT_EQ(Hex::Encode(serverFrame.GetData()), "0df0" "0b00" "01" "03" "0000" "3412" "40e20100" "00");

    KeepAliveResponse response{ 0x1234, 999, 3 };
    ByteBuffer responseFrame;
    ControlMessages::WriteFrame(responseFrame, response);
    EXPECT_EQ(Hex::Encode(responseFrame.GetData()), "0df0" "0b00" "01" "04" "0000" "3412" "e703" "0300" "00");

    EXPECT_EQ(ControlMessages::DecodeClientKeepAlive(ControlMessages::Encode(client)), client);
    EXPECT_EQ(ControlMessages::DecodeServerKeepAlive(ControlMessages::Encode(server)), server);
    EXPECT_EQ(ControlMessages::DecodeKeepAliveResponse(ControlMessages::Encode(response)), response);
    EXPECT_FALSE(ControlMessages::DecodeClientKeepAlive(Bytes("3412f401030000")).has_value());
    EXPECT_FALSE(ControlMessages::DecodeServerKeepAlive(Bytes("341240e201")).has_value());
    EXPECT_FALSE(ControlMessages::DecodeKeepAliveResponse({}).has_value());
}

TEST(ControlMessagesTest, AClientKeepAliveFrameWithoutItsTrailingByteDecodes)
{
    FrameReassembler reassembler;
    reassembler.Feed(Bytes("0df0" "0a00" "01" "03" "0000" "3412" "f401" "0300" "0df0" "0b00" "01" "03" "0000" "3412" "f501" "0400" "00"));
    std::optional<Frame> const shortFrame = reassembler.Next();
    ASSERT_TRUE(shortFrame);
    EXPECT_EQ(ControlMessages::GetOpcode(*shortFrame), ControlOpcode::KeepAlive);
    EXPECT_EQ(ControlMessages::DecodeClientKeepAlive(*shortFrame), (ClientKeepAlive{ 0x1234, 500, 3 }));
    std::optional<Frame> const fullFrame = reassembler.Next();
    ASSERT_TRUE(fullFrame);
    EXPECT_EQ(ControlMessages::DecodeClientKeepAlive(*fullFrame), (ClientKeepAlive{ 0x1234, 501, 4 }));
    EXPECT_FALSE(reassembler.Next());
    EXPECT_FALSE(reassembler.HasError());

    Frame tooShort;
    tooShort.IsControl = true;
    tooShort.Opcode = 3;
    tooShort.Payload = Bytes("3412f401");
    EXPECT_FALSE(ControlMessages::DecodeClientKeepAlive(tooShort).has_value());
}

TEST(ControlMessagesTest, FramesRoundTripThroughTheReassembler)
{
    SessionOffer offer;
    offer.SessionId = 7;
    offer.Time = FixtureTime();
    ByteBuffer stream;
    ControlMessages::WriteFrame(stream, offer);
    ControlMessages::WriteFrame(stream, ClientKeepAlive{ 7, 10, 1 });
    FrameWriter::WriteControl(stream, 9, Bytes("00"));
    FrameWriter::WriteDml(stream, 1, 1, {});

    FrameReassembler reassembler;
    reassembler.Feed(stream.GetData());
    std::vector<Frame> frames;
    while (std::optional<Frame> frame = reassembler.Next())
        frames.push_back(std::move(*frame));
    ASSERT_EQ(frames.size(), 4u);
    EXPECT_EQ(ControlMessages::GetOpcode(frames[0]), ControlOpcode::SessionOffer);
    EXPECT_EQ(ControlMessages::DecodeSessionOffer(frames[0].Payload), offer);
    EXPECT_EQ(ControlMessages::GetOpcode(frames[1]), ControlOpcode::KeepAlive);
    EXPECT_EQ(ControlMessages::DecodeClientKeepAlive(frames[1].Payload), (ClientKeepAlive{ 7, 10, 1 }));
    EXPECT_FALSE(ControlMessages::GetOpcode(frames[2]).has_value());
    EXPECT_FALSE(ControlMessages::GetOpcode(frames[3]).has_value());
}

TEST(ControlMessagesTest, TimestampsSplitSecondsAndMilliseconds)
{
    std::chrono::system_clock::time_point const now{ std::chrono::milliseconds(1789400000123LL) };
    SessionTimestamp const stamp = SessionTimestamp::FromTimePoint(now);
    EXPECT_EQ(stamp.TimeHigh, 0);
    EXPECT_EQ(static_cast<uint32>(stamp.TimeLow), 0x6AA813C0u);
    EXPECT_EQ(stamp.Milliseconds, 123u);
    EXPECT_EQ(stamp.GetSeconds(), 1789400000u);

    std::chrono::system_clock::time_point const late{ std::chrono::milliseconds(4294967301999LL) };
    SessionTimestamp const lateStamp = SessionTimestamp::FromTimePoint(late);
    EXPECT_EQ(lateStamp.TimeHigh, 1);
    EXPECT_EQ(lateStamp.TimeLow, 5);
    EXPECT_EQ(lateStamp.Milliseconds, 999u);
    EXPECT_EQ(lateStamp.GetSeconds(), 4294967301u);

    SessionTimestamp const negative = SessionTimestamp::FromTimePoint(std::chrono::system_clock::time_point{ std::chrono::milliseconds(-5) });
    EXPECT_EQ(negative, SessionTimestamp());

    SessionTimestamp high;
    high.TimeLow = -1;
    EXPECT_EQ(high.GetSeconds(), 0xFFFFFFFFu);
}
