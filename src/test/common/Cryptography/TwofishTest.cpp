/*
 * Project Ambrose by Imjustchico
 * Tests Twofish against the published zero-key and iterated known answers, and OFB mode for identity and keystream carry.
 */

#include "Hex.h"
#include "Twofish.h"
#include "TwofishOfb.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <string>

namespace
{
    std::string ToHex(Twofish::Block const& block)
    {
        return Hex::Encode(block, Hex::Case::Upper);
    }

    std::vector<uint8> RandomBytes(std::size_t count, uint32 seed)
    {
        std::mt19937 engine(seed);
        std::vector<uint8> bytes(count);
        for (uint8& byte : bytes)
            byte = static_cast<uint8>(engine());
        return bytes;
    }
}

TEST(TwofishTest, ZeroKeyAndPlaintextMatchPublishedVectors)
{
    Twofish::Block const zero{};
    EXPECT_EQ(ToHex(Twofish(std::vector<uint8>(16, 0)).EncryptBlock(zero)), "9F589F5CF6122C32B6BFEC2F2AE8C35A");
    EXPECT_EQ(ToHex(Twofish(std::vector<uint8>(24, 0)).EncryptBlock(zero)), "EFA71F788965BD4453F860178FC19101");
    EXPECT_EQ(ToHex(Twofish(std::vector<uint8>(32, 0)).EncryptBlock(zero)), "57FF739D4DC92C1BD7FC01700CC8216F");
}

TEST(TwofishTest, IteratedTableMatchesPublishedVectors)
{
    std::vector<uint8> key(16, 0);
    Twofish::Block plaintext{};
    Twofish::Block ciphertext{};
    std::vector<std::string> ciphertexts;
    for (int i = 1; i <= 49; ++i)
    {
        Twofish const cipher(key);
        ciphertext = cipher.EncryptBlock(plaintext);
        EXPECT_EQ(cipher.DecryptBlock(ciphertext), plaintext) << "iteration " << i;
        ciphertexts.push_back(ToHex(ciphertext));
        key.assign(plaintext.begin(), plaintext.end());
        plaintext = ciphertext;
    }
    EXPECT_EQ(ciphertexts[0], "9F589F5CF6122C32B6BFEC2F2AE8C35A");
    EXPECT_EQ(ciphertexts[1], "D491DB16E7B1C39E86CB086B789F5419");
    EXPECT_EQ(ciphertexts[2], "019F9809DE1711858FAAC3A3BA20FBC3");
    EXPECT_EQ(ciphertexts[48], "5D9D4EEFFA9151575524F115815A12E0");
}

TEST(TwofishTest, RejectsInvalidKeyLengths)
{
    EXPECT_THROW(Twofish(std::vector<uint8>(15, 0)), std::invalid_argument);
    EXPECT_THROW(Twofish(std::vector<uint8>(20, 0)), std::invalid_argument);
    EXPECT_TRUE(Twofish::IsValidKeyLength(32));
}

TEST(TwofishTest, OfbEncryptThenDecryptIsIdentity)
{
    std::vector<uint8> const key = RandomBytes(16, 1);
    std::vector<uint8> const ivBytes = RandomBytes(16, 2);
    std::span<uint8 const, 16> const iv(ivBytes.data(), 16);
    for (std::size_t const length : { std::size_t{ 0 }, std::size_t{ 1 }, std::size_t{ 15 }, std::size_t{ 16 }, std::size_t{ 17 }, std::size_t{ 1000 } })
    {
        std::vector<uint8> const plaintext = RandomBytes(length, static_cast<uint32>(length) + 10);
        std::vector<uint8> const ciphertext = TwofishOfb(key, iv).Process(plaintext);
        ASSERT_EQ(ciphertext.size(), length);
        if (length >= 16)
        {
            EXPECT_NE(ciphertext, plaintext);
        }
        EXPECT_EQ(TwofishOfb(key, iv).Process(ciphertext), plaintext) << length;
    }
}

TEST(TwofishTest, OfbKeystreamIsIteratedEncryptionOfTheIv)
{
    std::vector<uint8> const key = RandomBytes(32, 3);
    Twofish::Block iv{};
    std::iota(iv.begin(), iv.end(), uint8{ 1 });
    Twofish const cipher(key);
    Twofish::Block const first = cipher.EncryptBlock(iv);
    Twofish::Block const second = cipher.EncryptBlock(first);
    std::vector<uint8> const zeros(32, 0);
    std::vector<uint8> const keystream = TwofishOfb(key, iv).Process(zeros);
    EXPECT_TRUE(std::equal(first.begin(), first.end(), keystream.begin()));
    EXPECT_TRUE(std::equal(second.begin(), second.end(), keystream.begin() + 16));
}

TEST(TwofishTest, OfbCarriesPositionAcrossCallsAndRestarts)
{
    std::vector<uint8> const key = RandomBytes(16, 4);
    std::vector<uint8> const ivBytes = RandomBytes(16, 5);
    std::span<uint8 const, 16> const iv(ivBytes.data(), 16);
    std::vector<uint8> const plaintext = RandomBytes(100, 6);
    std::vector<uint8> const whole = TwofishOfb(key, iv).Process(plaintext);

    TwofishOfb split(key, iv);
    std::vector<uint8> pieces;
    for (std::size_t offset = 0; offset < plaintext.size(); offset += 7)
    {
        std::size_t const count = std::min<std::size_t>(7, plaintext.size() - offset);
        std::vector<uint8> const part = split.Process(std::span<uint8 const>(plaintext.data() + offset, count));
        pieces.insert(pieces.end(), part.begin(), part.end());
    }
    EXPECT_EQ(pieces, whole);

    split.Restart(iv);
    std::vector<uint8> inPlace = plaintext;
    split.Apply(inPlace);
    EXPECT_EQ(inPlace, whole);
}
