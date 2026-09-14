/*
 * Project Ambrose by Imjustchico
 * Tests KI frames: hand-built vectors, long frames in both length modes, chained DML messages, protocol errors, and randomized stream splits.
 */

#include "AllocationCounter.h"
#include "FrameReassembler.h"
#include "FrameWriter.h"
#include "Hex.h"

#include <gtest/gtest.h>

#include <random>

namespace
{
    std::vector<uint8> Bytes(std::string_view hex)
    {
        std::optional<std::vector<uint8>> bytes = Hex::Decode(hex);
        EXPECT_TRUE(bytes.has_value()) << hex;
        return bytes.value_or(std::vector<uint8>());
    }

    std::vector<uint8> Sequence(std::size_t size, uint8 start = 0)
    {
        std::vector<uint8> bytes(size);
        for (std::size_t i = 0; i < size; ++i)
            bytes[i] = static_cast<uint8>(start + i);
        return bytes;
    }

    std::vector<Frame> DrainAll(FrameReassembler& reassembler)
    {
        std::vector<Frame> frames;
        while (std::optional<Frame> frame = reassembler.Next())
            frames.push_back(std::move(*frame));
        return frames;
    }
}

TEST(FrameTest, ControlAndDmlVectorsMatchTheLengthRules)
{
    ByteBuffer control;
    FrameWriter::WriteControl(control, 0, Sequence(14, 0xA0));
    EXPECT_EQ(Hex::Encode(control.GetData()), "0df0" "1300" "01" "00" "0000" "a0a1a2a3a4a5a6a7a8a9aaabacad" "00");
    EXPECT_EQ(control.GetSize(), 23u);

    ByteBuffer dml;
    FrameWriter::WriteDml(dml, 5, 36, Sequence(10, 0x10));
    EXPECT_EQ(Hex::Encode(dml.GetData()), "0df0" "1300" "00" "00" "0000" "05" "24" "0e00" "10111213141516171819" "00");
    EXPECT_EQ(dml.GetSize(), 23u);

    ByteBuffer eight;
    FrameWriter::WriteDml(eight, 5, 0x24, Sequence(8));
    EXPECT_EQ(Hex::Encode(eight.GetData()), "0df0" "1100" "00" "00" "0000" "05" "24" "0c00" "0001020304050607" "00");

    ByteBuffer empty;
    FrameWriter::WriteControl(empty, 3, {});
    EXPECT_EQ(Hex::Encode(empty.GetData()), "0df0" "0500" "01" "03" "0000" "00");
    ByteBuffer emptyDml;
    FrameWriter::WriteDml(emptyDml, 1, 1, {});
    EXPECT_EQ(Hex::Encode(emptyDml.GetData()), "0df0" "0900" "00" "00" "0000" "01" "01" "0400" "00");
}

TEST(FrameTest, ReassemblerParsesWhatTheWriterBuilds)
{
    ByteBuffer stream;
    FrameWriter::WriteControl(stream, 5, Sequence(16));
    FrameWriter::WriteDml(stream, 7, 27, Sequence(40));
    Frame custom;
    custom.IsControl = true;
    custom.Opcode = 4;
    custom.Reserved = 0xBEEF;
    custom.Trailer = 0x7E;
    custom.Payload = Sequence(6);
    FrameWriter::WriteFrame(stream, custom);

    FrameReassembler reassembler;
    reassembler.Feed(stream.GetData());
    std::vector<Frame> const frames = DrainAll(reassembler);
    ASSERT_EQ(frames.size(), 3u);
    EXPECT_FALSE(reassembler.HasError());
    EXPECT_EQ(reassembler.GetBufferedSize(), 0u);

    EXPECT_TRUE(frames[0].IsControl);
    EXPECT_EQ(frames[0].Opcode, 5);
    EXPECT_EQ(frames[0].Payload, Sequence(16));
    EXPECT_FALSE(frames[0].IsLong);
    EXPECT_FALSE(frames[1].IsControl);
    std::vector<DmlMessageData> messages;
    ASSERT_EQ(FrameLayout::SplitDmlMessages(frames[1].Payload, messages), FrameError::None);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].ServiceId, 7);
    EXPECT_EQ(messages[0].Order, 27);
    EXPECT_EQ(messages[0].Body, Sequence(40));
    EXPECT_EQ(frames[2], custom);
}

