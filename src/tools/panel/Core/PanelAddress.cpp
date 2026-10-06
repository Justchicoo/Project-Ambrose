/*
 * Project Ambrose by Imjustchico
 * Checks a panel address against the way it is trusted: plain HTTP only on loopback unless opted into, HTTPS for anything vouched for or pinned, a pin that is a whole SHA-256 fingerprint, and no path beyond the origin; reads a pairing line as the URL of the panel's #link page with its token and fingerprint in the fragment, and writes a fingerprint in groups of four bytes for comparing by eye.
 */

#include "PanelAddress.h"

#include "StringUtil.h"

#include <fmt/format.h>

#include <array>
#include <utility>

namespace
{
    constexpr std::array<std::pair<PanelTrust, std::string_view>, 4> Names{ {
        { PanelTrust::Loopback, "loopback" },
        { PanelTrust::Public, "public" },
        { PanelTrust::Pinned, "pinned" },
        { PanelTrust::PlainOptIn, "plain-http" },
    } };

    std::string_view FragmentValue(std::string_view fragment, std::string_view key)
    {
        std::size_t const query = fragment.find('?');
        if (query == std::string_view::npos)
            return {};
        std::string_view rest = fragment.substr(query + 1);
        while (!rest.empty())
        {
            std::size_t const amp = rest.find('&');
            std::string_view const pair = rest.substr(0, amp);
            std::size_t const equals = pair.find('=');
            if (equals != std::string_view::npos && pair.substr(0, equals) == key)
                return pair.substr(equals + 1);
            rest = amp == std::string_view::npos ? std::string_view() : rest.substr(amp + 1);
        }
        return {};
    }
}

bool PanelAddress::IsLoopback(ShellOrigin const& origin)
{
    return origin.Host == "127.0.0.1" || origin.Host == "localhost" || origin.Host == "[::1]";
}

std::string_view PanelAddress::Name(PanelTrust trust)
{
    for (auto const& [known, name] : Names)
        if (known == trust)
            return name;
    return "public";
}

std::optional<PanelTrust> PanelAddress::TrustNamed(std::string_view name)
{
    for (auto const& [known, text] : Names)
        if (text == name)
            return known;
    return std::nullopt;
}

std::string PanelAddress::Grouped(std::string_view fingerprint)
{
    std::string const normal = ShellRules::NormalFingerprint(fingerprint);
    std::string grouped;
    for (std::size_t index = 0; index < normal.size(); index += 12)
    {
        if (!grouped.empty())
            grouped.push_back(' ');
        grouped.append(normal, index, std::min<std::size_t>(11, normal.size() - index));
    }
    return grouped;
}

std::string PanelAddress::Warning() const
{
    if (Trust != PanelTrust::PlainOptIn)
        return std::string();
    return fmt::format("{} is reached over plain HTTP beyond this computer, so the password and the session cross the network unencrypted", Origin.Describe());
}

std::optional<PanelAddress> PanelAddress::Parse(std::string_view text, PanelTrust trust, std::string_view pin, std::string& error)
{
    std::string const trimmed(Ambrose::Trim(text));
    std::optional<ShellOrigin> const origin = ShellOrigin::Of(trimmed);
    if (!origin || (origin->Scheme != "http" && origin->Scheme != "https"))
    {
        error = fmt::format("{} is not an http or https address", trimmed);
        return std::nullopt;
    }
    std::string_view const after = std::string_view(trimmed).substr(trimmed.find("//") + 2);
    std::size_t const path = after.find_first_of("/?#");
    if (path != std::string_view::npos && after.substr(path) != "/")
    {
        error = fmt::format("{} names a page; a panel is added by its address alone, {}", trimmed, origin->Describe());
        return std::nullopt;
    }

    PanelAddress address;
    address.Origin = *origin;
    address.Trust = trust;
    bool const loopback = IsLoopback(*origin);
    if (origin->Scheme == "http")
    {
        if (trust == PanelTrust::PlainOptIn)
            return address;
        if (!loopback)
        {
            error = fmt::format("{} is plain HTTP beyond this computer, where the password and the session would cross the network unencrypted; use https, or take the plain HTTP opt-in for this entry", origin->Describe());
            return std::nullopt;
        }
        address.Trust = PanelTrust::Loopback;
        return address;
    }
    if (trust == PanelTrust::PlainOptIn || trust == PanelTrust::Loopback)
    {
        address.Trust = PanelTrust::Public;
        return address;
    }
    if (trust == PanelTrust::Pinned)
    {
        address.Pin = ShellRules::NormalFingerprint(pin);
        if (address.Pin.empty())
        {
            error = fmt::format("a pinned panel needs the whole SHA-256 fingerprint of its certificate, and '{}' is not one", pin);
            return std::nullopt;
        }
    }
    return address;
}

std::optional<PanelPairing> PanelPairing::Parse(std::string_view line, std::string& error)
{
    std::string const trimmed(Ambrose::Trim(line));
    std::size_t const hash = trimmed.find('#');
    if (hash == std::string::npos || !std::string_view(trimmed).substr(hash + 1).starts_with("link?"))
    {
        error = "a pairing line is the panel's #link address the supervisor printed, and this is not one";
        return std::nullopt;
    }
    std::string_view const fragment = std::string_view(trimmed).substr(hash + 1);
    std::string const token(FragmentValue(fragment, "token"));
    std::string const pin(FragmentValue(fragment, "sha256"));
    if (token.empty())
    {
        error = "the pairing line carries no token";
        return std::nullopt;
    }
    std::optional<ShellOrigin> const origin = ShellOrigin::Of(trimmed);
    if (!origin)
    {
        error = "the pairing line names no address";
        return std::nullopt;
    }
    PanelTrust const trust = pin.empty() ? PanelTrust::Loopback : PanelTrust::Pinned;
    if (pin.empty() && (origin->Scheme != "http" || !PanelAddress::IsLoopback(*origin)))
    {
        error = "a pairing line for a panel beyond this computer carries the certificate's fingerprint, and this one does not";
        return std::nullopt;
    }
    std::optional<PanelAddress> const address = PanelAddress::Parse(origin->Describe(), trust, pin, error);
    if (!address)
        return std::nullopt;
    PanelPairing pairing;
    pairing.Address = *address;
    pairing.Token = token;
    pairing.Link = trimmed;
    return pairing;
}
