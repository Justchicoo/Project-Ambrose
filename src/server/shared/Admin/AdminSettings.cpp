/*
 * Project Ambrose by Imjustchico
 * Reads the Admin options from config, clamping out-of-range values and reporting each problem, and judges a bind address against the remote-access rule: anything but loopback needs TLS, which this build does not serve yet, or the plain-HTTP opt-in with no TLS files set.
 */

#include "AdminSettings.h"
#include "ConfigMgr.h"
#include "IpAddress.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>

AdminSettings AdminSettings::Load(ConfigMgr const& config, uint16 defaultPort, std::vector<std::string>* problems)
{
    auto report = [problems](std::string problem)
    {
        if (problems)
            problems->push_back(std::move(problem));
    };

    AdminSettings settings;
    settings.Enable = config.GetOption<bool>("Admin.Enable", settings.Enable, true);
    settings.BindIp = std::string(Ambrose::Trim(config.GetOption<std::string>("Admin.BindIP", settings.BindIp, true)));
    settings.Port = config.GetOption<uint16>("Admin.Port", defaultPort, true);
    settings.Token = std::string(Ambrose::Trim(config.GetOption<std::string>("Admin.Token", "", true)));
    settings.TokenFile = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>("Admin.TokenFile", "", true)));
    settings.AllowPlainHttpRemote = config.GetOption<bool>("Admin.AllowPlainHttpRemote", settings.AllowPlainHttpRemote, true);
    settings.CertificateFile = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>("Admin.CertificateFile", "", true)));
    settings.PrivateKeyFile = ConfigMgr::PathFromUtf8(Ambrose::Trim(config.GetOption<std::string>("Admin.PrivateKeyFile", "", true)));

    uint32 const burst = config.GetOption<uint32>("Admin.AuthFailureBurst", settings.AuthFailureBurst, true);
    settings.AuthFailureBurst = std::clamp<uint32>(burst, 1, MaxAuthFailureBurst);
    if (settings.AuthFailureBurst != burst)
        report(fmt::format("Admin.AuthFailureBurst = {} is outside 1-{}; using {}", burst, MaxAuthFailureBurst, settings.AuthFailureBurst));

    double const refill = config.GetOption<double>("Admin.AuthFailuresPerSecond", settings.AuthFailuresPerSecond, true);
    settings.AuthFailuresPerSecond = std::clamp(refill, 0.0, MaxAuthFailuresPerSecond);
    if (settings.AuthFailuresPerSecond != refill)
        report(fmt::format("Admin.AuthFailuresPerSecond = {} is outside 0-{}; using {}", refill, MaxAuthFailuresPerSecond, settings.AuthFailuresPerSecond));

    uint32 const requestBytes = config.GetOption<uint32>("Admin.MaxRequestBytes", settings.MaxRequestBytes, true);
    settings.MaxRequestBytes = std::clamp<uint32>(requestBytes, MinRequestBytes, MaxRequestBytesLimit);
    if (settings.MaxRequestBytes != requestBytes)
        report(fmt::format("Admin.MaxRequestBytes = {} is outside {}-{}; using {}", requestBytes, MinRequestBytes, MaxRequestBytesLimit, settings.MaxRequestBytes));

    uint32 const threads = config.GetOption<uint32>("Admin.Threads", settings.Threads, true);
    settings.Threads = std::clamp<uint32>(threads, MinThreads, MaxThreads);
    if (settings.Threads != threads)
        report(fmt::format("Admin.Threads = {} is outside {}-{}; using {}", threads, MinThreads, MaxThreads, settings.Threads));

    return settings;
}

bool AdminSettings::BindsBeyondThisMachine() const
{
    std::optional<asio::ip::address> const address = Ambrose::Asio::MakeAddress(BindIp);
    if (!address)
        return true;
    return !Ambrose::Asio::IsLoopback(*address);
}

bool AdminSettings::HasTls() const
{
    return !CertificateFile.empty() && !PrivateKeyFile.empty();
}

std::optional<std::string> AdminSettings::RemoteAccessError() const
{
    if (!Ambrose::Asio::MakeAddress(BindIp))
        return fmt::format("Admin.BindIP = {} is not an IP address the admin API can bind", Ambrose::ForLog(BindIp));
    if (CertificateFile.empty() != PrivateKeyFile.empty())
        return std::string(CertificateFile.empty()
            ? "Admin.PrivateKeyFile is set without Admin.CertificateFile, so the admin API has no certificate to serve TLS with"
            : "Admin.CertificateFile is set without Admin.PrivateKeyFile, so the admin API has no key to serve TLS with");
    if (!BindsBeyondThisMachine())
        return std::nullopt;
    if (HasTls())
        return fmt::format("Admin.BindIP = {} reaches beyond this machine and Admin.CertificateFile with Admin.PrivateKeyFile asks for TLS, which this build does not serve yet; bind 127.0.0.1, or clear both files {}", BindIp,
            AllowPlainHttpRemote ? "to serve plain HTTP off this machine, which Admin.AllowPlainHttpRemote = 1 already allows" : "and set Admin.AllowPlainHttpRemote = 1 to serve plain HTTP off this machine");
    if (!AllowPlainHttpRemote)
        return fmt::format("Admin.BindIP = {} reaches beyond this machine with no TLS; set Admin.CertificateFile and Admin.PrivateKeyFile, bind 127.0.0.1, or set Admin.AllowPlainHttpRemote = 1 to send the token, commands and logs unencrypted", BindIp);
    return std::nullopt;
}

std::optional<std::string> AdminSettings::PlainHttpRemoteWarning() const
{
    if (!BindsBeyondThisMachine() || HasTls() || !AllowPlainHttpRemote)
        return std::nullopt;
    return fmt::format("Admin.AllowPlainHttpRemote = 1 serves the admin API as plain HTTP on {}: the token is still required, but it, every command and every log line cross the network unencrypted, so anyone on the path can read them and reuse the token", BindIp);
}

bool AdminSettings::ListenerEquals(AdminSettings const& other) const
{
    return Enable == other.Enable && BindIp == other.BindIp && Port == other.Port && Threads == other.Threads
        && MaxRequestBytes == other.MaxRequestBytes && AllowPlainHttpRemote == other.AllowPlainHttpRemote
        && CertificateFile == other.CertificateFile && PrivateKeyFile == other.PrivateKeyFile;
}
