/*
 * Project Ambrose by Imjustchico
 * Login rules read from configuration, which each authentication attempt takes a snapshot of: revision enforcement, failed-attempt limits and lockouts, what a second login to an online account does, and how long session keys last.
 */

#ifndef AMBROSE_LOGINSETTINGS_H
#define AMBROSE_LOGINSETTINGS_H

#include "Types.h"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;

enum class DuplicateLoginPolicy : uint8
{
    Reject = 0,
    KickExisting = 1
};

struct LoginSettings
{
    static constexpr uint32 DefaultMaxAuthAttempts = 5;
    static constexpr uint32 MaxAuthAttemptsLimit = 1000;
    static constexpr uint32 DefaultLockoutSeconds = 900;
    static constexpr uint32 DefaultSessionKeyLifetimeSeconds = 30 * 3600;
    static constexpr uint32 MinSessionKeyLifetimeSeconds = 60;
    static constexpr uint32 MaxDurationSeconds = 30 * 24 * 3600;

    bool EnforceRevision = false;
    std::vector<std::string> AllowedRevisions;
    uint32 MaxAuthAttempts = DefaultMaxAuthAttempts;
    std::chrono::seconds Lockout{ DefaultLockoutSeconds };
    DuplicateLoginPolicy DuplicateLogins = DuplicateLoginPolicy::KickExisting;
    std::chrono::seconds SessionKeyLifetime{ DefaultSessionKeyLifetimeSeconds };

    bool AllowsRevision(std::string_view revision) const;

    static LoginSettings Load(ConfigMgr const& config, std::vector<std::string>* problems = nullptr);

    bool operator==(LoginSettings const&) const = default;
};

#endif
