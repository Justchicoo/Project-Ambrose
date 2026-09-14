/*
 * Project Ambrose by Imjustchico
 * Tests zlib, gzip, and raw round trips, the output cap that stops zip bombs, and corrupt, truncated, or mismatched streams.
 */

#include "Compression.h"

#include <gtest/gtest.h>

#include <random>
#include <string>

using namespace Ambrose::Compression;

namespace
{
    std::vector<uint8> Text(std::string_view text)
    {
        return std::vector<uint8>(text.begin(), text.end());
    }

    std::vector<uint8> Noise(std::size_t count)
    {
        std::mt19937 engine(42);
        std::vector<uint8> bytes(count);
        for (uint8& byte : bytes)
            byte = static_cast<uint8>(engine());
        return bytes;
    }
}

TEST(CompressionTest, RoundTripsEveryFormat)
{
    std::vector<uint8> const input = Text("Ravenwood Ravenwood Ravenwood Unicorn Way Triton Avenue Firecat Alley");
    for (Format const format : { Format::Zlib, Format::Gzip, Format::Raw })
    {
        std::vector<uint8> const compressed = Deflate(input, 9, format);
        InflateResult const result = Inflate(compressed, input.size(), format);
        ASSERT_TRUE(result.Succeeded()) << GetStatusName(result.Code);
        EXPECT_EQ(result.Data, input);
        EXPECT_EQ(result.ConsumedInput, compressed.size());
    }
    std::vector<uint8> const gzip = Deflate(input, 6, Format::Gzip);
    EXPECT_EQ(Inflate(gzip, 1000, Format::Auto).Data, input);
    EXPECT_EQ(Inflate(Deflate(input), 1000, Format::Auto).Data, input);
}

TEST(CompressionTest, LargeAndIncompressibleDataRoundTrip)
{
    std::vector<uint8> const noise = Noise(3 * 1024 * 1024 + 17);
    InflateResult const result = InflateExact(Deflate(noise, 1), noise.size());
    ASSERT_TRUE(result.Succeeded()) << GetStatusName(result.Code);
    EXPECT_EQ(result.Data, noise);
}

TEST(CompressionTest, InflatePastCapFailsCleanly)
{
    std::vector<uint8> const zeros(64 * 1024 * 1024, 0);
    std::vector<uint8> const bomb = Deflate(zeros, 9);
    ASSERT_LT(bomb.size(), 100000u);
    InflateResult const capped = Inflate(bomb, 1024 * 1024);
    EXPECT_EQ(capped.Code, Status::OutputTooLarge);
    EXPECT_TRUE(capped.Data.empty());
    std::vector<uint8> const small = Deflate(Text("12345"));
    EXPECT_EQ(Inflate(small, 4).Code, Status::OutputTooLarge);
    EXPECT_TRUE(Inflate(small, 5).Succeeded());
    EXPECT_EQ(Inflate(Deflate({}), 0).Code, Status::Ok);
}

TEST(CompressionTest, ExactSizeMismatchIsReported)
{
    std::vector<uint8> const compressed = Deflate(Text("exactly twenty bytes"));
    EXPECT_TRUE(InflateExact(compressed, 20).Succeeded());
    EXPECT_EQ(InflateExact(compressed, 21).Code, Status::SizeMismatch);
    EXPECT_EQ(InflateExact(compressed, 19).Code, Status::OutputTooLarge);
}

TEST(CompressionTest, CorruptTruncatedAndTrailingInputAreRejected)
{
    std::vector<uint8> const input = Noise(5000);
    std::vector<uint8> compressed = Deflate(input);

    std::vector<uint8> const truncated(compressed.begin(), compressed.begin() + static_cast<std::ptrdiff_t>(compressed.size() / 2));
    EXPECT_EQ(Inflate(truncated, input.size()).Code, Status::Truncated);

    std::vector<uint8> corrupt = compressed;
    corrupt[0] = 0x00;
    EXPECT_EQ(Inflate(corrupt, input.size()).Code, Status::Corrupt);

    std::vector<uint8> trailing = compressed;
    trailing.push_back(0xAB);
    EXPECT_EQ(Inflate(trailing, input.size()).Code, Status::TrailingData);
    InflateResult const allowed = Inflate(trailing, input.size(), Format::Zlib, true);
    ASSERT_TRUE(allowed.Succeeded());
    EXPECT_EQ(allowed.ConsumedInput, compressed.size());

    EXPECT_EQ(Inflate({}, 10).Code, Status::Truncated);
    uint8 const garbage[] = { 'n', 'o', 't', ' ', 'z', 'l', 'i', 'b' };
    EXPECT_EQ(Inflate(garbage, 10).Code, Status::Corrupt);
}

TEST(CompressionTest, TruncatedChecksumsAndHuffmanStreamsAreRejected)
{
    std::string text;
    for (int i = 0; i < 400; ++i)
        text += "The Headmaster greets young wizard number " + std::to_string(i) + " in Ravenwood. ";
    std::vector<uint8> const input(text.begin(), text.end());
    std::vector<uint8> const zlibStream = Deflate(input, 9, Format::Zlib);
    for (std::size_t cut = 1; cut <= 4; ++cut)
    {
        std::vector<uint8> const shortened(zlibStream.begin(), zlibStream.end() - static_cast<std::ptrdiff_t>(cut));
        EXPECT_EQ(Inflate(shortened, input.size()).Code, Status::Truncated) << "zlib missing " << cut;
    }
    std::vector<uint8> const gzipStream = Deflate(input, 9, Format::Gzip);
    for (std::size_t cut = 1; cut <= 8; cut += 3)
    {
        std::vector<uint8> const shortened(gzipStream.begin(), gzipStream.end() - static_cast<std::ptrdiff_t>(cut));
        EXPECT_EQ(Inflate(shortened, input.size(), Format::Gzip).Code, Status::Truncated) << "gzip missing " << cut;
    }
    std::vector<uint8> const midHuffman(zlibStream.begin(), zlibStream.begin() + static_cast<std::ptrdiff_t>(zlibStream.size() / 3));
    EXPECT_EQ(Inflate(midHuffman, input.size()).Code, Status::Truncated);
    std::vector<uint8> badChecksum = zlibStream;
    badChecksum.back() ^= 0x01;
    EXPECT_EQ(Inflate(badChecksum, input.size()).Code, Status::Corrupt);
}
