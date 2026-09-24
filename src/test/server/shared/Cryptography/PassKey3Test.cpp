/*
 * Project Ambrose by Imjustchico
 * Tests that PassKey3 is the 88-character base64 SHA-512 of a session key and the login salt, matching an independent vector, and that a changed salt fails verification.
 */

#include "PassKey3.h"

#include <gtest/gtest.h>

namespace
{
    LoginSalt const Salt{ 0x1234, 0xAABBCCDD, 0x0123 };
}

TEST(PassKey3Test, Has88CharactersAndMatchesAnIndependentVector)
{
    std::string const sessionKey = "kP1oLuYmh3xVb2nQwTrA9sDfGhJkLzXcVbNmQwErTyU=";
    std::string const passKey3 = PassKey3::Compute(sessionKey, Salt);
    EXPECT_EQ(passKey3.size(), 88u);
    EXPECT_EQ(passKey3, "arlPtpqss0QLSqHYmoVbnynI0z5AEtgw0MV5HOjug4/kM7mUgAw5ShUXYKZgkjM6cJHVCrW3M71ehK+0pMNUnw==");
    EXPECT_TRUE(PassKey3::Verify(sessionKey, Salt, passKey3));
    EXPECT_FALSE(PassKey3::Verify(sessionKey, LoginSalt{ Salt.SessionId, Salt.Seconds + 1, Salt.Milliseconds }, passKey3));
}
