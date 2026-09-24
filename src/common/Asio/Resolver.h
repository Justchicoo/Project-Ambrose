/*
 * Project Ambrose by Imjustchico
 * Synchronous host name resolution to TCP endpoints with an address family filter and IPv4 preference.
 */

#ifndef AMBROSE_RESOLVER_H
#define AMBROSE_RESOLVER_H

#include "IoContext.h"
#include "Types.h"

#include <asio/ip/tcp.hpp>

#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace Ambrose::Asio
{
    enum class ResolveFamily : uint8
    {
        Any,
        V4,
        V6
    };

    class Resolver
    {
    public:
        explicit Resolver(IoContext& context);

        Resolver(Resolver const&) = delete;
        Resolver& operator=(Resolver const&) = delete;

        std::vector<asio::ip::tcp::endpoint> ResolveAll(std::string const& host, uint16 port, ResolveFamily family, std::error_code& error);
        std::optional<asio::ip::tcp::endpoint> Resolve(std::string const& host, uint16 port, ResolveFamily family = ResolveFamily::Any);
        void Cancel();

    private:
        asio::ip::tcp::resolver _impl;
    };
}

#endif
