/*
 * Project Ambrose by Imjustchico
 * Tests HMAC-SHA-256 and HMAC-SHA-512 against RFC 4231 vectors, split updates, and constant-time verification.
 */

#include "Hex.h"
#include "Hmac.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    std::span<uint8 const> Bytes(std::string_view text)
    {
        return { reinterpret_cast<uint8 const*>(text.data()), text.size() };
    }

    std::string ToHex(std::vector<uint8> const& bytes)
    {
        return Hex::Encode(bytes, Hex::Case::Lower);
    }
}

TEST(HmacTest, MatchesRfc4231TestCases)
{
    std::vector<uint8> const key1(20, 0x0B);
    EXPECT_EQ(ToHex(Hmac::Compute(CryptoHash::Algorithm::Sha256, key1, Bytes("Hi There"))), "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    EXPECT_EQ(ToHex(Hmac::Compute(CryptoHash::Algorithm::Sha512, key1, Bytes("Hi There"))), "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854");
    EXPECT_EQ(ToHex(Hmac::Compute(CryptoHash::Algorithm::Sha256, Bytes("Jefe"), Bytes("what do ya want for nothing?"))), "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    EXPECT_EQ(ToHex(Hmac::Compute(CryptoHash::Algorithm::Sha512, Bytes("Jefe"), Bytes("what do ya want for nothing?"))), "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737");
}

TEST(HmacTest, MovedFromHmacThrowsInsteadOfCrashing)
{
    Hmac original(CryptoHash::Algorithm::Sha512, Bytes("key"));
    Hmac moved = std::move(original);
    EXPECT_EQ(moved.Update("data").Finalize().size(), 64u);
    EXPECT_THROW(original.Update("data"), std::logic_error);
    EXPECT_THROW(original.Finalize(), std::logic_error);
}

TEST(HmacTest, SplitUpdatesAndVerify)
{
    Hmac hmac(CryptoHash::Algorithm::Sha256, Bytes("Jefe"));
    std::vector<uint8> const tag = hmac.Update("what do ya ").Update("want for nothing?").Finalize();
    EXPECT_EQ(ToHex(tag), "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    EXPECT_EQ(hmac.GetLength(), 32u);
    EXPECT_TRUE(Hmac::Verify(CryptoHash::Algorithm::Sha256, Bytes("Jefe"), Bytes("what do ya want for nothing?"), tag));
    std::vector<uint8> tampered = tag;
    tampered.back() ^= 1;
    EXPECT_FALSE(Hmac::Verify(CryptoHash::Algorithm::Sha256, Bytes("Jefe"), Bytes("what do ya want for nothing?"), tampered));
    EXPECT_FALSE(Hmac::Verify(CryptoHash::Algorithm::Sha256, Bytes("Jefe"), Bytes("what do ya want for nothing?"), std::span<uint8 const>(tag.data(), 31)));
}
