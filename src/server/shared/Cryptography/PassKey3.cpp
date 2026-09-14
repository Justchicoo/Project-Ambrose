/*
 * Project Ambrose by Imjustchico
 * Hashes the session key followed by the decimal login salt with SHA-512 into base64 and compares candidates in constant time.
 */

#include "PassKey3.h"
#include "Base64.h"
#include "ConstantTime.h"
#include "SHA512.h"

std::string PassKey3::Compute(std::string_view sessionKey, LoginSalt const& salt)
{
    SHA512 hash;
    hash.Update(sessionKey);
    hash.Update(salt.ToString());
    return Base64::Encode(hash.Finalize());
}

bool PassKey3::Verify(std::string_view sessionKey, LoginSalt const& salt, std::string_view passKey3)
{
    std::string const expected = Compute(sessionKey, salt);
    return Ambrose::Crypto::ConstantTimeEquals(expected, passKey3);
}
