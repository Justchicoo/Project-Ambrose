/*
 * Project Ambrose by Imjustchico
 * Reads the list as addresses and CIDR ranges, reporting each entry it cannot read rather than trusting it, and resolves a client's address by walking a forwarded header from the right while the hops are trusted; an untrusted peer, an empty list, a header that is only trusted hops or one that holds nothing readable all leave the peer's own address standing.
 */

#include "TrustedProxies.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <charconv>

namespace
{
    uint8 FullPrefix(asio::ip::address const& address)
    {
        return address.is_v6() ? uint8{ 128 } : uint8{ 32 };
    }

    std::vector<std::string_view> SplitList(std::string_view list)
    {
        std::vector<std::string_view> parts;
        for (std::string_view rest = list; !rest.empty();)
        {
            std::size_t const comma = rest.find(',');
            std::string_view const part = Ambrose::Trim(rest.substr(0, comma));
            if (!part.empty())
                parts.push_back(part);
            rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
        }
        return parts;
    }

    std::string_view StripPort(std::string_view text)
    {
        if (text.size() > 2 && text.front() == '[')
        {
            std::size_t const close = text.find(']');
            if (close != std::string_view::npos)
                return text.substr(1, close - 1);
        }
        std::size_t const colon = text.find(':');
        if (colon != std::string_view::npos && text.find(':', colon + 1) == std::string_view::npos)
            return text.substr(0, colon);
        return text;
    }
}

TrustedProxies TrustedProxies::Parse(std::string_view list, std::vector<std::string>* problems, std::string_view option)
{
    TrustedProxies trusted;
    for (std::string_view const entry : SplitList(list))
    {
        std::string_view addressText = entry;
        std::optional<uint8> prefix;
        if (std::size_t const slash = entry.find('/'); slash != std::string_view::npos)
        {
            addressText = Ambrose::Trim(entry.substr(0, slash));
            std::string_view const prefixText = Ambrose::Trim(entry.substr(slash + 1));
            unsigned value = 0;
            auto const [end, parsed] = std::from_chars(prefixText.data(), prefixText.data() + prefixText.size(), value);
            if (prefixText.empty() || parsed != std::errc() || end != prefixText.data() + prefixText.size() || value > 128)
            {
                if (problems)
                    problems->push_back(fmt::format("{} holds {}, whose prefix length is not a number from 0 to 128; it is left out", option, Ambrose::ForLog(entry)));
                continue;
            }
            prefix = static_cast<uint8>(value);
        }
        std::optional<asio::ip::address> const address = Ambrose::Asio::MakeAddress(addressText);
        if (!address)
        {
            if (problems)
                problems->push_back(fmt::format("{} holds {}, which is not an address or a CIDR range; it is left out", option, Ambrose::ForLog(entry)));
            continue;
        }
        asio::ip::address const unmapped = Ambrose::Asio::Unmap(*address);
        uint8 const length = prefix.value_or(FullPrefix(unmapped));
        if (length > FullPrefix(unmapped))
        {
            if (problems)
                problems->push_back(fmt::format("{} holds {}, whose prefix is longer than the address allows; it is left out", option, Ambrose::ForLog(entry)));
            continue;
        }
        trusted._networks.push_back({ unmapped, length });
    }
    return trusted;
}

bool TrustedProxies::Holds(asio::ip::address const& address) const
{
    asio::ip::address const unmapped = Ambrose::Asio::Unmap(address);
    for (Network const& network : _networks)
    {
        if (unmapped.is_v6() == network.Address.is_v6() && Ambrose::Asio::IsInNetwork(unmapped, network.Address, network.PrefixLength))
            return true;
    }
    return false;
}

bool TrustedProxies::Holds(std::string_view address) const
{
    std::optional<asio::ip::address> const parsed = Ambrose::Asio::MakeAddress(StripPort(Ambrose::Trim(address)));
    return parsed && Holds(*parsed);
}

std::string TrustedProxies::ClientAddress(std::string_view peer, std::string_view forwardedFor) const
{
    std::string const own(Ambrose::Trim(peer));
    if (_networks.empty() || forwardedFor.empty() || !Holds(own))
        return own;
    std::vector<std::string_view> const hops = SplitList(forwardedFor);
    for (std::size_t index = hops.size(); index > 0; --index)
    {
        std::string_view const hop = StripPort(hops[index - 1]);
        std::optional<asio::ip::address> const address = Ambrose::Asio::MakeAddress(hop);
        if (!address)
            return own;
        if (!Holds(*address))
            return Ambrose::Asio::Unmap(*address).to_string();
    }
    return own;
}
