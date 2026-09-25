/*
 * Project Ambrose by Imjustchico
 * Session rules for one app: how long to wait for SessionAccept, how often the server sends keepalives, how long it waits for proof of life, how many protocol strikes close a session, how many dropped messages a session may send before each further one strikes, and how fast a session's pings are answered.
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
    static constexpr uint32 DefaultHandoffGraceSeconds = 30;
    static constexpr uint32 DefaultAttachTimeoutSeconds = 30;
    static constexpr uint32 DefaultMaxStrikes = 10;
    static constexpr uint32 MaxStrikesLimit = 1000;
    static constexpr uint32 DefaultDroppedMessageBurst = 64;
    static constexpr uint32 DefaultDroppedMessagesPerSecond = 16;
    static constexpr uint32 DefaultPingBurst = 16;
    static constexpr uint32 DefaultPingsPerSecond = 4;
    static constexpr uint32 MaxBudgetRate = 100000;

    std::chrono::milliseconds AcceptTimeout{ std::chrono::seconds(DefaultAcceptTimeoutSeconds) };
    std::chrono::milliseconds KeepAliveInterval{ std::chrono::seconds(DefaultKeepAliveIntervalSeconds) };
    std::chrono::milliseconds KeepAliveTimeout{ std::chrono::seconds(DefaultKeepAliveTimeoutSeconds) };
    std::chrono::milliseconds HandoffGrace{ std::chrono::seconds(DefaultHandoffGraceSeconds) };
    std::chrono::milliseconds AttachTimeout{ std::chrono::seconds(DefaultAttachTimeoutSeconds) };
    uint32 MaxStrikes = DefaultMaxStrikes;
    uint32 DroppedMessageBurst = DefaultDroppedMessageBurst;
    uint32 DroppedMessagesPerSecond = DefaultDroppedMessagesPerSecond;
    uint32 PingBurst = DefaultPingBurst;
    uint32 PingsPerSecond = DefaultPingsPerSecond;

    static SessionSettings Load(ConfigMgr const& config, std::vector<std::string>* problems = nullptr);

    bool operator==(SessionSettings const&) const = default;
};

#endif
