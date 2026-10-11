/*
 * Project Ambrose by Imjustchico
 * Binds a throwaway socket on the address and port asked for, TCP as an acceptor and UDP as a datagram socket, and calls the port free only when the bind takes; anything else holding it makes the bind fail with the operating system's reason.
 */

#include "PortProbe.h"
#include "IpAddress.h"
#include "StringUtil.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/socket_base.hpp>

#include <fmt/format.h>

#include <system_error>

namespace PortProbe
{

std::optional<Protocol> Parse(std::string_view text) noexcept
{
    if (Ambrose::EqualsIgnoreCase(text, "tcp"))
        return Protocol::Tcp;
    if (Ambrose::EqualsIgnoreCase(text, "udp"))
        return Protocol::Udp;
    if (Ambrose::EqualsIgnoreCase(text, "http"))
        return Protocol::Tcp;
    return std::nullopt;
}

std::string_view Name(Protocol protocol) noexcept
{
    return protocol == Protocol::Udp ? "udp" : "tcp";
}

bool IsFree(Protocol protocol, std::string_view address, uint16 port, std::string& error)
{
    error.clear();
    std::optional<asio::ip::address> const parsed = Ambrose::Asio::MakeAddress(address);
    if (!parsed)
    {
        error = fmt::format("{} is not an IP address", address);
        return false;
    }
    if (port == 0)
    {
        error = "port 0 asks the operating system to choose, so there is nothing to prove free";
        return false;
    }
    asio::io_context context;
    std::error_code code;
    if (protocol == Protocol::Udp)
    {
        asio::ip::udp::socket socket(context);
        socket.open(parsed->is_v6() ? asio::ip::udp::v6() : asio::ip::udp::v4(), code);
        if (code)
        {
            error = code.message();
            return false;
        }
        socket.bind(asio::ip::udp::endpoint(*parsed, port), code);
        if (code)
        {
            error = code.message();
            return false;
        }
        socket.close(code);
        return true;
    }
    asio::ip::tcp::acceptor acceptor(context);
    acceptor.open(parsed->is_v6() ? asio::ip::tcp::v6() : asio::ip::tcp::v4(), code);
    if (code)
    {
        error = code.message();
        return false;
    }
    acceptor.bind(asio::ip::tcp::endpoint(*parsed, port), code);
    if (code)
    {
        error = code.message();
        return false;
    }
    acceptor.close(code);
    return true;
}

}
