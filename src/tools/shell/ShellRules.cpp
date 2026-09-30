/*
 * Project Ambrose by Imjustchico
 * Reads the scheme, host and port out of a URL with the default ports filled in, and answers each window decision from them: a view stays only on its own origin and hands any other web address, or any new window, to the system browser while refusing every other scheme; a certificate is accepted only on an exact fingerprint match; and the host channel admits only the program's own origin, naming each stranger once.
 */

#include "ShellRules.h"

#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <system_error>

namespace
{
    int DefaultPort(std::string_view scheme)
    {
        if (scheme == "http")
            return 80;
        if (scheme == "https")
            return 443;
        return 0;
    }

    bool WebScheme(std::string_view scheme)
    {
        return scheme == "http" || scheme == "https";
    }
}

std::string ShellOrigin::Describe() const
{
    if (Port == 0 || Port == DefaultPort(Scheme))
        return fmt::format("{}://{}", Scheme, Host);
    return fmt::format("{}://{}:{}", Scheme, Host, Port);
}

std::optional<ShellOrigin> ShellOrigin::Of(std::string_view url)
{
    std::size_t const colon = url.find(':');
    if (colon == std::string_view::npos || colon == 0)
        return std::nullopt;
    std::string scheme = Ambrose::ToLower(std::string(url.substr(0, colon)));
    if (!std::all_of(scheme.begin(), scheme.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '-' || c == '.'; }))
        return std::nullopt;
    std::string_view rest = url.substr(colon + 1);
    if (!rest.starts_with("//"))
        return std::nullopt;
    rest.remove_prefix(2);
    std::string_view authority = rest.substr(0, rest.find_first_of("/?#"));
    std::size_t const at = authority.rfind('@');
    if (at != std::string_view::npos)
        authority.remove_prefix(at + 1);
    if (authority.empty())
        return std::nullopt;

    ShellOrigin origin;
    origin.Scheme = std::move(scheme);
    std::string_view host = authority;
    std::string_view port;
    if (authority.front() == '[')
    {
        std::size_t const close = authority.find(']');
        if (close == std::string_view::npos)
            return std::nullopt;
        host = authority.substr(0, close + 1);
        std::string_view const after = authority.substr(close + 1);
        if (!after.empty())
        {
            if (after.front() != ':')
                return std::nullopt;
            port = after.substr(1);
        }
    }
    else
    {
        std::size_t const portColon = authority.rfind(':');
        if (portColon != std::string_view::npos)
        {
            host = authority.substr(0, portColon);
            port = authority.substr(portColon + 1);
        }
    }
    if (host.empty())
        return std::nullopt;
    origin.Host = Ambrose::ToLower(std::string(host));
    origin.Port = DefaultPort(origin.Scheme);
    if (!port.empty())
    {
        int value = 0;
        auto const [end, failed] = std::from_chars(port.data(), port.data() + port.size(), value);
        if (failed != std::errc() || end != port.data() + port.size() || value < 1 || value > 65535)
            return std::nullopt;
        origin.Port = value;
    }
    return origin;
}

ShellOrigin ShellOrigin::Own(std::string_view program)
{
    ShellOrigin origin;
#ifdef _WIN32
    origin.Scheme = "https";
    origin.Port = 443;
#else
    origin.Scheme = OwnScheme;
#endif
    origin.Host = fmt::format("{}.{}", Ambrose::ToLower(std::string(program)), OwnDomain);
    return origin;
}

ShellNavigation ShellRules::Navigate(ShellOrigin const& bound, std::string_view url, bool newWindow)
{
    if (url == "about:blank" || url == "about:srcdoc")
        return newWindow ? ShellNavigation::Refuse : ShellNavigation::Stay;
    std::optional<ShellOrigin> const target = ShellOrigin::Of(url);
    if (!target)
        return ShellNavigation::Refuse;
    if (!newWindow && *target == bound)
        return ShellNavigation::Stay;
    return WebScheme(target->Scheme) ? ShellNavigation::SystemBrowser : ShellNavigation::Refuse;
}

std::string ShellRules::NormalFingerprint(std::string_view fingerprint)
{
    std::string digits;
    for (char const c : fingerprint)
        if (std::isxdigit(static_cast<unsigned char>(c)))
            digits.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    if (digits.size() != 64)
        return std::string();
    std::string normal;
    for (std::size_t index = 0; index < digits.size(); index += 2)
    {
        if (!normal.empty())
            normal.push_back(':');
        normal.append(digits, index, 2);
    }
    return normal;
}

ShellPinDecision ShellRules::Certificate(ShellOrigin const& origin, std::string_view fingerprint, std::string_view pin)
{
    std::string const served = NormalFingerprint(fingerprint);
    std::string const pinned = NormalFingerprint(pin);
    ShellPinDecision decision;
    if (served.empty())
    {
        decision.Reason = fmt::format("{} served a certificate whose fingerprint could not be read, so it was refused", origin.Describe());
        return decision;
    }
    if (pinned.empty())
    {
        decision.Reason = fmt::format("{} served a certificate this computer cannot verify and no certificate is pinned for it, so it was refused; it served {}",
            origin.Describe(), served);
        return decision;
    }
    if (served != pinned)
    {
        decision.Reason = fmt::format("{} served the certificate {}, which is not the pinned {}, so it was refused", origin.Describe(), served, pinned);
        return decision;
    }
    decision.Accept = true;
    return decision;
}

ShellGate::ShellGate(ShellOrigin own, bool remote, Logger log) : _own(std::move(own)), _remote(remote), _log(std::move(log))
{
}

bool ShellGate::Admit(std::string_view sourceUrl)
{
    std::optional<ShellOrigin> const origin = ShellOrigin::Of(sourceUrl);
    if (!_remote && origin && *origin == _own)
        return true;
    std::string const named = origin ? origin->Describe() : std::string(sourceUrl.substr(0, 200));
    std::lock_guard const lock(_mutex);
    if (_logged.insert(named).second && _log)
        _log(fmt::format("the host channel answers only {}, so a message from {} was dropped", _own.Describe(), named));
    return false;
}