TEST(FrameTest, OneByteAtATimeYieldsOneFrame)
{
    ByteBuffer stream;
    FrameWriter::WriteDml(stream, 5, 7, Sequence(300));
    FrameReassembler reassembler;
    std::size_t yielded = 0;
    for (uint8 byte : stream.GetData())
    {
        reassembler.Feed(std::span<uint8 const>(&byte, 1));
        while (std::optional<Frame> frame = reassembler.Next())
        {
            ++yielded;
            EXPECT_EQ(frame->Payload.size(), 304u);
        }
    }
    EXPECT_EQ(yielded, 1u);
    EXPECT_FALSE(reassembler.HasError());
    EXPECT_EQ(reassembler.GetBufferedSize(), 0u);
}

TEST(FrameTest, ChainedDmlMessagesSplitInOrder)
{
    std::vector<DmlMessageData> const sent{ { 5, 7, Sequence(3) }, { 12, 92, {} }, { 53, 64, Sequence(20, 9) } };
    ByteBuffer stream;
    FrameWriter::WriteDml(stream, sent);
    FrameReassembler reassembler;
    reassembler.Feed(stream.GetData());
    std::optional<Frame> const frame = reassembler.Next();
    ASSERT_TRUE(frame.has_value());
    std::vector<DmlMessageData> received;
    ASSERT_EQ(FrameLayout::SplitDmlMessages(frame->Payload, received), FrameError::None);
    EXPECT_EQ(received, sent);

    std::vector<uint8> const two = Bytes("0df0" "0e00" "00" "00" "0000" "05" "07" "0500" "aa" "0c" "5c" "0400" "00");
    FrameReassembler pair;
    pair.Feed(two);
    std::optional<Frame> const pairFrame = pair.Next();
    ASSERT_TRUE(pairFrame.has_value());
    std::vector<DmlMessageData> pairMessages;
    ASSERT_EQ(FrameLayout::SplitDmlMessages(pairFrame->Payload, pairMessages), FrameError::None);
    ASSERT_EQ(pairMessages.size(), 2u);
    EXPECT_EQ(pairMessages[0].Body, std::vector<uint8>{ 0xAA });
    EXPECT_EQ(pairMessages[1].Order, 92);
    EXPECT_TRUE(pairMessages[1].Body.empty());

    EXPECT_THROW(FrameWriter::WriteDml(stream, std::span<DmlMessageData const>()), std::invalid_argument);
}

