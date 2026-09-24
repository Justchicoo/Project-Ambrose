/*
 * Project Ambrose by Imjustchico
 * Reads the Admin options from config, clamping out-of-range values and reporting each problem, judges a bind address against the remote-access rule, where anything but loopback needs a certificate and key or the plain-HTTP opt-in, collects the warning a binding the rule allows still carries, that plain HTTP off this machine crosses the network unencrypted, and finds the built panel beside the executable when no folder is set.
 */

#include "ListenerSettings.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "IpAddress.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>

ListenerSettings ListenerSettings::Load(ConfigMgr const& config, std::string_view prefix, uint16 defaultPort, std::vector<std::string>* problems)
{
    auto const key = [prefix](std::string_view name) { return fmt::format("{}.{}", prefix, name); };
    auto report = [problems](std::string problem)
    {
        if (problems)
            problems->push_back(std::move(problem));
    };

    ListenerSettings settings;
    settings.Prefix = std::string(prefix);
    settings.Enable = config.GetOption<bool>(key("Enable"), settings.Enable, true);
    settings.BindIp = std::string(Ambrose::Trim(config.GetOption<std::string>(key("BindIP"), settings.BindIp, true)));
    settings.Port = config.GetOption<uint16>(key("Port"), defaultPort, true);
    settings.Token = std::string(Ambrose::Trim(config.GetOption<std::string>(key("Token"), "", true)));
    settings.TokenFile = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(key("TokenFile"), "", true)));
    settings.AllowPlainHttpRemote = config.GetOption<bool>(key("AllowPlainHttpRemote"), settings.AllowPlainHttpRemote, true);
    settings.CertificateFile = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(key("CertificateFile"), "", true)));
    settings.PrivateKeyFile = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(key("PrivateKeyFile"), "", true)));

    uint32 const burst = config.GetOption<uint32>(key("AuthFailureBurst"), settings.AuthFailureBurst, true);
    settings.AuthFailureBurst = std::clamp<uint32>(burst, 1, MaxAuthFailureBurst);
    if (settings.AuthFailureBurst != burst)
        report(fmt::format("{} = {} is outside 1-{}; using {}", key("AuthFailureBurst"), burst, MaxAuthFailureBurst, settings.AuthFailureBurst));

    double const refill = config.GetOption<double>(key("AuthFailuresPerSecond"), settings.AuthFailuresPerSecond, true);
    settings.AuthFailuresPerSecond = std::clamp(refill, 0.0, MaxAuthFailuresPerSecond);
    if (settings.AuthFailuresPerSecond != refill)
        report(fmt::format("{} = {} is outside 0-{}; using {}", key("AuthFailuresPerSecond"), refill, MaxAuthFailuresPerSecond, settings.AuthFailuresPerSecond));

    uint32 const requestBytes = config.GetOption<uint32>(key("MaxRequestBytes"), settings.MaxRequestBytes, true);
    settings.MaxRequestBytes = std::clamp<uint32>(requestBytes, MinRequestBytes, MaxRequestBytesLimit);
    if (settings.MaxRequestBytes != requestBytes)
        report(fmt::format("{} = {} is outside {}-{}; using {}", key("MaxRequestBytes"), requestBytes, MinRequestBytes, MaxRequestBytesLimit, settings.MaxRequestBytes));

    uint32 const threads = config.GetOption<uint32>(key("Threads"), settings.Threads, true);
    settings.Threads = std::clamp<uint32>(threads, MinThreads, MaxThreads);
    if (settings.Threads != threads)
        report(fmt::format("{} = {} is outside {}-{}; using {}", key("Threads"), threads, MinThreads, MaxThreads, settings.Threads));

    settings.DashboardDir = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>(key("DashboardDir"), "", true)));
    settings.TrustedProxies = std::string(Ambrose::Trim(config.GetOption<std::string>(key("TrustedProxies"), "", true)));

    std::string const hostList = config.GetOption<std::string>(key("AllowedHosts"), "", true);
    for (std::string_view rest = hostList; !rest.empty();)
    {
        std::size_t const comma = rest.find(',');
        std::string_view const name = Ambrose::Trim(rest.substr(0, comma));
        if (!name.empty())
            settings.AllowedHosts.push_back(Ambrose::ToLower(name));
        rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
    }

    uint32 const idle = config.GetOption<uint32>(key("SessionIdleMinutes"), settings.SessionIdleMinutes, true);
    settings.SessionIdleMinutes = std::clamp<uint32>(idle, MinSessionIdleMinutes, MaxSessionIdleMinutes);
    if (settings.SessionIdleMinutes != idle)
        report(fmt::format("{} = {} is outside {}-{}; using {}", key("SessionIdleMinutes"), idle, MinSessionIdleMinutes, MaxSessionIdleMinutes, settings.SessionIdleMinutes));

    uint32 const lifetime = config.GetOption<uint32>(key("SessionLifetimeHours"), settings.SessionLifetimeHours, true);
    settings.SessionLifetimeHours = std::clamp<uint32>(lifetime, MinSessionLifetimeHours, MaxSessionLifetimeHours);
    if (settings.SessionLifetimeHours != lifetime)
        report(fmt::format("{} = {} is outside {}-{}; using {}", key("SessionLifetimeHours"), lifetime, MinSessionLifetimeHours, MaxSessionLifetimeHours, settings.SessionLifetimeHours));

    uint32 const rateBurst = config.GetOption<uint32>(key("RateLimitBurst"), settings.RateLimitBurst, true);
    settings.RateLimitBurst = std::clamp<uint32>(rateBurst, 1, MaxRateLimitBurst);
    if (settings.RateLimitBurst != rateBurst)
        report(fmt::format("{} = {} is outside 1-{}; using {}", key("RateLimitBurst"), rateBurst, MaxRateLimitBurst, settings.RateLimitBurst));

    double const ratePerSecond = config.GetOption<double>(key("RateLimitPerSecond"), settings.RateLimitPerSecond, true);
    settings.RateLimitPerSecond = std::clamp(ratePerSecond, 0.0, MaxRateLimitPerSecond);
    if (settings.RateLimitPerSecond != ratePerSecond)
        report(fmt::format("{} = {} is outside 0-{}; using {}", key("RateLimitPerSecond"), ratePerSecond, MaxRateLimitPerSecond, settings.RateLimitPerSecond));

    return settings;
}

