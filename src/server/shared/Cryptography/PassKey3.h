/*
 * Project Ambrose by Imjustchico
 * PassKey3, the 88-character base64 SHA-512 of a session key and the login salt that later login messages carry.
 */

#ifndef AMBROSE_PASSKEY3_H
#define AMBROSE_PASSKEY3_H

#include "LoginSalt.h"

#include <string>
#include <string_view>

namespace PassKey3
{
    std::string Compute(std::string_view sessionKey, LoginSalt const& salt);
    bool Verify(std::string_view sessionKey, LoginSalt const& salt, std::string_view passKey3);
}

#endif
