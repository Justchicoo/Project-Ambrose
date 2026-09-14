/*
 * Project Ambrose by Imjustchico
 * Tests both CRC-32 variants against known answers, a bitwise reference, split updates, and CRC combination.
 */

#include "Crc32.h"

#include <gtest/gtest.h>

#include <random>
#include <vector>

namespace
{
    uint32 BitwiseReference(std::span<uint8 const> data, uint32 state)
    {
        for (uint8 const byte : data)
        {
            state ^= byte;
            for (int bit = 0; bit < 8; ++bit)
                state = (state & 1u) != 0 ? (state >> 1) ^ 0xEDB88320u : state >> 1;
        }
        return state;
    }

    std::vector<uint8> RandomBytes(std::size_t count, uint32 seed)
    {
        std::mt19937 engine(seed);
        std::uniform_int_distribution<int> distribution(0, 255);
        std::vector<uint8> bytes(count);
        for (uint8& byte : bytes)
            byte = static_cast<uint8>(distribution(engine));
        return bytes;
    }
}

TEST(Crc32Test, ClientVariantUsesInitZeroAndNoFinalXor)
{
    EXPECT_EQ(Crc32::ComputeClient("123456789"), 0x2DFD2D88u);
    EXPECT_NE(Crc32::ComputeClient("123456789"), 0xCBF43926u);
    EXPECT_EQ(Crc32::Client().Update("123456789").GetValue(), 0x2DFD2D88u);
    EXPECT_EQ(Crc32::ComputeClient(""), 0u);
}

TEST(Crc32Test, StandardVariantMatchesZlib)
{
    EXPECT_EQ(Crc32::ComputeStandard("123456789"), 0xCBF43926u);
    EXPECT_EQ(Crc32(Crc32::StandardInitial, Crc32::StandardFinalXor).Update("123456789").GetValue(), 0xCBF43926u);
    EXPECT_EQ(Crc32::ComputeStandard("The quick brown fox jumps over the lazy dog"), 0x414FA339u);
    EXPECT_EQ(Crc32::ComputeStandard(""), 0u);
}

TEST(Crc32Test, SlicingMatchesBitwiseReferenceForEveryLength)
{
    std::vector<uint8> const data = RandomBytes(300, 1234);
    for (std::size_t length = 0; length <= data.size(); ++length)
    {
        std::span<uint8 const> const slice(data.data(), length);
        ASSERT_EQ(Crc32::ComputeClient(slice), BitwiseReference(slice, 0)) << "length " << length;
        ASSERT_EQ(Crc32::ComputeStandard(slice), ~BitwiseReference(slice, 0xFFFFFFFFu)) << "length " << length;
    }
}

TEST(Crc32Test, IncrementalUpdatesEqualOneShot)
{
    std::vector<uint8> const data = RandomBytes(4096, 99);
    uint32 const clientExpected = Crc32::ComputeClient(data);
    uint32 const standardExpected = Crc32::ComputeStandard(data);
    for (std::size_t split : { std::size_t{ 0 }, std::size_t{ 1 }, std::size_t{ 7 }, std::size_t{ 8 }, std::size_t{ 9 }, std::size_t{ 1000 }, std::size_t{ 4095 }, std::size_t{ 4096 } })
    {
        Crc32 client = Crc32::Client();
        client.Update(std::span<uint8 const>(data.data(), split)).Update(std::span<uint8 const>(data.data() + split, data.size() - split));
        EXPECT_EQ(client.GetValue(), clientExpected) << "split " << split;
        Crc32 standard = Crc32::Standard();
        standard.Update(std::span<uint8 const>(data.data(), split)).Update(std::span<uint8 const>(data.data() + split, data.size() - split));
        EXPECT_EQ(standard.GetValue(), standardExpected) << "split " << split;
    }
    Crc32 bytewise = Crc32::Client();
    for (uint8 const byte : data)
        bytewise.Update(std::span<uint8 const>(&byte, 1));
    EXPECT_EQ(bytewise.GetValue(), clientExpected);
    bytewise.Reset();
    EXPECT_EQ(bytewise.GetValue(), 0u);
}

TEST(Crc32Test, CombineJoinsTwoIndependentChecksums)
{
    std::vector<uint8> const data = RandomBytes(5000, 7);
    for (std::size_t split : { std::size_t{ 0 }, std::size_t{ 1 }, std::size_t{ 333 }, std::size_t{ 4999 }, std::size_t{ 5000 } })
    {
        std::span<uint8 const> const first(data.data(), split);
        std::span<uint8 const> const second(data.data() + split, data.size() - split);
        EXPECT_EQ(Crc32::Combine(Crc32::ComputeClient(first), Crc32::ComputeClient(second), second.size()), Crc32::ComputeClient(data)) << split;
        EXPECT_EQ(Crc32::Combine(Crc32::ComputeStandard(first), Crc32::ComputeStandard(second), second.size()), Crc32::ComputeStandard(data)) << split;
        for (auto const& [initial, finalXor] :{ std::pair<uint32, uint32>{ 0xFFFFFFFFu, 0u }, std::pair<uint32, uint32>{ 0x12345678u, 0u }, std::pair<uint32, uint32>{ 0u, 0xFFFFFFFFu } })
        {
            Crc32 const variant(initial, finalXor);
            uint32 const firstValue = Crc32(initial, finalXor).Update(first).GetValue();
            uint32 const secondValue = Crc32(initial, finalXor).Update(second).GetValue();
            uint32 const wholeValue = Crc32(initial, finalXor).Update(data).GetValue();
            EXPECT_EQ(variant.CombineWith(firstValue, secondValue, second.size()), wholeValue) << split << " init " << initial << " xor " << finalXor;
        }
    }
}
