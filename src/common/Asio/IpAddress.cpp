/*
 * Project Ambrose by Imjustchico
 * Parses and classifies IP addresses, compares prefixes bit by bit, and formats endpoints with IPv6 brackets.
 */

#include "IpAddress.h"

#include <fmt/format.h>

#include <array>
#include <system_error>

namespace
{
    std::array<uint8, 16> ToBytes(asio::ip::address const& address, std::size_t& length)
    {
        std::array<uint8, 16> bytes{};
        if (address.is_v4())
        {
            asio::ip::address_v4::bytes_type const v4 = address.to_v4().to_bytes();
            std::copy(v4.begin(), v4.end(), bytes.begin());
            length = 4;
        }
        else
        {
            asio::ip::address_v6::bytes_type const v6 = address.to_v6().to_bytes();
            std::copy(v6.begin(), v6.end(), bytes.begin());
            length = 16;
        }
        return bytes;
    }
}

std::optional<asio::ip::address> Ambrose::Asio::MakeAddress(std::string_view text)
{
    if (text.size() >= 2 && text.front() == '[' && text.back() == ']')
        text = text.substr(1, text.size() - 2);
    if (text.empty() || text.find('%') != std::string_view::npos)
        return std::nullopt;
    std::error_code error;
    asio::ip::address const address = asio::ip::make_address(std::string(text), error);
    if (error)
        return std::nullopt;
    return address;
}

std::optional<asio::ip::tcp::endpoint> Ambrose::Asio::MakeEndpoint(std::string_view host, uint16 port)
{
    std::optional<asio::ip::address> const address = MakeAddress(host);
    if (!address)
        return std::nullopt;
    return asio::ip::tcp::endpoint(*address, port);
}

asio::ip::address Ambrose::Asio::Unmap(asio::ip::address const& address)
{
    if (address.is_v6() && address.to_v6().is_v4_mapped())
        return asio::ip::make_address_v4(asio::ip::v4_mapped, address.to_v6());
    return address;
}

bool Ambrose::Asio::IsLoopback(asio::ip::address const& address)
{
    return Unmap(address).is_loopback();
}

bool Ambrose::Asio::IsPrivate(asio::ip::address const& address)
{
    asio::ip::address const unmapped = Unmap(address);
    if (unmapped.is_v4())
    {
        uint32 const value = AddressToUInt(unmapped.to_v4());
        return (value >> 24) == 10 || (value >> 20) == 0xAC1 || (value >> 16) == 0xC0A8 || (value >> 16) == 0xA9FE;
    }
    asio::ip::address_v6 const v6 = unmapped.to_v6();
    asio::ip::address_v6::bytes_type const bytes = v6.to_bytes();
    return (bytes[0] & 0xFE) == 0xFC || v6.is_link_local();
}

bool Ambrose::Asio::IsUnspecified(asio::ip::address const& address)
{
    return Unmap(address).is_unspecified();
}

bool Ambrose::Asio::IsInNetwork(asio::ip::address const& address, asio::ip::address const& network, uint8 prefixLength)
{
    asio::ip::address left = Unmap(address);
    asio::ip::address right = network;
    if (network.is_v6() && network.to_v6().is_v4_mapped())
    {
        if (prefixLength >= 96)
        {
            right = Unmap(network);
            prefixLength = static_cast<uint8>(prefixLength - 96);
        }
        else if (left.is_v4())
            left = asio::ip::make_address_v6(asio::ip::v4_mapped, left.to_v4());
    }
    if (left.is_v4() != right.is_v4())
        return false;
    std::size_t length = 0;
    std::array<uint8, 16> const a = ToBytes(left, length);
    std::array<uint8, 16> const b = ToBytes(right, length);
    if (prefixLength > length * 8)
        return false;
    std::size_t const fullBytes = prefixLength / 8;
    for (std::size_t i = 0; i < fullBytes; ++i)
        if (a[i] != b[i])
            return false;
    uint8 const remainingBits = prefixLength % 8;
    if (remainingBits == 0)
        return true;
    uint8 const mask = static_cast<uint8>(0xFF << (8 - remainingBits));
    return (a[fullBytes] & mask) == (b[fullBytes] & mask);
}

uint32 Ambrose::Asio::AddressToUInt(asio::ip::address_v4 const& address)
{
    return address.to_uint();
}

asio::ip::address_v4 Ambrose::Asio::AddressFromUInt(uint32 value)
{
    return asio::ip::address_v4(value);
}

std::string Ambrose::Asio::ToString(asio::ip::tcp::endpoint const& endpoint)
{
    if (endpoint.address().is_v6())
        return fmt::format("[{}]:{}", endpoint.address().to_string(), endpoint.port());
    return fmt::format("{}:{}", endpoint.address().to_string(), endpoint.port());
}
