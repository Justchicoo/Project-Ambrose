/*
 * Project Ambrose by Imjustchico
 * One HTTP listener's settings, read from config under the prefix that names it, Admin for an app's admin API and Panel for the supervisor's panel: whether it runs, where it binds, its token and token file, what it carries that must not be read off the wire, the plain-HTTP opt-in, the certificate and key it serves TLS with, the failed-authentication limit, the largest request it takes, the remote-access rule that judges a bind address in its own option names, and the warnings a binding the rule allows still has to say out loud.
 */

#ifndef AMBROSE_LISTENERSETTINGS_H
#define AMBROSE_LISTENERSETTINGS_H

#include "Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;

struct ListenerSettings
{
    static constexpr uint32 MinThreads = 2;
    static constexpr uint32 MaxThreads = 64;
    static constexpr uint32 MaxAuthFailureBurst = 100000;
    static constexpr double MaxAuthFailuresPerSecond = 100000.0;
    static constexpr std::size_t MinTokenLength = 16;
    static constexpr std::size_t MaxTokenLength = 512;
    static constexpr uint32 MinRequestBytes = 1024;
    static constexpr uint32 MaxRequestBytesLimit = 16777216;
    static constexpr uint32 MinSessionIdleMinutes = 5;
    static constexpr uint32 MaxSessionIdleMinutes = 10080;
    static constexpr uint32 MinSessionLifetimeHours = 1;
    static constexpr uint32 MaxSessionLifetimeHours = 720;
    static constexpr uint32 MaxRateLimitBurst = 1000000;
    static constexpr double MaxRateLimitPerSecond = 100000.0;

    std::string Prefix = "Admin";
    std::string Label = "the admin API";
    std::string LogCategory = "server.admin";
    std::string Secrets = "the token, commands and logs";
    bool Enable = false;
    std::string BindIp = "127.0.0.1";
    uint16 Port = 0;
    std::string Token;
    std::filesystem::path TokenFile;
    bool AllowPlainHttpRemote = false;
    std::filesystem::path CertificateFile;
    std::filesystem::path PrivateKeyFile;
    uint32 AuthFailureBurst = 10;
    double AuthFailuresPerSecond = 1.0;
    uint32 MaxRequestBytes = 262144;
    uint32 Threads = 2;
    std::filesystem::path DashboardDir;
    std::vector<std::string> AllowedHosts;
    std::string TrustedProxies;
    uint32 SessionIdleMinutes = 720;
    uint32 SessionLifetimeHours = 168;
    uint32 RateLimitBurst = 120;
    double RateLimitPerSecond = 2.0;

    static ListenerSettings Load(ConfigMgr const& config, std::string_view prefix, uint16 defaultPort, std::vector<std::string>* problems = nullptr);

    std::string Option(std::string_view name) const;

    bool BindsBeyondThisMachine() const;
    bool HasTls() const;
    std::optional<std::string> RemoteAccessError() const;
    std::optional<std::string> PlainHttpRemoteWarning() const;
    std::vector<std::string> Warnings() const;
    bool ListenerEquals(ListenerSettings const& other) const;
    std::filesystem::path DashboardFolder() const;
};

#endif
