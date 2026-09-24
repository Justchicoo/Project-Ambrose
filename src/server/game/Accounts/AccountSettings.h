/*
 * Project Ambrose by Imjustchico
 * Account rules read from configuration: username and password length limits, the verifier key ring, and whether unencrypted verifiers are still accepted, with every problem reported and a key error failing the load.
 */

#ifndef AMBROSE_ACCOUNTSETTINGS_H
#define AMBROSE_ACCOUNTSETTINGS_H

#include "AccountText.h"
#include "VerifierKeyRing.h"

#include <optional>
#include <string>
#include <vector>

class ConfigMgr;

struct AccountSettings
{
    static constexpr uint32 MaxUsernameLength = Ambrose::AccountText::MaxUsernameLength;
    static constexpr uint32 MaxPasswordLength = Ambrose::AccountText::MaxPasswordLength;
    static constexpr uint32 MaxEmailLength = Ambrose::AccountText::MaxEmailLength;
    static constexpr uint32 DefaultUsernameMinLength = Ambrose::AccountText::DefaultUsernameMinLength;
    static constexpr uint32 DefaultPasswordMinLength = Ambrose::AccountText::DefaultPasswordMinLength;

    uint32 UsernameMinLength = DefaultUsernameMinLength;
    uint32 PasswordMinLength = DefaultPasswordMinLength;
    bool AllowPlainVerifiers = true;
    VerifierKeyRing Keys;

    static std::optional<AccountSettings> Load(ConfigMgr const& config, std::vector<std::string>& problems);
};

#endif
