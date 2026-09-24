/*
 * Project Ambrose by Imjustchico
 * Login rules read from configuration, which each authentication attempt, character list and idle check takes a snapshot of: the name the login server shows, revision enforcement, failed-attempt limits and lockouts, what a second login to an online account does, how long session keys last, when idle clients are dropped, and how long a shutdown waits for clients to leave.
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
    static constexpr uint32 DefaultAfkTimeoutSeconds = 360;
    static constexpr uint32 MaxAfkTimeoutSeconds = 24 * 3600;
    static constexpr int8 DefaultAfkWarning = 1;
    static constexpr uint32 DefaultKeyTtlSeconds = 60;
    static constexpr uint32 MinKeyTtlSeconds = 5;
    static constexpr uint32 DefaultShutdownGraceSeconds = 5;
    static constexpr uint32 MaxShutdownGraceSeconds = 60;

    static constexpr std::string_view DefaultName = "Ambrose";
    static constexpr std::size_t MaxNameBytes = 64;

    std::string Name{ DefaultName };
    bool EnforceRevision = false;
    std::vector<std::string> AllowedRevisions;
    uint32 MaxAuthAttempts = DefaultMaxAuthAttempts;
    std::chrono::seconds Lockout{ DefaultLockoutSeconds };
    std::chrono::seconds KeyTtl{ DefaultKeyTtlSeconds };
    DuplicateLoginPolicy DuplicateLogins = DuplicateLoginPolicy::KickExisting;
    std::chrono::seconds SessionKeyLifetime{ DefaultSessionKeyLifetimeSeconds };
    std::chrono::seconds AfkTimeout{ DefaultAfkTimeoutSeconds };
    int8 AfkWarning = DefaultAfkWarning;
    std::chrono::seconds ShutdownGrace{ DefaultShutdownGraceSeconds };

    bool AllowsRevision(std::string_view revision) const;

    static LoginSettings Load(ConfigMgr const& config, std::vector<std::string>* problems = nullptr);

    bool operator==(LoginSettings const&) const = default;
};

#endif
