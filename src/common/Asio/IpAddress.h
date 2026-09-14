/*
 * Project Ambrose by Imjustchico
 * IPv4 and IPv6 address parsing, loopback and private-range checks, subnet matching, and endpoint formatting.
 */

#ifndef AMBROSE_IPADDRESS_H
#define AMBROSE_IPADDRESS_H

#include "Types.h"

#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace Ambrose::Asio
{
    std::optional<asio::ip::address> MakeAddress(std::string_view text);
    std::optional<asio::ip::tcp::endpoint> MakeEndpoint(std::string_view host, uint16 port);
    asio::ip::address Unmap(asio::ip::address const& address);
    bool IsLoopback(asio::ip::address const& address);
    bool IsPrivate(asio::ip::address const& address);
    bool IsUnspecified(asio::ip::address const& address);
    bool IsInNetwork(asio::ip::address const& address, asio::ip::address const& network, uint8 prefixLength);
    uint32 AddressToUInt(asio::ip::address_v4 const& address);
    asio::ip::address_v4 AddressFromUInt(uint32 value);
    std::string ToString(asio::ip::tcp::endpoint const& endpoint);
}

#endif