std::filesystem::path ListenerSettings::DashboardFolder() const
{
    return DashboardDir.empty() ? Ambrose::GetExecutableDirectory() / "dashboard" : DashboardDir;
}

bool ListenerSettings::BindsBeyondThisMachine() const
{
    std::optional<asio::ip::address> const address = Ambrose::Asio::MakeAddress(BindIp);
    if (!address)
        return true;
    return !Ambrose::Asio::IsLoopback(*address);
}

bool ListenerSettings::HasTls() const
{
    return !CertificateFile.empty() && !PrivateKeyFile.empty();
}

std::string ListenerSettings::Option(std::string_view name) const
{
    return fmt::format("{}.{}", Prefix, name);
}

std::optional<std::string> ListenerSettings::RemoteAccessError() const
{
    if (!Ambrose::Asio::MakeAddress(BindIp))
        return fmt::format("{} = {} is not an IP address {} can bind", Option("BindIP"), Ambrose::ForLog(BindIp), Label);
    if (CertificateFile.empty() != PrivateKeyFile.empty())
        return CertificateFile.empty()
            ? fmt::format("{} is set without {}, so {} has no certificate to serve TLS with", Option("PrivateKeyFile"), Option("CertificateFile"), Label)
            : fmt::format("{} is set without {}, so {} has no key to serve TLS with", Option("CertificateFile"), Option("PrivateKeyFile"), Label);
    if (!BindsBeyondThisMachine() || HasTls())
        return std::nullopt;
    if (!AllowPlainHttpRemote)
        return fmt::format("{} = {} reaches beyond this machine with no TLS; set {} and {}, bind 127.0.0.1, or set {} = 1 to send {} unencrypted",
            Option("BindIP"), BindIp, Option("CertificateFile"), Option("PrivateKeyFile"), Option("AllowPlainHttpRemote"), Secrets);
    return std::nullopt;
}

std::optional<std::string> ListenerSettings::PlainHttpRemoteWarning() const
{
    if (!BindsBeyondThisMachine() || HasTls() || !AllowPlainHttpRemote)
        return std::nullopt;
    return fmt::format("{} = 1 serves {} as plain HTTP on {}: {} cross the network unencrypted, so anyone on the path can read them and use them",
        Option("AllowPlainHttpRemote"), Label, BindIp, Secrets);
}

std::vector<std::string> ListenerSettings::Warnings() const
{
    std::vector<std::string> warnings;
    if (std::optional<std::string> const remote = PlainHttpRemoteWarning())
        warnings.push_back(*remote);
    return warnings;
}

bool ListenerSettings::ListenerEquals(ListenerSettings const& other) const
{
    return Enable == other.Enable && BindIp == other.BindIp && Port == other.Port && Threads == other.Threads
        && MaxRequestBytes == other.MaxRequestBytes && AllowPlainHttpRemote == other.AllowPlainHttpRemote
        && CertificateFile == other.CertificateFile && PrivateKeyFile == other.PrivateKeyFile;
}
