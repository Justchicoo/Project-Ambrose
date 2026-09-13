/*
 * Project Ambrose by Imjustchico
 * Tests little-endian layout, scalar round trips, bit-exact floats, patching, and overrun handling.
 */

#include "ByteBuffer.h"

#include <gtest/gtest.h>

#include <bit>
#include <limits>
#include <vector>

namespace
{
    template<typename T>
    void ExpectRoundTrip(T value)
    {
        ByteBuffer buffer;
        buffer.Write(value);
        EXPECT_EQ(buffer.GetSize(), sizeof(T));
        EXPECT_EQ(buffer.Read<T>(), value);
        EXPECT_EQ(buffer.GetRemaining(), 0u);
    }
}

TEST(ByteBufferTest, WritesLittleEndian)
{
    ByteBuffer buffer;
    buffer.Write<uint32>(0x11223344u);
    buffer.Write<uint16>(0xAABBu);
    std::vector<uint8> const expected{ 0x44, 0x33, 0x22, 0x11, 0xBB, 0xAA };
    EXPECT_EQ(std::vector<uint8>(buffer.GetData().begin(), buffer.GetData().end()), expected);
}

TEST(ByteBufferTest, IntegerMinAndMaxRoundTrip)
{
    ExpectRoundTrip(std::numeric_limits<int8>::min());
    ExpectRoundTrip(std::numeric_limits<int8>::max());
    ExpectRoundTrip(std::numeric_limits<uint8>::max());
    ExpectRoundTrip(std::numeric_limits<int16>::min());
    ExpectRoundTrip(std::numeric_limits<uint16>::max());
    ExpectRoundTrip(std::numeric_limits<int32>::min());
    ExpectRoundTrip(std::numeric_limits<uint32>::max());
    ExpectRoundTrip(std::numeric_limits<int64>::min());
    ExpectRoundTrip(std::numeric_limits<uint64>::max());
}

TEST(ByteBufferTest, FloatsRoundTripBitExactIncludingNaN)
{
    float const nan = std::bit_cast<float>(uint32(0x7FC00123u));
    ByteBuffer buffer;
    buffer.Write(nan);
    buffer.Write(-0.0);
    buffer.Write(std::numeric_limits<double>::infinity());
    EXPECT_EQ(std::bit_cast<uint32>(buffer.Read<float>()), 0x7FC00123u);
    EXPECT_EQ(std::bit_cast<uint64>(buffer.Read<double>()), std::bit_cast<uint64>(-0.0));
    EXPECT_EQ(buffer.Read<double>(), std::numeric_limits<double>::infinity());
}

TEST(ByteBufferTest, ReadPastEndThrowsAndKeepsPosition)
{
    ByteBuffer buffer;
    buffer.Write<uint16>(7);
    buffer.Write<uint8>(1);
    EXPECT_EQ(buffer.Read<uint16>(), 7u);
    EXPECT_THROW(buffer.Read<uint32>(), ByteBufferException);
    EXPECT_EQ(buffer.GetReadPosition(), 2u);
    EXPECT_EQ(buffer.Read<uint8>(), 1u);
    EXPECT_THROW(buffer.Read<uint8>(), ByteBufferException);
}

TEST(ByteBufferTest, ExceptionReportsPositionAndSizes)
{
    ByteBuffer buffer(std::vector<uint8>{ 1, 2, 3 });
    buffer.Skip(2);
    try
    {
        buffer.ReadBytes(5);
        FAIL() << "expected ByteBufferException";
    }
    catch (ByteBufferException const& error)
    {
        EXPECT_EQ(error.GetPosition(), 2u);
        EXPECT_EQ(error.GetRequested(), 5u);
        EXPECT_EQ(error.GetSize(), 3u);
    }
}

TEST(ByteBufferTest, PutPatchesInPlaceAndChecksBounds)
{
    ByteBuffer buffer;
    buffer.Write<uint16>(0);
    buffer.Write<uint32>(0xDEADBEEFu);
    buffer.Put<uint16>(0, 0x1234u);
    EXPECT_EQ(buffer.Read<uint16>(), 0x1234u);
    EXPECT_THROW(buffer.Put<uint32>(4, 1u), ByteBufferException);
    EXPECT_THROW(buffer.Put<uint8>(100, 1u), ByteBufferException);
}

TEST(ByteBufferTest, ReadBytesAndSetReadPosition)
{
    ByteBuffer buffer(std::vector<uint8>{ 10, 20, 30, 40 });
    auto const view = buffer.ReadBytes(3);
    ASSERT_EQ(view.size(), 3u);
    EXPECT_EQ(view[2], 30u);
    buffer.SetReadPosition(0);
    EXPECT_EQ(buffer.Read<uint8>(), 10u);
    EXPECT_NO_THROW(buffer.SetReadPosition(4));
    EXPECT_THROW(buffer.SetReadPosition(5), ByteBufferException);
}
