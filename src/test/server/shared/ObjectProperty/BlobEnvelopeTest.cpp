/*
 * Project Ambrose by Imjustchico
 * Tests the ObjectProperty blob envelope: a stored header of 0x8000000A for ten bytes, stored and compressed round trips including empty payloads, lengths that disagree with the payload, trailing bytes, truncated input, a bad header or checksum, the packing reported on failures, and declared sizes above the cap or beyond what the input can produce refused without allocating them.
 */

#include "AllocationCounter.h"
#include "BlobEnvelope.h"
#include "Compression.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
    std::vector<uint8> Sequence(std::size_t size)
    {
        std::vector<uint8> bytes(size);
        for (std::size_t i = 0; i < size; ++i)
            bytes[i] = static_cast<uint8>(i * 7 + 3);
        return bytes;
    }

    std::vector<uint8> Header(uint32 value)
    {
        return { static_cast<uint8>(value & 0xFF), static_cast<uint8>((value >> 8) & 0xFF), static_cast<uint8>((value >> 16) & 0xFF), static_cast<uint8>((value >> 24) & 0xFF) };
    }
}

TEST(BlobEnvelopeTest, AStoredWrapOfTenBytesHasHeader8000000A)
{
    std::vector<uint8> const payload = Sequence(10);
    std::vector<uint8> const wrapped = BlobEnvelope::Wrap(payload, BlobEnvelope::Packing::Store);
    ASSERT_EQ(wrapped.size(), 14u);
    EXPECT_EQ(std::vector<uint8>(wrapped.begin(), wrapped.begin() + 4), (std::vector<uint8>{ 0x0A, 0x00, 0x00, 0x80 }));
    EXPECT_EQ(std::vector<uint8>(wrapped.begin() + 4, wrapped.end()), payload);

    BlobEnvelope::UnwrapResult const unwrapped = BlobEnvelope::Unwrap(wrapped, 1024);
    ASSERT_TRUE(unwrapped.Succeeded()) << BlobEnvelope::GetStatusName(unwrapped.Code);
    EXPECT_EQ(unwrapped.Packed, BlobEnvelope::Packing::Store);
    EXPECT_EQ(unwrapped.Data, payload);

    BlobEnvelope::UnwrapResult const empty = BlobEnvelope::Unwrap(BlobEnvelope::Wrap({}, BlobEnvelope::Packing::Store), 0);
    ASSERT_TRUE(empty.Succeeded());
    EXPECT_TRUE(empty.Data.empty());
    BlobEnvelope::UnwrapResult const emptyCompressed = BlobEnvelope::Unwrap(BlobEnvelope::Wrap({}, BlobEnvelope::Packing::Compress), 0);
    ASSERT_TRUE(emptyCompressed.Succeeded()) << BlobEnvelope::GetStatusName(emptyCompressed.Code);
    EXPECT_EQ(emptyCompressed.Packed, BlobEnvelope::Packing::Compress);
    EXPECT_TRUE(emptyCompressed.Data.empty());
}

TEST(BlobEnvelopeTest, CompressedWrapsRoundTripAndMismatchedSizesAreRefused)
{
    std::vector<uint8> const payload = Sequence(5000);
    std::vector<uint8> const wrapped = BlobEnvelope::Wrap(payload, BlobEnvelope::Packing::Compress);
    EXPECT_EQ(std::vector<uint8>(wrapped.begin(), wrapped.begin() + 4), Header(5000));
    BlobEnvelope::UnwrapResult const unwrapped = BlobEnvelope::Unwrap(wrapped, 5000);
    ASSERT_TRUE(unwrapped.Succeeded()) << BlobEnvelope::GetStatusName(unwrapped.Code);
    EXPECT_EQ(unwrapped.Packed, BlobEnvelope::Packing::Compress);
    EXPECT_EQ(unwrapped.Data, payload);

    std::vector<uint8> smaller = wrapped;
    smaller[0] = static_cast<uint8>((4999 & 0xFF));
    smaller[1] = static_cast<uint8>((4999 >> 8) & 0xFF);
    EXPECT_EQ(BlobEnvelope::Unwrap(smaller, 10000).Code, BlobEnvelope::Status::SizeMismatch);
    std::vector<uint8> larger = wrapped;
    larger[0] = static_cast<uint8>((5001 & 0xFF));
    larger[1] = static_cast<uint8>((5001 >> 8) & 0xFF);
    EXPECT_EQ(BlobEnvelope::Unwrap(larger, 10000).Code, BlobEnvelope::Status::SizeMismatch);

    std::vector<uint8> storedLonger = BlobEnvelope::Wrap(Sequence(10), BlobEnvelope::Packing::Store);
    storedLonger.push_back(0);
    EXPECT_EQ(BlobEnvelope::Unwrap(storedLonger, 1024).Code, BlobEnvelope::Status::SizeMismatch);

    std::vector<uint8> trailing = wrapped;
    trailing.push_back(0);
    BlobEnvelope::UnwrapResult const trailed = BlobEnvelope::Unwrap(trailing, 10000);
    EXPECT_EQ(trailed.Code, BlobEnvelope::Status::TrailingData);
    EXPECT_EQ(trailed.Packed, BlobEnvelope::Packing::Compress);
}

