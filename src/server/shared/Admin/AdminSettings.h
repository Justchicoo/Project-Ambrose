/*
 * Project Ambrose by Imjustchico
 * Admin API listener settings loaded from config: whether it runs, where it binds, its token and token file, the plain-HTTP opt-in, the TLS files, the failed-authentication limit, the largest request it takes, and the remote-access rule that judges a bind address.
 */

#ifndef AMBROSE_ADMINSETTINGS_H
#define AMBROSE_ADMINSETTINGS_H

#include "Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class ConfigMgr;

struct AdminSettings
{
    static constexpr uint32 MinThreads = 2;
    static constexpr uint32 MaxThreads = 64;
    static constexpr uint32 MaxAuthFailureBurst = 100000;
    static constexpr double MaxAuthFailuresPerSecond = 100000.0;
    static constexpr std::size_t MinTokenLength = 16;
    static constexpr std::size_t MaxTokenLength = 512;
    static constexpr uint32 MinRequestBytes = 1024;
    static constexpr uint32 MaxRequestBytesLimit = 16777216;

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

    static AdminSettings Load(ConfigMgr const& config, uint16 defaultPort, std::vector<std::string>* problems = nullptr);

    bool BindsBeyondThisMachine() const;
    bool HasTls() const;
    std::optional<std::string> RemoteAccessError() const;
    std::optional<std::string> PlainHttpRemoteWarning() const;
    bool ListenerEquals(AdminSettings const& other) const;
};

#endif
