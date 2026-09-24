/*
 * Project Ambrose by Imjustchico
 * Resolves host names through asio, skipping DNS for numeric addresses and filtering results by family.
 */

#include "Resolver.h"
#include "IpAddress.h"

#include <algorithm>

Ambrose::Asio::Resolver::Resolver(IoContext& context) : _impl(context.GetImpl())
{
}

std::vector<asio::ip::tcp::endpoint> Ambrose::Asio::Resolver::ResolveAll(std::string const& host, uint16 port, ResolveFamily family, std::error_code& error)
{
    error.clear();
    std::vector<asio::ip::tcp::endpoint> endpoints;
    auto const matches = [family](asio::ip::tcp::endpoint const& endpoint)
    {
        return family == ResolveFamily::Any || (family == ResolveFamily::V4) == Unmap(endpoint.address()).is_v4();
    };

    if (std::optional<asio::ip::tcp::endpoint> const numeric = MakeEndpoint(host, port))
    {
        if (matches(*numeric))
            endpoints.push_back(*numeric);
        else
            error = asio::error::address_family_not_supported;
        return endpoints;
    }

    asio::ip::tcp::resolver::results_type results;
    std::string const service = std::to_string(port);
    asio::ip::resolver_base::flags const flags = asio::ip::resolver_base::numeric_service;
    if (family == ResolveFamily::V4)
        results = _impl.resolve(asio::ip::tcp::v4(), host, service, flags, error);
    else if (family == ResolveFamily::V6)
        results = _impl.resolve(asio::ip::tcp::v6(), host, service, flags, error);
    else
        results = _impl.resolve(host, service, flags, error);
    if (error)
        return endpoints;
    for (asio::ip::tcp::resolver::results_type::value_type const& entry : results)
    {
        asio::ip::tcp::endpoint const endpoint = entry.endpoint();
        if (matches(endpoint) && std::find(endpoints.begin(), endpoints.end(), endpoint) == endpoints.end())
            endpoints.push_back(endpoint);
    }
    std::stable_partition(endpoints.begin(), endpoints.end(), [](asio::ip::tcp::endpoint const& endpoint) { return endpoint.address().is_v4(); });
    if (endpoints.empty())
        error = asio::error::host_not_found;
    return endpoints;
}

std::optional<asio::ip::tcp::endpoint> Ambrose::Asio::Resolver::Resolve(std::string const& host, uint16 port, ResolveFamily family)
{
    std::error_code error;
    std::vector<asio::ip::tcp::endpoint> const endpoints = ResolveAll(host, port, family, error);
    if (error || endpoints.empty())
        return std::nullopt;
    return endpoints.front();
}

void Ambrose::Asio::Resolver::Cancel()
{
    _impl.cancel();
}