TEST(BlobEnvelopeTest, TruncatedAndCorruptInputIsRefused)
{
    EXPECT_EQ(BlobEnvelope::Unwrap(std::vector<uint8>{ 0x01, 0x00, 0x00 }, 1024).Code, BlobEnvelope::Status::Truncated);
    std::vector<uint8> stored = BlobEnvelope::Wrap(Sequence(10), BlobEnvelope::Packing::Store);
    stored.pop_back();
    EXPECT_EQ(BlobEnvelope::Unwrap(stored, 1024).Code, BlobEnvelope::Status::Truncated);

    std::vector<uint8> compressed = BlobEnvelope::Wrap(Sequence(5000), BlobEnvelope::Packing::Compress);
    std::vector<uint8> cut(compressed.begin(), compressed.begin() + static_cast<std::ptrdiff_t>(compressed.size() / 2));
    EXPECT_EQ(BlobEnvelope::Unwrap(cut, 5000).Code, BlobEnvelope::Status::Truncated);

    std::vector<uint8> garbage = Header(100);
    for (int i = 0; i < 40; ++i)
        garbage.push_back(0xAB);
    EXPECT_EQ(BlobEnvelope::Unwrap(garbage, 1024).Code, BlobEnvelope::Status::Corrupt);

    std::vector<uint8> badChecksum = BlobEnvelope::Wrap(Sequence(5000), BlobEnvelope::Packing::Compress);
    badChecksum.back() ^= 0x01;
    EXPECT_EQ(BlobEnvelope::Unwrap(badChecksum, 5000).Code, BlobEnvelope::Status::Corrupt);
    EXPECT_EQ(BlobEnvelope::Unwrap(stored, 1024).Packed, BlobEnvelope::Packing::Store);
}

TEST(BlobEnvelopeTest, ADeclaredSizeAboveTheCapIsRefusedWithoutAllocating)
{
    std::vector<uint8> const huge = Header(0x7FFFFFF0u);
    std::vector<uint8> const hugeStored = Header(0xFFFFFFF0u);
    BlobEnvelope::Status compressedStatus = BlobEnvelope::Status::Ok;
    BlobEnvelope::Status storedStatus = BlobEnvelope::Status::Ok;
    std::size_t largest = 0;
    {
        AllocationScope const allocations;
        compressedStatus = BlobEnvelope::Unwrap(huge, 1u << 20).Code;
        storedStatus = BlobEnvelope::Unwrap(hugeStored, 1u << 20).Code;
        largest = allocations.GetLargest();
    }
    EXPECT_EQ(compressedStatus, BlobEnvelope::Status::TooLarge);
    EXPECT_EQ(storedStatus, BlobEnvelope::Status::TooLarge);
    if (AllocationScope::IsSupported())
    {
        EXPECT_LT(largest, 1024u);
    }
    EXPECT_EQ(BlobEnvelope::Unwrap(BlobEnvelope::Wrap(Sequence(11), BlobEnvelope::Packing::Store), 10).Code, BlobEnvelope::Status::TooLarge);

    std::vector<uint8> declaredLarge = Header(uint32{ 16 } << 20);
    declaredLarge.insert(declaredLarge.end(), 16 * 1024, 0xAB);
    BlobEnvelope::Status declaredStatus = BlobEnvelope::Status::Ok;
    std::size_t declaredLargest = 0;
    {
        AllocationScope const allocations;
        declaredStatus = BlobEnvelope::Unwrap(declaredLarge, std::size_t{ 16 } << 20).Code;
        declaredLargest = allocations.GetLargest();
    }
    EXPECT_EQ(declaredStatus, BlobEnvelope::Status::Corrupt);
    if (AllocationScope::IsSupported())
    {
        EXPECT_LE(declaredLargest, std::size_t{ 256 } * 1024);
    }
    EXPECT_EQ(BlobEnvelope::Unwrap(BlobEnvelope::Wrap(Sequence(11), BlobEnvelope::Packing::Compress), 10).Code, BlobEnvelope::Status::TooLarge);
}