TEST(FrameTest, ProtocolErrorsStopTheStream)
{
    auto errorOf = [](std::string_view hex, FrameLimits limits = FrameLimits())
    {
        FrameReassembler reassembler(limits);
        reassembler.Feed(Bytes(hex));
        EXPECT_FALSE(reassembler.Next().has_value()) << hex;
        return reassembler.GetError();
    };
    EXPECT_EQ(errorOf("00"), FrameError::BadMagic);
    EXPECT_EQ(errorOf("0d00"), FrameError::BadMagic);
    EXPECT_EQ(errorOf("ffff0df0" "0500" "01000000" "00"), FrameError::BadMagic);
    EXPECT_EQ(errorOf("0df0" "0400" "01000000"), FrameError::BadLength);
    EXPECT_EQ(errorOf("0df0" "0800" "00000000" "05070400"), FrameError::BadLength);
    EXPECT_EQ(errorOf("0df0" "0500" "02000000" "00"), FrameError::BadControlFlag);
    EXPECT_EQ(errorOf("0df0" "0900" "00000000" "05070500" "00"), FrameError::BadDmlLength);
    EXPECT_EQ(errorOf("0df0" "0a00" "00000000" "05070400" "aa" "00"), FrameError::BadDmlLength);
    EXPECT_EQ(errorOf("0df0" "0900" "00000000" "05070300" "00"), FrameError::BadDmlLength);
    EXPECT_EQ(errorOf("0df0" "0080" "04000000" "03"), FrameError::BadControlFlag);
    EXPECT_EQ(errorOf("0df0" "0080" "00000000" "00", FrameLimits{ 64, LongFrameLength::HeaderAndBody }), FrameError::BadLength);
    EXPECT_EQ(errorOf("0df0" "0500"), FrameError::None);

    FrameReassembler stopped;
    stopped.Feed(Bytes("0df0" "0500" "02000000" "00"));
    EXPECT_FALSE(stopped.Next().has_value());
    ByteBuffer valid;
    FrameWriter::WriteControl(valid, 0, Sequence(4));
    stopped.Feed(valid.GetData());
    EXPECT_FALSE(stopped.Next().has_value());
    EXPECT_EQ(stopped.GetError(), FrameError::BadControlFlag);
    stopped.Reset();
    EXPECT_FALSE(stopped.HasError());
    stopped.Feed(valid.GetData());
    EXPECT_TRUE(stopped.Next().has_value());

    EXPECT_EQ(FrameLayout::GetErrorName(FrameError::TooLarge), "frame too large");
    EXPECT_EQ(FrameLayout::GetErrorName(FrameError::BadDmlLength), "bad DML length");
}

TEST(FrameTest, OversizedFramesFailBeforeBuffering)
{
    FrameLimits const limits{ 64, LongFrameLength::BodyOnly };
    FrameReassembler shortFrame(limits);
    shortFrame.Feed(Bytes("0df0" "ffff" "01000000"));
    EXPECT_FALSE(shortFrame.Next().has_value());
    EXPECT_EQ(shortFrame.GetError(), FrameError::TooLarge);

    std::vector<uint8> const hugeLong = Bytes("0df0" "0080" "f0ffffff" "01000000");
    if (AllocationScope::IsSupported())
    {
        FrameReassembler reassembler;
        reassembler.Feed(hugeLong);
        std::size_t largest = 0;
        {
            AllocationScope scope;
            EXPECT_FALSE(reassembler.Next().has_value());
            largest = scope.GetLargest();
        }
        EXPECT_EQ(reassembler.GetError(), FrameError::TooLarge);
        EXPECT_LT(largest, 1024u);
    }
    FrameReassembler exact(FrameLimits{ 23, LongFrameLength::BodyOnly });
    ByteBuffer control;
    FrameWriter::WriteControl(control, 0, Sequence(14));
    exact.Feed(control.GetData());
    EXPECT_TRUE(exact.Next().has_value());
    FrameReassembler over(FrameLimits{ 22, LongFrameLength::BodyOnly });
    over.Feed(std::span<uint8 const>(control.GetData()).first(5));
    EXPECT_FALSE(over.Next().has_value());
    EXPECT_EQ(over.GetError(), FrameError::TooLarge);
}

