/*
 * Project Ambrose by Imjustchico
 * Session timing for one app: how long to wait for SessionAccept, how often the server sends keepalives, and how long it waits for proof of life.
 */

#ifndef AMBROSE_SESSIONSETTINGS_H
#define AMBROSE_SESSIONSETTINGS_H

#include "Types.h"

#include <chrono>
#include <string>
#include <vector>

class ConfigMgr;

struct SessionSettings
{
    static constexpr uint32 DefaultAcceptTimeoutSeconds = 15;
    static constexpr uint32 DefaultKeepAliveIntervalSeconds = 60;
    static constexpr uint32 DefaultKeepAliveTimeoutSeconds = 15;
    static constexpr uint32 MaxSeconds = 3600;

    std::chrono::milliseconds AcceptTimeout{ std::chrono::seconds(DefaultAcceptTimeoutSeconds) };
    std::chrono::milliseconds KeepAliveInterval{ std::chrono::seconds(DefaultKeepAliveIntervalSeconds) };
    std::chrono::milliseconds KeepAliveTimeout{ std::chrono::seconds(DefaultKeepAliveTimeoutSeconds) };

    static SessionSettings Load(ConfigMgr const& config, std::vector<std::string>* problems = nullptr);

    bool operator==(SessionSettings const&) const = default;
};

#endif
