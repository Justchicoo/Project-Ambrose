/*
 * Project Ambrose by Imjustchico
 * Reads Account.UsernameMinLength, Account.PasswordMinLength, Account.VerifierKeys, Account.VerifierActiveKey and Account.AllowPlainVerifiers, clamping lengths and refusing an unusable key ring or keys without an active key.
 */

#include "AccountSettings.h"
#include "ConfigMgr.h"

#include <fmt/format.h>

#include <algorithm>

std::optional<AccountSettings> AccountSettings::Load(ConfigMgr const& config, std::vector<std::string>& problems)
{
    auto length = [&](std::string const& option, uint32 fallback, uint32 maximum)
    {
        uint32 const configured = config.GetOption<uint32>(option, fallback, true);
        uint32 const value = std::clamp<uint32>(configured, 1, maximum);
        if (value != configured)
            problems.push_back(fmt::format("{} = {} is outside 1-{}; using {}", option, configured, maximum, value));
        return value;
    };

    AccountSettings settings;
    settings.UsernameMinLength = length("Account.UsernameMinLength", DefaultUsernameMinLength, MaxUsernameLength);
    settings.PasswordMinLength = length("Account.PasswordMinLength", DefaultPasswordMinLength, MaxPasswordLength);

    std::string error;
    std::optional<VerifierKeyRing> keys = VerifierKeyRing::Parse(config.GetOption<std::string>("Account.VerifierKeys", "", true), config.GetOption<uint32>("Account.VerifierActiveKey", 0, true), error);
    if (!keys)
    {
        problems.push_back(fmt::format("Account.VerifierKeys and Account.VerifierActiveKey are unusable: {}", error));
        return std::nullopt;
    }
    if (keys->GetKeyCount() > 0 && keys->GetActiveKeyId() == 0)
    {
        problems.push_back("Account.VerifierKeys lists keys but Account.VerifierActiveKey is 0, so new verifiers would be stored unencrypted; set it to the key that seals them");
        return std::nullopt;
    }
    settings.Keys = std::move(*keys);
    settings.AllowPlainVerifiers = config.GetOption<bool>("Account.AllowPlainVerifiers", true, true);
    return settings;
}
