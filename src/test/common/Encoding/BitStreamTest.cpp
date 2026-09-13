/*
 * Project Ambrose by Imjustchico
 * Pins LSB-first bit order with golden bytes and tests widths, realignment, back-patching, and overruns.
 */

#include "BitReader.h"
#include "BitWriter.h"

#include <gtest/gtest.h>

#include <bit>
#include <random>
#include <utility>
#include <vector>

namespace
{
    std::vector<uint8> Bytes(BitWriter const& writer)
    {
        return std::vector<uint8>(writer.GetBytes().begin(), writer.GetBytes().end());
    }
}

TEST(BitStreamTest, GoldenBitThenAlignedUInt32ThenThreeBits)
{
    BitWriter writer;
    writer.WriteBit(true);
    writer.Write<uint32>(0xAABBCCDDu);
    writer.WriteBits(0b101, 3);
    EXPECT_EQ(Bytes(writer), (std::vector<uint8>{ 0x01, 0xDD, 0xCC, 0xBB, 0xAA, 0x05 }));

    BitReader reader(writer.GetBytes());
    EXPECT_TRUE(reader.ReadBit());
    EXPECT_EQ(reader.Read<uint32>(), 0xAABBCCDDu);
    EXPECT_EQ(reader.ReadBits(3), 0b101u);
    EXPECT_FALSE(reader.Failed());
}

TEST(BitStreamTest, GoldenFieldSpanningTwoBytes)
{
    BitWriter writer;
    writer.WriteBits(0b101, 3);
    writer.WriteBits(0x7F, 7);
    writer.Realign();
    EXPECT_EQ(Bytes(writer), (std::vector<uint8>{ 0xFD, 0x03 }));
    EXPECT_EQ(writer.GetBitSize(), 16u);

    BitReader reader(writer.GetBytes());
    EXPECT_EQ(reader.ReadBits(3), 0b101u);
    EXPECT_EQ(reader.ReadBits(7), 0x7Fu);
}

TEST(BitStreamTest, ClientBitWidthsRoundTrip)
{
    BitWriter writer;
    writer.WriteBit(true);
    writer.WriteBits(3, 2);
    writer.WriteBits(15, 4);
    writer.WriteBits(31, 5);
    writer.WriteBits(127, 7);
    writer.WriteSignedBits(-2, 24);
    writer.WriteBits(0xABCDEF, 24);
    writer.WriteSignedBits(-1, 24);

    BitReader reader(writer.GetBytes());
    EXPECT_TRUE(reader.ReadBit());
    EXPECT_EQ(reader.ReadBits(2), 3u);
    EXPECT_EQ(reader.ReadBits(4), 15u);
    EXPECT_EQ(reader.ReadBits(5), 31u);
    EXPECT_EQ(reader.ReadBits(7), 127u);
    EXPECT_EQ(reader.ReadSignedBits(24), -2);
    EXPECT_EQ(reader.ReadBits(24), 0xABCDEFu);
    EXPECT_EQ(reader.ReadSignedBits(24), -1);
    EXPECT_FALSE(reader.Failed());
}

TEST(BitStreamTest, SignedTwentyFourMinusOneIsAllOnes)
{
    BitWriter writer;
    writer.WriteSignedBits(-1, 24);
    EXPECT_EQ(Bytes(writer), (std::vector<uint8>{ 0xFF, 0xFF, 0xFF }));
    BitReader reader(writer.GetBytes());
    EXPECT_EQ(reader.ReadSignedBits(24), -1);
}

TEST(BitStreamTest, MixedBoolAndUInt16Realign)
{
    BitWriter writer;
    writer.WriteBit(true);
    writer.Write<uint16>(0x1234);
    writer.WriteBit(false);
    writer.WriteBit(true);
    writer.Write<uint16>(0xBEEF);
    EXPECT_EQ(Bytes(writer), (std::vector<uint8>{ 0x01, 0x34, 0x12, 0x02, 0xEF, 0xBE }));

    BitReader reader(writer.GetBytes());
    EXPECT_TRUE(reader.ReadBit());
    EXPECT_EQ(reader.Read<uint16>(), 0x1234u);
    EXPECT_FALSE(reader.ReadBit());
    EXPECT_TRUE(reader.ReadBit());
    EXPECT_EQ(reader.Read<uint16>(), 0xBEEFu);
    EXPECT_EQ(reader.GetRemainingBits(), 0u);
}

TEST(BitStreamTest, SixtyFourBitAndFloatingValuesRoundTrip)
{
    BitWriter writer;
    writer.WriteBits(0xFFFFFFFFFFFFFFFFull, 64);
    writer.Write<uint64>(0x0123456789ABCDEFull);
    writer.Write<float>(std::bit_cast<float>(uint32(0x7FC00321u)));
    writer.Write<double>(-2.5);

    BitReader reader(writer.GetBytes());
    EXPECT_EQ(reader.ReadBits(64), 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reader.Read<uint64>(), 0x0123456789ABCDEFull);
    EXPECT_EQ(std::bit_cast<uint32>(reader.Read<float>()), 0x7FC00321u);
    EXPECT_EQ(reader.Read<double>(), -2.5);
}

TEST(BitStreamTest, BitsAboveTheWidthAreIgnored)
{
    BitWriter writer;
    writer.WriteBits(0xFF, 3);
    writer.WriteBits(0, 5);
    EXPECT_EQ(Bytes(writer), (std::vector<uint8>{ 0x07 }));
}

