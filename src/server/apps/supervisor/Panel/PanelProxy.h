/*
 * Project Ambrose by Imjustchico
 * The panel's reverse-proxy support: ready-made proxy configurations for the six products an operator may already run, rendered from templates of our own that forward only to the panel listener with the event socket's upgrade passed, timeouts longer than its 60-second silent close, buffering off for the socket, a body limit matching the listener's, HSTS set in one place only and the apps' control APIs and /metrics never proxied, and the connection check that reports the request the panel actually received with every mismatch explained and its fix named.
 */

#ifndef AMBROSE_PANELPROXY_H
#define AMBROSE_PANELPROXY_H

#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class ProxyProduct
{
    Caddy,
    Nginx,
    Apache,
    HAProxy,
    Traefik,
    IIS
};

std::optional<ProxyProduct> ParseProxyProduct(std::string_view name);
std::string_view NameOf(ProxyProduct product);
std::vector<ProxyProduct> AllProxyProducts();

struct ProxyConfigParams
{
    ProxyProduct Product = ProxyProduct::Nginx;
    std::string Hostname;
    std::string PanelBindIp = "127.0.0.1";
    uint16 PanelPort = 12080;
    bool IncludePatchserver = false;
    uint16 PatchserverPort = 12500;
    std::size_t MaxBodyBytes = 262144;
    std::string CertificateFile;
    std::string CertificateKeyFile;
};

std::string RenderProxyConfig(ProxyConfigParams const& params);

struct ProxyCheckInput
{
    std::string Peer;
    std::string ForwardedFor;
    std::string ForwardedProto;
    std::string ForwardedHost;
    std::string Forwarded;
    std::string TrustedProxies;
    std::string PublicUrl;
    std::string HostHeader;
    bool Secure = false;
    bool UpgradeReceived = false;
};

nlohmann::json CheckProxyRequest(ProxyCheckInput const& input);

#endif