TEST(FrameTest, LongFramesUseTheMarkerInBothLengthModes)
{
    std::vector<uint8> const shortBody = Sequence(FrameLayout::MaxShortBody);
    ByteBuffer atLimit;
    FrameWriter::WriteControl(atLimit, 1, shortBody);
    EXPECT_EQ(Hex::Encode(atLimit.GetData().first(4)), "0df08477");
    ByteBuffer dmlAtLimit;
    FrameWriter::WriteDml(dmlAtLimit, 5, 1, shortBody);
    EXPECT_EQ(Hex::Encode(dmlAtLimit.GetData().first(4)), "0df08877");

    std::vector<uint8> const longBody = Sequence(FrameLayout::MaxShortBody + 1);
    ByteBuffer bodyOnly;
    FrameWriter::WriteDml(bodyOnly, 5, 1, longBody, LongFrameLength::BodyOnly);
    EXPECT_EQ(Hex::Encode(bodyOnly.GetData().first(16)), "0df0" "0080" "80770000" "00000000" "0501" "8477");
    EXPECT_EQ(bodyOnly.GetSize(), 8u + 4 + 4 + longBody.size() + 1);

    ByteBuffer headerAndBody;
    FrameWriter::WriteDml(headerAndBody, 5, 1, longBody, LongFrameLength::HeaderAndBody);
    EXPECT_EQ(Hex::Encode(headerAndBody.GetData().first(8)), "0df0" "0080" "88770000");
    EXPECT_EQ(headerAndBody.GetSize(), bodyOnly.GetSize());

    ByteBuffer controlLong;
    FrameWriter::WriteControl(controlLong, 2, longBody, LongFrameLength::BodyOnly);
    EXPECT_EQ(Hex::Encode(controlLong.GetData().first(8)), "0df0" "0080" "80770000");

    for (LongFrameLength mode : { LongFrameLength::BodyOnly, LongFrameLength::HeaderAndBody })
    {
        ByteBuffer stream;
        FrameWriter::WriteDml(stream, 5, 1, longBody, mode);
        FrameWriter::WriteControl(stream, 2, longBody, mode);
        FrameReassembler reassembler(FrameLimits{ FrameLimits::DefaultMaxFrameSize, mode });
        reassembler.Feed(stream.GetData());
        std::vector<Frame> const frames = DrainAll(reassembler);
        ASSERT_EQ(frames.size(), 2u) << static_cast<int>(mode);
        EXPECT_TRUE(frames[0].IsLong);
        std::vector<DmlMessageData> messages;
        ASSERT_EQ(FrameLayout::SplitDmlMessages(frames[0].Payload, messages), FrameError::None);
        EXPECT_EQ(messages.at(0).Body, longBody);
        EXPECT_EQ(frames[1].Payload, longBody);
    }

    Frame forced;
    forced.IsControl = true;
    forced.IsLong = true;
    forced.Payload = Sequence(3);
    ByteBuffer forcedBuffer;
    FrameWriter::WriteFrame(forcedBuffer, forced);
    EXPECT_EQ(Hex::Encode(forcedBuffer.GetData()), "0df0" "0080" "03000000" "01000000" "000102" "00");

    EXPECT_THROW(FrameWriter::WriteDml(bodyOnly, 5, 1, Sequence(FrameLayout::MaxDmlBody + 1)), std::length_error);
    Frame badDml;
    badDml.Payload = Sequence(3);
    EXPECT_THROW(FrameWriter::WriteFrame(bodyOnly, badDml), std::invalid_argument);
}

