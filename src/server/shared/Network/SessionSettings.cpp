/*
 * Project Ambrose by Imjustchico
 * Reads session timing options in seconds, the strike limit and the dropped-message budget from config, clamping out-of-range values and reporting each problem.
 */

#include "SessionSettings.h"
#include "ConfigMgr.h"

#include <fmt/format.h>

#include <algorithm>

SessionSettings SessionSettings::Load(ConfigMgr const& config, std::vector<std::string>* problems)
{
    auto seconds = [&](std::string const& option, uint32 fallback, uint32 minimum)
    {
        uint32 const configured = config.GetOption<uint32>(option, fallback, true);
        uint32 const value = std::clamp(configured, minimum, MaxSeconds);
        if (value != configured && problems)
            problems->push_back(fmt::format("{} = {} is outside {}-{}; using {}", option, configured, minimum, MaxSeconds, value));
        return std::chrono::milliseconds(std::chrono::seconds(value));
    };

    SessionSettings settings;
    settings.AcceptTimeout = seconds("Network.SessionAcceptTimeout", DefaultAcceptTimeoutSeconds, 1);
    settings.KeepAliveInterval = seconds("Network.KeepAliveInterval", DefaultKeepAliveIntervalSeconds, 0);
    settings.KeepAliveTimeout = seconds("Network.KeepAliveTimeout", DefaultKeepAliveTimeoutSeconds, 1);
    auto count = [&](std::string const& option, uint32 fallback, uint32 maximum)
    {
        uint32 const configured = config.GetOption<uint32>(option, fallback, true);
        uint32 const value = std::clamp<uint32>(configured, 1, maximum);
        if (value != configured && problems)
            problems->push_back(fmt::format("{} = {} is outside 1-{}; using {}", option, configured, maximum, value));
        return value;
    };
    settings.MaxStrikes = count("Network.MaxStrikes", DefaultMaxStrikes, MaxStrikesLimit);
    settings.DroppedMessageBurst = count("Network.DroppedMessageBurst", DefaultDroppedMessageBurst, MaxDroppedMessageRate);
    settings.DroppedMessagesPerSecond = count("Network.DroppedMessagesPerSecond", DefaultDroppedMessagesPerSecond, MaxDroppedMessageRate);
    return settings;
}
