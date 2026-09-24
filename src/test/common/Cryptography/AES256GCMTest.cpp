/*
 * Project Ambrose by Imjustchico
 * Tests AES-256-GCM against the published GCM test cases 13 to 16, round trips with random nonces, and rejection of a changed tag, ciphertext, nonce, key or associated data.
 */

#include "AES256GCM.h"
#include "Hex.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace
{
    std::vector<uint8> FromHex(std::string_view text)
    {
        std::optional<std::vector<uint8>> bytes = Hex::Decode(text);
        EXPECT_TRUE(bytes) << text;
        return bytes ? *bytes : std::vector<uint8>();
    }

    AES256GCM::Key KeyFromHex(std::string_view text)
    {
        std::vector<uint8> const bytes = FromHex(text);
        AES256GCM::Key key{};
        std::copy_n(bytes.begin(), std::min(bytes.size(), key.size()), key.begin());
        return key;
    }

    AES256GCM::Nonce NonceFromHex(std::string_view text)
    {
        std::vector<uint8> const bytes = FromHex(text);
        AES256GCM::Nonce nonce{};
        std::copy_n(bytes.begin(), std::min(bytes.size(), nonce.size()), nonce.begin());
        return nonce;
    }

    std::string SealedHex(AES256GCM::Key const& key, AES256GCM::Nonce const& nonce, std::span<uint8 const> plaintext, std::span<uint8 const> associatedData)
    {
        std::vector<uint8> const sealed = AES256GCM::SealWithNonce(key, nonce, plaintext, associatedData);
        return Hex::Encode(std::span<uint8 const>(sealed).subspan(AES256GCM::NonceSize));
    }
}

TEST(AES256GCMTest, MatchesPublishedTestCases)
{
    AES256GCM::Key const zeroKey{};
    AES256GCM::Nonce const zeroNonce{};
    EXPECT_EQ(SealedHex(zeroKey, zeroNonce, {}, {}), "530f8afbc74536b9a963b4f1c4cb738b");
    std::vector<uint8> const zeroBlock(16, 0);
    EXPECT_EQ(SealedHex(zeroKey, zeroNonce, zeroBlock, {}), "cea7403d4d606b6e074ec5d3baf39d18d0d1c8a799996bf0265b98b5d48ab919");

    AES256GCM::Key const key = KeyFromHex("feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308");
    AES256GCM::Nonce const nonce = NonceFromHex("cafebabefacedbaddecaf888");
    std::vector<uint8> const plaintext = FromHex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255");
    EXPECT_EQ(SealedHex(key, nonce, plaintext, {}),
        "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015adb094dac5d93471bdec1a502270e3cc6c");
    std::vector<uint8> const associatedData = FromHex("feedfacedeadbeeffeedfacedeadbeefabaddad2");
    EXPECT_EQ(SealedHex(key, nonce, std::span<uint8 const>(plaintext).first(60), associatedData),
        "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f66276fc6ece0f4e1768cddf8853bb2d551b");
}

TEST(AES256GCMTest, RoundTripsWithFreshNoncesAndRejectsTampering)
{
    AES256GCM::Key const key = KeyFromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    std::string const secret = "a5ftaNFOs/GqlZzl1Jx9xhLh6x2v1zsecFhHSD/WpsgJ8s606N9v+ZhMYpj/AoXKzmYUv42qnwBwEBtsiYmeIg==";
    std::span<uint8 const> const plaintext(reinterpret_cast<uint8 const*>(secret.data()), secret.size());
    std::string const context = "wizard";
    std::span<uint8 const> const associatedData(reinterpret_cast<uint8 const*>(context.data()), context.size());

    std::vector<uint8> const sealed = AES256GCM::Seal(key, plaintext, associatedData);
    ASSERT_EQ(sealed.size(), AES256GCM::NonceSize + secret.size() + AES256GCM::TagSize);
    EXPECT_NE(AES256GCM::Seal(key, plaintext, associatedData), sealed);
    std::optional<std::vector<uint8>> const opened = AES256GCM::Open(key, sealed, associatedData);
    ASSERT_TRUE(opened);
    EXPECT_EQ(std::string(opened->begin(), opened->end()), secret);

    for (std::size_t const index : { std::size_t{ 0 }, AES256GCM::NonceSize, sealed.size() - 1 })
    {
        std::vector<uint8> tampered = sealed;
        tampered[index] ^= 0x01;
        EXPECT_FALSE(AES256GCM::Open(key, tampered, associatedData)) << index;
    }
    std::string const otherContext = "Wizard";
    EXPECT_FALSE(AES256GCM::Open(key, sealed, std::span<uint8 const>(reinterpret_cast<uint8 const*>(otherContext.data()), otherContext.size())));
    AES256GCM::Key otherKey = key;
    otherKey[31] ^= 0x80;
    EXPECT_FALSE(AES256GCM::Open(otherKey, sealed, associatedData));
    EXPECT_FALSE(AES256GCM::Open(key, std::span<uint8 const>(sealed).first(AES256GCM::NonceSize + AES256GCM::TagSize - 1), associatedData));
}