TEST(FrameTest, RandomizedSplitsReassembleEveryFrame)
{
    for (LongFrameLength mode : { LongFrameLength::BodyOnly, LongFrameLength::HeaderAndBody })
    {
        std::mt19937_64 random(0xF00DF00Dull + static_cast<uint64>(mode));
        std::vector<Frame> sent;
        ByteBuffer stream;
        for (int i = 0; i < 10000; ++i)
        {
            Frame frame;
            frame.IsControl = (random() % 4) == 0;
            frame.Opcode = static_cast<uint8>(random());
            frame.Reserved = static_cast<uint16>(random());
            frame.Trailer = (random() % 16) == 0 ? static_cast<uint8>(random()) : 0;
            std::size_t const bodySize = (random() % 500) == 0 ? FrameLayout::MaxShortBody + 1 + random() % 64 : random() % 200;
            if (frame.IsControl)
                frame.Payload = Sequence(bodySize, static_cast<uint8>(i));
            else
            {
                std::size_t const count = 1 + random() % 3;
                for (std::size_t m = 0; m < count; ++m)
                {
                    std::size_t const size = m == 0 ? bodySize : random() % 32;
                    frame.Payload.push_back(static_cast<uint8>(random()));
                    frame.Payload.push_back(static_cast<uint8>(random()));
                    frame.Payload.push_back(static_cast<uint8>((size + 4) & 0xFF));
                    frame.Payload.push_back(static_cast<uint8>((size + 4) >> 8));
                    std::vector<uint8> const body = Sequence(size, static_cast<uint8>(m));
                    frame.Payload.insert(frame.Payload.end(), body.begin(), body.end());
                }
            }
            frame.IsLong = frame.Payload.size() - (frame.IsControl ? 0 : 4) > FrameLayout::MaxShortBody;
            FrameWriter::WriteFrame(stream, frame, mode);
            sent.push_back(std::move(frame));
        }

        FrameReassembler reassembler(FrameLimits{ FrameLimits::DefaultMaxFrameSize, mode });
        std::vector<Frame> received;
        std::span<uint8 const> const data = stream.GetData();
        std::size_t offset = 0;
        while (offset < data.size())
        {
            std::size_t const chunk = std::min<std::size_t>(data.size() - offset, 1 + random() % 4096);
            reassembler.Feed(data.subspan(offset, chunk));
            offset += chunk;
            std::vector<Frame> drained = DrainAll(reassembler);
            for (Frame& frame : drained)
                received.push_back(std::move(frame));
        }
        EXPECT_FALSE(reassembler.HasError()) << FrameLayout::GetErrorName(reassembler.GetError());
        EXPECT_EQ(reassembler.GetBufferedSize(), 0u);
        ASSERT_EQ(received.size(), sent.size());
        for (std::size_t i = 0; i < sent.size(); ++i)
            ASSERT_EQ(received[i], sent[i]) << "frame " << i;
    }
}

TEST(FrameTest, LimitsChangeLiveFromTheNextFrame)
{
    ByteBuffer stream;
    FrameWriter::WriteControl(stream, 0, Sequence(40));
    FrameWriter::WriteControl(stream, 0, Sequence(40));
    std::size_t const frameSize = stream.GetSize() / 2;

    FrameReassembler reassembler(FrameLimits{ frameSize, LongFrameLength::BodyOnly });
    reassembler.Feed(std::span<uint8 const>(stream.GetData()).first(frameSize + 6));
    ASSERT_TRUE(reassembler.Next().has_value());
    reassembler.SetLimits(FrameLimits{ frameSize - 1, LongFrameLength::BodyOnly });
    EXPECT_EQ(reassembler.GetLimits().MaxFrameSize, frameSize - 1);
    EXPECT_FALSE(reassembler.Next().has_value());
    EXPECT_EQ(reassembler.GetError(), FrameError::TooLarge);

    FrameReassembler raised(FrameLimits{ frameSize - 1, LongFrameLength::BodyOnly });
    raised.SetLimits(FrameLimits{ frameSize, LongFrameLength::BodyOnly });
    raised.Feed(stream.GetData());
    EXPECT_EQ(DrainAll(raised).size(), 2u);
    EXPECT_FALSE(raised.HasError());
}

TEST(FrameTest, LongFramesArriveOneByteAtATimeInBothModes)
{
    std::vector<uint8> const body = Sequence(FrameLayout::MaxShortBody + 3, 7);
    for (LongFrameLength mode : { LongFrameLength::BodyOnly, LongFrameLength::HeaderAndBody })
    {
        ByteBuffer stream;
        FrameWriter::WriteDml(stream, 5, 9, body, mode);
        FrameWriter::WriteControl(stream, 2, body, mode);
        FrameReassembler reassembler(FrameLimits{ FrameLimits::DefaultMaxFrameSize, mode });
        std::vector<Frame> frames;
        for (uint8 byte : stream.GetData())
        {
            reassembler.Feed(std::span<uint8 const>(&byte, 1));
            while (std::optional<Frame> frame = reassembler.Next())
                frames.push_back(std::move(*frame));
        }
        ASSERT_EQ(frames.size(), 2u) << static_cast<int>(mode);
        EXPECT_FALSE(reassembler.HasError());
        EXPECT_TRUE(frames[0].IsLong);
        EXPECT_EQ(frames[0].Payload.size(), body.size() + FrameLayout::DmlHeaderSize);
        EXPECT_EQ(frames[1].Payload, body);
    }
}

