/*
 * Project Ambrose by Imjustchico
 * Tests the verifier key ring offline: key list parsing and its errors without key material, plain storage with no active key, sealing bound to the username, and opening with older keys.
 */

#include "ClientKey.h"
#include "VerifierKeyRing.h"

#include <gtest/gtest.h>

namespace
{
    std::string const KeyOne = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    std::string const KeyTwo = "F0E1D2C3B4A5968778695A4B3C2D1E0FF0E1D2C3B4A5968778695A4B3C2D1E0F";
}

TEST(VerifierKeyRingTest, ParsesKeyListsAndReportsProblemsWithoutKeys)
{
    std::string error;
    std::optional<VerifierKeyRing> const empty = VerifierKeyRing::Parse("", 0, error);
    ASSERT_TRUE(empty) << error;
    EXPECT_EQ(empty->GetActiveKeyId(), 0);
    EXPECT_EQ(empty->GetKeyCount(), 0u);

    std::optional<VerifierKeyRing> const two = VerifierKeyRing::Parse(" 1:" + KeyOne + " , 2 : " + KeyTwo + ",", 2, error);
    ASSERT_TRUE(two) << error;
    EXPECT_EQ(two->GetActiveKeyId(), 2);
    EXPECT_EQ(two->GetKeyCount(), 2u);
    EXPECT_TRUE(two->HasKey(0));
    EXPECT_TRUE(two->HasKey(1));
    EXPECT_FALSE(two->HasKey(3));

    struct BadCase
    {
        std::string Keys;
        uint32 Active;
        std::string Expected;
    };
    for (BadCase const& bad : std::initializer_list<BadCase>{
        { KeyOne, 0, "each verifier key must be written id:hex with an id from 1 to 255" },
        { "0:" + KeyOne, 0, "each verifier key must be written id:hex with an id from 1 to 255" },
        { "256:" + KeyOne, 0, "each verifier key must be written id:hex with an id from 1 to 255" },
        { "1:" + KeyOne + ",1:" + KeyTwo, 1, "verifier key 1 is listed twice" },
        { "1:" + KeyOne.substr(2), 1, "verifier key 1 must be 64 hex digits" },
        { "1:zz" + KeyOne.substr(2), 1, "verifier key 1 must be 64 hex digits" },
        { "1:" + KeyOne, 2, "the active verifier key 2 is not in the key list" },
        { "", 1, "the active verifier key 1 is not in the key list" },
        { "", 256, "the active verifier key 256 is not in the key list" } })
    {
        error.clear();
        EXPECT_FALSE(VerifierKeyRing::Parse(bad.Keys, bad.Active, error)) << bad.Keys;
        EXPECT_EQ(error, bad.Expected) << bad.Keys;
        EXPECT_EQ(error.find(KeyOne.substr(0, 16)), std::string::npos) << error;
        EXPECT_EQ(error.find(KeyOne.substr(2, 16)), std::string::npos) << error;
        EXPECT_EQ(error.find(KeyTwo.substr(0, 16)), std::string::npos) << error;
    }
}

TEST(VerifierKeyRingTest, SealsWithTheActiveKeyAndOpensWithAnyListedKey)
{
    std::string const verifier = ClientKey::HashPassword("hunter2");
    std::string error;
    std::optional<VerifierKeyRing> const plain = VerifierKeyRing::Parse("1:" + KeyOne, 0, error);
    ASSERT_TRUE(plain) << error;
    VerifierKeyRing::SealedVerifier const unsealed = plain->Seal(verifier, "Wizard");
    EXPECT_EQ(unsealed.KeyId, 0);
    EXPECT_EQ(unsealed.Stored, verifier);
    EXPECT_EQ(plain->Open(unsealed.Stored, 0, "Wizard"), verifier);

    std::optional<VerifierKeyRing> const first = VerifierKeyRing::Parse("1:" + KeyOne, 1, error);
    ASSERT_TRUE(first) << error;
    VerifierKeyRing::SealedVerifier const sealed = first->Seal(verifier, "Wizard");
    EXPECT_EQ(sealed.KeyId, 1);
    EXPECT_EQ(sealed.Stored.size(), 156u);
    EXPECT_EQ(sealed.Stored.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="), std::string::npos);
    EXPECT_EQ(sealed.Stored.find(verifier.substr(0, 16)), std::string::npos);
    EXPECT_NE(first->Seal(verifier, "Wizard").Stored, sealed.Stored);
    EXPECT_EQ(first->Open(sealed.Stored, 1, "Wizard"), verifier);
    EXPECT_EQ(first->Open(sealed.Stored, 1, "WIZARD"), verifier);
    EXPECT_FALSE(first->Open(sealed.Stored, 1, "Wizard2"));
    EXPECT_FALSE(first->Open(sealed.Stored, 2, "Wizard"));
    EXPECT_FALSE(first->Open("not base64!", 1, "Wizard"));

    std::optional<VerifierKeyRing> const rotated = VerifierKeyRing::Parse("1:" + KeyOne + ",2:" + KeyTwo, 2, error);
    ASSERT_TRUE(rotated) << error;
    EXPECT_EQ(rotated->Open(sealed.Stored, 1, "Wizard"), verifier);
    VerifierKeyRing::SealedVerifier const resealed = rotated->Seal(verifier, "Wizard");
    EXPECT_EQ(resealed.KeyId, 2);
    EXPECT_FALSE(first->Open(resealed.Stored, 2, "Wizard"));
    EXPECT_FALSE(rotated->Open(resealed.Stored, 1, "Wizard"));

    VerifierKeyRing copy = *rotated;
    copy = *first;
    EXPECT_EQ(copy.GetKeyCount(), 1u);
    EXPECT_EQ(copy.Open(sealed.Stored, 1, "wizard"), verifier);
}
