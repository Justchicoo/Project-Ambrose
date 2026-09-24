/*
 * Project Ambrose by Imjustchico
 * The hops a listener believes about a client's address: with none configured the peer's own address is the client's and every forwarded header is ignored, and with some configured a forwarded list is read from the right, past the hops that are trusted, to the first address that is not, so a peer nobody trusts cannot name any address it likes and have a throttle, an audit row or a sign-in record believe it.
 */

#ifndef AMBROSE_TRUSTEDPROXIES_H
#define AMBROSE_TRUSTEDPROXIES_H

#include "IpAddress.h"
#include "Types.h"

#include <string>
#include <string_view>
#include <vector>

class TrustedProxies
{
public:
    static TrustedProxies Parse(std::string_view list, std::vector<std::string>* problems = nullptr, std::string_view option = "TrustedProxies");

    bool IsEmpty() const { return _networks.empty(); }
    std::size_t Count() const { return _networks.size(); }
    bool Holds(std::string_view address) const;
    std::string ClientAddress(std::string_view peer, std::string_view forwardedFor) const;

private:
    struct Network
    {
        asio::ip::address Address;
        uint8 PrefixLength = 0;
    };

    bool Holds(asio::ip::address const& address) const;

    std::vector<Network> _networks;
};

#endif
