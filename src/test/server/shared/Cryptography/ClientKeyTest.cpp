/*
 * Project Ambrose by Imjustchico
 * Tests the unpadded decimal login salt, ClientKey1 against a vector computed independently from the formula, rejection of one-character changes, and fresh session keys.
 */

#include "ClientKey.h"

#include <gtest/gtest.h>

#include <set>

namespace
{
    LoginSalt const Salt{ 0x1234, 0xAABBCCDD, 0x0123 };
}

TEST(ClientKeyTest, SaltIsUnpaddedDecimal)
{
    EXPECT_EQ(Salt.ToString(), "46602864434397291");
    EXPECT_EQ((LoginSalt{ 7, 1789404572, 5 }).ToString(), "717894045725");
}

TEST(ClientKeyTest, ClientKey1MatchesAnIndependentVector)
{
    std::string const verifier = ClientKey::HashPassword("hunter2");
    EXPECT_EQ(verifier, "a5ftaNFOs/GqlZzl1Jx9xhLh6x2v1zsecFhHSD/WpsgJ8s606N9v+ZhMYpj/AoXKzmYUv42qnwBwEBtsiYmeIg==");
    std::string const clientKey1 = "YkhClR5RmZ9cES5T516PTdyHwCsSHDkBzG7pdR6H2PWXZJkYu/Ye2hxC24F4grcRBnDPBKI8c6cJBxrxdvgT0w==";
    EXPECT_EQ(ClientKey::ComputeClientKey1(verifier, Salt), clientKey1);
    EXPECT_TRUE(ClientKey::VerifyClientKey1(verifier, Salt, clientKey1));

    EXPECT_FALSE(ClientKey::VerifyClientKey1(ClientKey::HashPassword("hunter3"), Salt, clientKey1));
    EXPECT_EQ(ClientKey::ComputeClientKey1(ClientKey::HashPassword("hunter3"), Salt), "8Ds3FqCmvDHA9Q9ZYb9YpeW7jfHAydhIZt+g5P46BOv5KnSWDcd6Vh08qT1Q0/jxpep89tCFLo5xHuf6i+FKjw==");
    EXPECT_FALSE(ClientKey::VerifyClientKey1(verifier, LoginSalt{ 0x1235, Salt.Seconds, Salt.Milliseconds }, clientKey1));
    EXPECT_FALSE(ClientKey::VerifyClientKey1(verifier, LoginSalt{ Salt.SessionId, Salt.Seconds, 0x0124 }, clientKey1));
    EXPECT_FALSE(ClientKey::VerifyClientKey1(verifier, Salt, clientKey1.substr(0, 87)));
}

TEST(ClientKeyTest, SessionKeysAreFreshAnd44Characters)
{
    std::set<std::string> keys;
    for (int i = 0; i < 16; ++i)
    {
        std::string const key = ClientKey::GenerateSessionKey(Salt);
        EXPECT_EQ(key.size(), 44u);
        keys.insert(key);
    }
    EXPECT_EQ(keys.size(), 16u);
}