TEST(FrameTest, LongLengthModeIsFixedForAFrameAlreadyStarted)
{
    std::vector<uint8> const body = Sequence(FrameLayout::MaxShortBody + 3);
    ByteBuffer stream;
    FrameWriter::WriteControl(stream, 2, body, LongFrameLength::BodyOnly);
    FrameWriter::WriteControl(stream, 2, body, LongFrameLength::HeaderAndBody);

    FrameReassembler reassembler(FrameLimits{ FrameLimits::DefaultMaxFrameSize, LongFrameLength::BodyOnly });
    std::span<uint8 const> const data = stream.GetData();
    reassembler.Feed(data.first(100));
    EXPECT_FALSE(reassembler.Next().has_value());
    reassembler.SetLimits(FrameLimits{ FrameLimits::DefaultMaxFrameSize, LongFrameLength::HeaderAndBody });
    reassembler.Feed(data.subspan(100));
    std::vector<Frame> const frames = DrainAll(reassembler);
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].Payload, body);
    EXPECT_EQ(frames[1].Payload, body);
    EXPECT_FALSE(reassembler.HasError());
}

TEST(FrameTest, DmlMessageCountIsLimitedLive)
{
    std::vector<DmlMessageData> messages(5, DmlMessageData{ 1, 1, {} });
    ByteBuffer stream;
    FrameWriter::WriteDml(stream, messages);

    FrameReassembler limited(FrameLimits{ FrameLimits::DefaultMaxFrameSize, LongFrameLength::BodyOnly, 4 });
    limited.Feed(stream.GetData());
    EXPECT_FALSE(limited.Next().has_value());
    EXPECT_EQ(limited.GetError(), FrameError::TooManyDmlMessages);
    EXPECT_EQ(FrameLayout::GetErrorName(FrameError::TooManyDmlMessages), "too many DML messages");

    FrameReassembler raised(FrameLimits{ FrameLimits::DefaultMaxFrameSize, LongFrameLength::BodyOnly, 4 });
    raised.SetLimits(FrameLimits{ FrameLimits::DefaultMaxFrameSize, LongFrameLength::BodyOnly, 5 });
    raised.Feed(stream.GetData());
    std::optional<Frame> const frame = raised.Next();
    ASSERT_TRUE(frame.has_value());
    std::vector<DmlMessageData> split;
    EXPECT_EQ(FrameLayout::SplitDmlMessages(frame->Payload, split, 4), FrameError::TooManyDmlMessages);
    EXPECT_TRUE(split.empty());
    EXPECT_EQ(FrameLayout::SplitDmlMessages(frame->Payload, split, 5), FrameError::None);
    EXPECT_EQ(split.size(), 5u);
}

TEST(FrameTest, BufferMemoryIsReleasedAfterALargeFrame)
{
    ByteBuffer stream;
    FrameWriter::WriteControl(stream, 1, Sequence(std::size_t{ 1 } << 20));
    FrameReassembler reassembler;
    std::span<uint8 const> const data = stream.GetData();
    for (std::size_t offset = 0; offset < data.size(); offset += 4096)
        reassembler.Feed(data.subspan(offset, std::min<std::size_t>(4096, data.size() - offset)));
    EXPECT_GE(reassembler.GetBufferCapacity(), data.size());
    ASSERT_TRUE(reassembler.Next().has_value());
    EXPECT_EQ(reassembler.GetBufferedSize(), 0u);
    EXPECT_LE(reassembler.GetBufferCapacity(), std::size_t{ 64 } << 10);

    ByteBuffer small;
    FrameWriter::WriteControl(small, 1, Sequence(16));
    reassembler.Feed(small.GetData());
    EXPECT_TRUE(reassembler.Next().has_value());
}
