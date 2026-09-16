/*
 * Project Ambrose by Imjustchico
 * Account rules read from configuration: username and password length limits, the verifier key ring, and whether unencrypted verifiers are still accepted, with every problem reported and a key error failing the load.
 */

#ifndef AMBROSE_ACCOUNTSETTINGS_H
#define AMBROSE_ACCOUNTSETTINGS_H

#include "VerifierKeyRing.h"

#include <optional>
#include <string>
#include <vector>

class ConfigMgr;

struct AccountSettings
{
    static constexpr uint32 MaxUsernameLength = 32;
    static constexpr uint32 MaxPasswordLength = 128;
    static constexpr uint32 MaxEmailLength = 255;
    static constexpr uint32 DefaultUsernameMinLength = 3;
    static constexpr uint32 DefaultPasswordMinLength = 4;

    uint32 UsernameMinLength = DefaultUsernameMinLength;
    uint32 PasswordMinLength = DefaultPasswordMinLength;
    bool AllowPlainVerifiers = true;
    VerifierKeyRing Keys;

    static std::optional<AccountSettings> Load(ConfigMgr const& config, std::vector<std::string>& problems);
};

#endif
