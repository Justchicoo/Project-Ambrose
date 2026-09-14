/*
 * Project Ambrose by Imjustchico
 * Reads network options from config, clamping out-of-range values to safe bounds and reporting each problem.
 */

#include "NetworkSettings.h"
#include "ConfigMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>

NetworkSettings NetworkSettings::Load(ConfigMgr const& config, std::string const& portOption, uint16 defaultPort, std::vector<std::string>* problems)
{
    auto report = [problems](std::string problem)
    {
        if (problems)
            problems->push_back(std::move(problem));
    };

    NetworkSettings settings;
    settings.BindIp = config.GetOption<std::string>("BindIP", settings.BindIp, true);
    settings.Port = config.GetOption<uint16>(portOption, defaultPort, true);

    uint32 const threads = config.GetOption<uint32>("Network.Threads", 1, true);
    settings.Threads = std::clamp<std::size_t>(threads, 1, MaxThreads);
    if (settings.Threads != threads)
        report(fmt::format("Network.Threads = {} is outside 1-{}; using {}", threads, MaxThreads, settings.Threads));

    uint64 const maxFrameSize = config.GetOption<uint64>("Network.MaxFrameSize", FrameLimits::DefaultMaxFrameSize, true);
    std::size_t const minimumFrameSize = FrameLayout::LongPrefixSize + FrameLayout::FrameHeaderSize + FrameLayout::DmlHeaderSize + FrameLayout::TrailerSize;
    settings.Limits.MaxFrameSize = static_cast<std::size_t>(std::clamp<uint64>(maxFrameSize, minimumFrameSize, uint64{ 1 } << 30));
    if (settings.Limits.MaxFrameSize != maxFrameSize)
        report(fmt::format("Network.MaxFrameSize = {} is outside {}-{}; using {}", maxFrameSize, minimumFrameSize, uint64{ 1 } << 30, settings.Limits.MaxFrameSize));

    uint32 const maxMessages = config.GetOption<uint32>("Network.MaxDmlMessages", static_cast<uint32>(FrameLimits::DefaultMaxDmlMessages), true);
    settings.Limits.MaxDmlMessages = std::max<std::size_t>(maxMessages, 1);
    if (maxMessages == 0)
        report("Network.MaxDmlMessages = 0 would reject every DML frame; using 1");

    std::string const longLength = config.GetOption<std::string>("Network.LongFrameLength", "BodyOnly", true);
    if (Ambrose::EqualsIgnoreCase(longLength, "HeaderAndBody"))
        settings.Limits.LongLength = LongFrameLength::HeaderAndBody;
    else if (!Ambrose::EqualsIgnoreCase(longLength, "BodyOnly"))
        report(fmt::format("Network.LongFrameLength = {} is not BodyOnly or HeaderAndBody; using BodyOnly", longLength));

    settings.OutKBuff = config.GetOption<int32>("Network.OutKBuff", -1, true);
    settings.TcpNoDelay = config.GetOption<bool>("Network.TcpNoDelay", true, true);
    return settings;
}

bool NetworkSettings::operator==(NetworkSettings const& other) const noexcept
{
    return BindIp == other.BindIp && Port == other.Port && Threads == other.Threads && Limits.MaxFrameSize == other.Limits.MaxFrameSize
        && Limits.LongLength == other.Limits.LongLength && Limits.MaxDmlMessages == other.Limits.MaxDmlMessages && OutKBuff == other.OutKBuff && TcpNoDelay == other.TcpNoDelay;
}