TEST(BitStreamTest, ReadPastEndSetsFailedAndReturnsZeros)
{
    std::vector<uint8> const data{ 0xFF };
    BitReader reader(data);
    EXPECT_EQ(reader.ReadBits(4), 15u);
    EXPECT_EQ(reader.ReadBits(8), 0u);
    EXPECT_TRUE(reader.Failed());
    EXPECT_EQ(reader.GetBitPosition(), 4u);
    EXPECT_FALSE(reader.ReadBit());
    EXPECT_EQ(reader.Read<uint32>(), 0u);
    EXPECT_TRUE(reader.ReadBytes(1).empty());
    EXPECT_EQ(reader.ReadSignedBits(4), 0);
    EXPECT_TRUE(reader.Failed());
}

TEST(BitStreamTest, InvalidWidthAndSeekFailSafely)
{
    std::vector<uint8> const data{ 0x01, 0x02 };
    BitReader wide(data);
    EXPECT_EQ(wide.ReadBits(65), 0u);
    EXPECT_TRUE(wide.Failed());

    BitReader seeking(data);
    seeking.SeekBit(16);
    EXPECT_FALSE(seeking.Failed());
    seeking.SeekBit(17);
    EXPECT_TRUE(seeking.Failed());

    BitWriter writer;
    EXPECT_THROW(writer.WriteBits(0, 65), std::invalid_argument);
    EXPECT_THROW(writer.SeekBit(1), std::out_of_range);
}

TEST(BitStreamTest, BackPatchingRewritesAnEarlierUInt32Exactly)
{
    BitWriter writer;
    writer.WriteBit(true);
    writer.Realign();
    std::size_t const sizeField = writer.GetBitPosition();
    writer.Write<uint32>(0xFFFFFFFFu);
    std::size_t const payloadStart = writer.GetBitPosition();
    writer.WriteBits(0b1011, 4);
    writer.Write<uint16>(0x5566);
    std::size_t const payloadEnd = writer.GetBitPosition();

    writer.SeekBit(sizeField);
    writer.Write<uint32>(static_cast<uint32>(payloadEnd - payloadStart));
    writer.SeekBit(payloadEnd);
    writer.WriteBit(true);

    EXPECT_EQ(Bytes(writer), (std::vector<uint8>{ 0x01, 0x18, 0x00, 0x00, 0x00, 0x0B, 0x66, 0x55, 0x01 }));
    BitReader reader(writer.GetBytes());
    EXPECT_TRUE(reader.ReadBit());
    EXPECT_EQ(reader.Read<uint32>(), 24u);
    EXPECT_EQ(reader.ReadBits(4), 0b1011u);
    EXPECT_EQ(reader.Read<uint16>(), 0x5566u);
    EXPECT_TRUE(reader.ReadBit());
}

TEST(BitStreamTest, BackPatchingPartialBitsClearsOldOnes)
{
    BitWriter writer;
    writer.WriteBits(0xFF, 8);
    writer.SeekBit(2);
    writer.WriteBits(0, 3);
    EXPECT_EQ(Bytes(writer), (std::vector<uint8>{ 0xE3 }));
    EXPECT_EQ(writer.GetBitSize(), 8u);
}

TEST(BitStreamTest, RandomSequencesRoundTripAndTruncationsFailSafely)
{
    std::mt19937_64 engine(1610u);
    std::uniform_int_distribution<int> widthDistribution(1, 64);
    std::uniform_int_distribution<int> kindDistribution(0, 3);
    for (int round = 0; round < 500; ++round)
    {
        std::vector<std::pair<int, uint64>> operations;
        BitWriter writer;
        for (int step = 0; step < 40; ++step)
        {
            int const kind = kindDistribution(engine);
            uint64 value = engine();
            if (kind == 0)
            {
                uint8 const width = static_cast<uint8>(widthDistribution(engine));
                if (width < 64)
                    value &= (uint64(1) << width) - 1;
                writer.WriteBits(value, width);
                operations.emplace_back(width, value);
            }
            else if (kind == 1)
            {
                writer.Write<uint32>(static_cast<uint32>(value));
                operations.emplace_back(-32, static_cast<uint32>(value));
            }
            else if (kind == 2)
            {
                writer.Write<uint16>(static_cast<uint16>(value));
                operations.emplace_back(-16, static_cast<uint16>(value));
            }
            else
            {
                writer.Write<uint64>(value);
                operations.emplace_back(-64, value);
            }
        }

        BitReader reader(writer.GetBytes());
        for (auto const& [kind, expected] : operations)
        {
            uint64 const actual = kind > 0 ? reader.ReadBits(static_cast<uint8>(kind))
                : kind == -32 ? reader.Read<uint32>()
                : kind == -16 ? reader.Read<uint16>()
                : reader.Read<uint64>();
            ASSERT_EQ(actual, expected) << "round " << round;
        }
        ASSERT_FALSE(reader.Failed());

        std::vector<uint8> const full(writer.GetBytes().begin(), writer.GetBytes().end());
        std::uniform_int_distribution<std::size_t> cutDistribution(0, full.size() - 1);
        std::vector<uint8> const truncated(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(cutDistribution(engine)));
        BitReader partial(truncated);
        for (auto const& [kind, expected] : operations)
        {
            static_cast<void>(expected);
            if (kind > 0)
                partial.ReadBits(static_cast<uint8>(kind));
            else if (kind == -32)
                partial.Read<uint32>();
            else if (kind == -16)
                partial.Read<uint16>();
            else
                partial.Read<uint64>();
        }
        ASSERT_TRUE(partial.Failed()) << "round " << round;
    }
}
