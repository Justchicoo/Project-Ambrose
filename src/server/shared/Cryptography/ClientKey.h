/*
 * Project Ambrose by Imjustchico
 * ClientKey1 and session keys: the client's base64 SHA-512 password verifier, the salted ClientKey1 it sends, a constant-time check of that key, and fresh 44-character session keys.
 */

#ifndef AMBROSE_CLIENTKEY_H
#define AMBROSE_CLIENTKEY_H

#include "LoginSalt.h"

#include <string>
#include <string_view>

namespace ClientKey
{
    std::string HashPassword(std::string_view password);
    std::string ComputeClientKey1(std::string_view verifier, LoginSalt const& salt);
    bool VerifyClientKey1(std::string_view verifier, LoginSalt const& salt, std::string_view clientKey1);
    std::string GenerateSessionKey(LoginSalt const& salt);
}

#endif
