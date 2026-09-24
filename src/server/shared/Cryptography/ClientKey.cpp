/*
 * Project Ambrose by Imjustchico
 * Hashes UTF-8 text with SHA-512 into base64, appends the decimal login salt before hashing ClientKey1, compares keys in constant time, and derives session keys from 32 random bytes and the salt.
 */

#include "ClientKey.h"
#include "Base64.h"
#include "ConstantTime.h"
#include "CryptoRandom.h"
#include "SHA256.h"
#include "SHA512.h"

std::string ClientKey::HashPassword(std::string_view password)
{
    return Base64::Encode(SHA512::GetDigestOf(password));
}

std::string ClientKey::ComputeClientKey1(std::string_view verifier, LoginSalt const& salt)
{
    SHA512 hash;
    hash.Update(verifier);
    hash.Update(salt.ToString());
    return Base64::Encode(hash.Finalize());
}

bool ClientKey::VerifyClientKey1(std::string_view verifier, LoginSalt const& salt, std::string_view clientKey1)
{
    std::string const expected = ComputeClientKey1(verifier, salt);
    return Ambrose::Crypto::ConstantTimeEquals(expected, clientKey1);
}

std::string ClientKey::GenerateSessionKey(LoginSalt const& salt)
{
    std::array<uint8, 32> const random = Ambrose::Crypto::GetRandomArray<32>();
    SHA256 hash;
    hash.Update(random);
    hash.Update(salt.ToString());
    return Base64::Encode(hash.Finalize());
}
