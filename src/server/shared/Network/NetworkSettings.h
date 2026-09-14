/*
 * Project Ambrose by Imjustchico
 * Listener and socket settings for one app: bind address, port, network threads, frame limits, send buffer, and TCP_NODELAY, loaded from config.
 */

#ifndef AMBROSE_NETWORKSETTINGS_H
#define AMBROSE_NETWORKSETTINGS_H

#include "Frame.h"
#include "Types.h"

#include <cstddef>
#include <string>
#include <vector>

class ConfigMgr;
struct ConfigIssue;

struct NetworkSettings
{
    static constexpr std::size_t MaxThreads = 256;

    std::string BindIp = "0.0.0.0";
    uint16 Port = 0;
    std::size_t Threads = 1;
    FrameLimits Limits;
    int32 OutKBuff = -1;
    bool TcpNoDelay = true;

    static NetworkSettings Load(ConfigMgr const& config, std::string const& portOption, uint16 defaultPort, std::vector<std::string>* problems = nullptr);

    bool operator==(NetworkSettings const& other) const noexcept;
};

#endif
