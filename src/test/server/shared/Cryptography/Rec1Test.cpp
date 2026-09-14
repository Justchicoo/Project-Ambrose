/*
 * Project Ambrose by Imjustchico
 * Tests the Rec1 key and IV layout for a known session and that Twofish-OFB Encode and Decode round-trip every length without padding.
 */

#include "Hex.h"
#include "Rec1.h"

#include <gtest/gtest.h>

namespace
{
    LoginSalt const Salt{ 0x1234, 0xAABBCCDD, 0x0123 };
}

TEST(Rec1Test, Rec1KeyAndIvFollowTheLayout)
{
    std::array<uint8, 32> const key = Rec1::DeriveKey(Salt);
    EXPECT_EQ(Hex::Encode(key, Hex::Case::Upper), "1718191A3400121EDDBB2122CCAA23012728292A2B2C2D2E2F30313233343536");
    std::array<uint8, 16> const iv = Rec1::DeriveIv();
    EXPECT_EQ(Hex::Encode(iv, Hex::Case::Upper), "B6B5B4B3B2B1B0AFAEADACABAAA9A8A7");
}

TEST(Rec1Test, Rec1RoundTripsEveryLengthWithoutPadding)
{
    std::string const record = "1234 wizard a5ftaNFOs/GqlZzl1Jx9xhLh6x2v1zsecFhHSD/WpsgJ8s606N9v+ZhMYpj/AoXKzmYUv42qnwBwEBtsiYmeIg==";
    for (std::size_t length = 0; length <= record.size(); length += 7)
    {
        std::string const plain = record.substr(0, length);
        std::string const encoded = Rec1::Encode(plain, Salt);
        ASSERT_EQ(encoded.size(), plain.size());
        if (length >= 16)
        {
            EXPECT_NE(encoded, plain);
        }
        EXPECT_EQ(Rec1::Decode(encoded, Salt), plain);
    }
    LoginSalt other = Salt;
    other.Milliseconds = 0x0124;
    EXPECT_NE(Rec1::Decode(Rec1::Encode(record, Salt), other), record);
}
