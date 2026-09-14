/*
 * Project Ambrose by Imjustchico
 * Tests SHA-256 and SHA-512 against FIPS 180-4 vectors, split updates, reuse after finalize, and integer updates.
 */

#include "Hex.h"
#include "SHA256.h"
#include "SHA512.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
    template<typename Digest>
    std::string ToHex(Digest const& digest)
    {
        return Hex::Encode(std::span<uint8 const>(digest.data(), digest.size()), Hex::Case::Lower);
    }

    std::string const TwoBlock256 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    std::string const TwoBlock512 = "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
}

TEST(ShaTest, Sha256MatchesFips180Vectors)
{
    EXPECT_EQ(ToHex(SHA256::GetDigestOf("abc")), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(ToHex(SHA256::GetDigestOf("")), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(ToHex(SHA256::GetDigestOf(TwoBlock256)), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    EXPECT_EQ(ToHex(SHA256::GetDigestOf(std::string(1000000, 'a'))), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(ShaTest, Sha512MatchesFips180Vectors)
{
    EXPECT_EQ(ToHex(SHA512::GetDigestOf("abc")), "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    EXPECT_EQ(ToHex(SHA512::GetDigestOf("")), "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
    EXPECT_EQ(ToHex(SHA512::GetDigestOf(TwoBlock512)), "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
    EXPECT_EQ(ToHex(SHA512::GetDigestOf(std::string(1000000, 'a'))), "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b");
}

TEST(ShaTest, IncrementalUpdatesEqualOneShot)
{
    for (std::size_t split = 0; split <= TwoBlock512.size(); ++split)
    {
        SHA256 sha256;
        sha256.Update(std::string_view(TwoBlock512).substr(0, split)).Update(std::string_view(TwoBlock512).substr(split));
        ASSERT_EQ(sha256.Finalize(), SHA256::GetDigestOf(TwoBlock512)) << split;
        SHA512 sha512;
        sha512.Update(std::string_view(TwoBlock512).substr(0, split)).Update(std::string_view(TwoBlock512).substr(split));
        ASSERT_EQ(sha512.Finalize(), SHA512::GetDigestOf(TwoBlock512)) << split;
    }
}

TEST(ShaTest, FinalizeResetsForReuse)
{
    SHA256 hash;
    hash.Update("abc");
    SHA256::Digest const first = hash.Finalize();
    hash.Update("abc");
    EXPECT_EQ(hash.Finalize(), first);
    hash.Update("discarded");
    hash.Reset();
    EXPECT_EQ(ToHex(hash.Update("abc").Finalize()), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(ShaTest, IntegersAreHashedLittleEndian)
{
    SHA256 integers;
    integers.UpdateInteger(uint32{ 0x64636261 });
    uint8 const bytes[] = { 'a', 'b', 'c', 'd' };
    EXPECT_EQ(integers.Finalize(), SHA256::GetDigestOf(std::span<uint8 const>(bytes)));
    SHA512 negative;
    negative.UpdateInteger(int16{ -1 });
    uint8 const allOnes[] = { 0xFF, 0xFF };
    EXPECT_EQ(negative.Finalize(), SHA512::GetDigestOf(std::span<uint8 const>(allOnes)));
}

TEST(ShaTest, MovedFromHashIsUsableAgain)
{
    SHA256 original;
    original.Update("ab");
    SHA256 moved = std::move(original);
    EXPECT_EQ(ToHex(moved.Update("c").Finalize()), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    original.Reset();
    EXPECT_EQ(ToHex(original.Update("abc").Finalize()), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(ShaTest, GenericHashRejectsWrongDigestBuffer)
{
    CryptoHash hash(CryptoHash::Algorithm::Sha512);
    EXPECT_EQ(hash.GetDigestLength(), 64u);
    uint8 small[32] = {};
    EXPECT_THROW(hash.Finalize(small), std::invalid_argument);
}
