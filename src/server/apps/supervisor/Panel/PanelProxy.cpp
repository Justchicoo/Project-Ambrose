/*
 * Project Ambrose by Imjustchico
 * The panel's reverse-proxy support: ready-made proxy configurations for the six products an operator may already run, rendered from templates of our own that forward only to the panel listener with the event socket's upgrade passed, timeouts longer than its 60-second silent close, buffering off for the socket, a body limit matching the listener's, HSTS set in one place only and the apps' control APIs and /metrics never proxied, and the connection check that reports the request the panel actually received with every mismatch explained and its fix named.
 */

#include "PanelProxy.h"

#include "StringUtil.h"
#include "TrustedProxies.h"

#include <nlohmann/json.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <map>

namespace
{
    constexpr int SocketTimeoutSeconds = 90;
    constexpr std::string_view HstsValue = "max-age=31536000; includeSubDomains";
    constexpr std::string_view SocketPath = "/api/panel/events";

    std::string Substitute(std::string text, std::map<std::string, std::string, std::less<>> const& values)
    {
        for (auto const& [name, value] : values)
        {
            std::string const token = "{" + name + "}";
            for (std::size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size()))
                text.replace(at, token.size(), value);
        }
        return text;
    }

    std::map<std::string, std::string, std::less<>> BaseValues(ProxyConfigParams const& params)
    {
        return {
            { "hostname", params.Hostname },
            { "panel_ip", params.PanelBindIp },
            { "panel_port", std::to_string(params.PanelPort) },
            { "patch_port", std::to_string(params.PatchserverPort) },
            { "body_bytes", std::to_string(params.MaxBodyBytes) },
            { "body_kb", std::to_string((params.MaxBodyBytes + 1023) / 1024) },
            { "socket_timeout", std::to_string(SocketTimeoutSeconds) },
            { "hsts", std::string(HstsValue) },
            { "socket_path", std::string(SocketPath) },
            { "cert_file", params.CertificateFile.empty() ? "/etc/ssl/ambrose/" + params.Hostname + ".crt" : params.CertificateFile },
            { "cert_key", params.CertificateKeyFile.empty() ? "/etc/ssl/ambrose/" + params.Hostname + ".key" : params.CertificateKeyFile },
            { "cert_pem", params.CertificateFile.empty() ? "/etc/ssl/ambrose/" + params.Hostname + ".pem" : params.CertificateFile },
        };
    }

    std::string NginxTemplate()
    {
        return R"NGINX(# Project Ambrose by Imjustchico
# Reverse proxy for the Ambrose panel: {hostname} -> {panel_ip}:{panel_port}.
# Only the panel listener is forwarded; the apps' own control listeners and /metrics are never proxied.
# Point ssl_certificate and ssl_certificate_key at the certificate for {hostname}, then check with: nginx -t -c <this file>

user www-data;
worker_processes auto;
pid /run/nginx-ambrose-panel.pid;
error_log /var/log/nginx/ambrose-panel-error.log;

events {
    worker_connections 768;
}

http {
    access_log /var/log/nginx/ambrose-panel-access.log;

    upstream ambrose_panel {
        server {panel_ip}:{panel_port};
    }

    server {
        listen 443 ssl;
        server_name {hostname};

        ssl_certificate {cert_file};
        ssl_certificate_key {cert_key};

        # HSTS is set here and nowhere else.
        add_header Strict-Transport-Security "{hsts}" always;

        # Matches the panel listener's own body limit.
        client_max_body_size {body_bytes};

        location {socket_path} {
            proxy_pass http://ambrose_panel;
            proxy_http_version 1.1;
            proxy_set_header Upgrade $http_upgrade;
            proxy_set_header Connection "upgrade";
            proxy_set_header Host $host;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
            proxy_set_header X-Forwarded-Host $host;
            # Longer than the event socket's 60-second silent close.
            proxy_read_timeout {socket_timeout}s;
            proxy_send_timeout {socket_timeout}s;
            # Buffering off for the socket.
            proxy_buffering off;
        }

        location / {
            proxy_pass http://ambrose_panel;
            proxy_set_header Host $host;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;
            proxy_set_header X-Forwarded-Host $host;
            proxy_read_timeout {socket_timeout}s;
        }
    }

    server {
        listen 80;
        server_name {hostname};
        # Plain HTTP is refused: the panel is served over TLS only.
        return 444;
    }
}
)NGINX";
    }

    std::string CaddyTemplate()
    {
        return R"CADDY(# Project Ambrose by Imjustchico
# Reverse proxy for the Ambrose panel: {hostname} -> {panel_ip}:{panel_port}.
# Only the panel listener is forwarded; the apps' own control listeners and /metrics are never proxied.
# Caddy sets HSTS on HTTPS sites itself, so it is not set again here. Check with: caddy validate --config --adapter caddyfile --file <this file>

{hostname} {
    # Matches the panel listener's own body limit.
    request_body {
        max_size {body_bytes}
    }

    handle {socket_path} {
        reverse_proxy {panel_ip}:{panel_port} {
            header_up Host {host}
            header_up X-Forwarded-For {remote_host}
            header_up X-Forwarded-Proto {scheme}
            header_up X-Forwarded-Host {host}
            # Longer than the event socket's 60-second silent close.
            transport http {
                dial_timeout {socket_timeout}s
                response_header_timeout {socket_timeout}s
            }
            # Buffering off for the socket.
            flush_interval -1
        }
    }

    handle {
        reverse_proxy {panel_ip}:{panel_port} {
            header_up Host {host}
            header_up X-Forwarded-For {remote_host}
            header_up X-Forwarded-Proto {scheme}
            header_up X-Forwarded-Host {host}
            transport http {
                dial_timeout {socket_timeout}s
                response_header_timeout {socket_timeout}s
            }
        }
    }
}
)CADDY";
    }

    std::string ApacheTemplate()
    {
        return R"APACHE(# Project Ambrose by Imjustchico
# Reverse proxy for the Ambrose panel: {hostname} -> {panel_ip}:{panel_port}.
# Only the panel listener is forwarded; the apps' own control listeners and /metrics are never proxied.
# Needs mod_ssl, mod_proxy, mod_proxy_http, mod_proxy_wstunnel, mod_headers and mod_rewrite. Check with: apache2 -t -f <this file>

ServerRoot "/etc/apache2"
Listen 443
ErrorLog /var/log/apache2/ambrose-panel-error.log
LoadModule mpm_event_module /usr/lib/apache2/modules/mod_mpm_event.so
LoadModule ssl_module /usr/lib/apache2/modules/mod_ssl.so
LoadModule proxy_module /usr/lib/apache2/modules/mod_proxy.so
LoadModule proxy_http_module /usr/lib/apache2/modules/mod_proxy_http.so
LoadModule proxy_wstunnel_module /usr/lib/apache2/modules/mod_proxy_wstunnel.so
LoadModule headers_module /usr/lib/apache2/modules/mod_headers.so
LoadModule rewrite_module /usr/lib/apache2/modules/mod_rewrite.so

<VirtualHost *:443>
    ServerName {hostname}

    SSLEngine on
    SSLCertificateFile {cert_file}
    SSLCertificateKeyFile {cert_key}

    # HSTS is set here and nowhere else.
    Header always set Strict-Transport-Security "{hsts}"

    # Matches the panel listener's own body limit.
    LimitRequestBody {body_bytes}

    ProxyPreserveHost On
    RequestHeader set X-Forwarded-Proto "https"
    RequestHeader set X-Forwarded-Host "{hostname}"
    RequestHeader append X-Forwarded-For "%{REMOTE_ADDR}s"

    # The event socket: the upgrade is passed, buffering stays off and timeouts outlast its 60-second silent close.
    ProxyPass {socket_path} ws://{panel_ip}:{panel_port}{socket_path}
    ProxyPassReverse {socket_path} ws://{panel_ip}:{panel_port}{socket_path}

    ProxyPass / http://{panel_ip}:{panel_port}/
    ProxyPassReverse / http://{panel_ip}:{panel_port}/
    # Longer than the event socket's 60-second silent close.
    ProxyTimeout {socket_timeout}
</VirtualHost>
)APACHE";
    }

    std::string HAProxyTemplate()
    {
        return R"HAPROXY(# Project Ambrose by Imjustchico
# Reverse proxy for the Ambrose panel: {hostname} -> {panel_ip}:{panel_port}.
# Only the panel listener is forwarded; the apps' own control listeners and /metrics are never proxied.
# Bundle the certificate and key for {hostname} into one PEM for the bind line below
# (for example: cat panel.crt panel.key > panel.pem). Check with: haproxy -c -f <this file>

global
    maxconn 2048

defaults
    mode http
    timeout connect 5s
    timeout client {socket_timeout}s
    timeout server {socket_timeout}s
    # Longer than the event socket's 60-second silent close.
    timeout tunnel {socket_timeout}s

frontend ambrose_panel_https
    bind *:443 ssl crt {cert_pem}
    # HSTS is set here and nowhere else.
    http-response set-header Strict-Transport-Security "{hsts}"
    acl is_socket path {socket_path}
    # Matches the panel listener's own body limit.
    acl body_too_large req.body_len gt {body_bytes}
    http-request deny deny_status 413 if body_too_large
    use_backend ambrose_panel_socket if is_socket
    default_backend ambrose_panel

backend ambrose_panel
    http-request set-header X-Forwarded-Proto https
    http-request set-header X-Forwarded-Host {hostname}
    option forwardfor
    server panel {panel_ip}:{panel_port} maxconn 256

backend ambrose_panel_socket
    # Buffering off for the socket.
    no option http-buffer-request
    http-request set-header X-Forwarded-Proto https
    http-request set-header X-Forwarded-Host {hostname}
    option forwardfor
    server panel {panel_ip}:{panel_port} maxconn 256
)HAPROXY";
    }

    std::string TraefikTemplate()
    {
        return R"TRAEFIK(# Project Ambrose by Imjustchico
# Reverse proxy for the Ambrose panel: {hostname} -> {panel_ip}:{panel_port}.
# Only the panel listener is forwarded; the apps' own control listeners and /metrics are never proxied.
# Traefik streams the event socket without buffering and forwards X-Forwarded-* itself.

[http.routers]
  [http.routers.ambrose-panel]
    rule = "Host(`{hostname}`)"
    service = "ambrose-panel"
    entryPoints = ["websecure"]
    middlewares = ["ambrose-panel-hsts", "ambrose-panel-limits"]
    [http.routers.ambrose-panel.tls]

[http.services]
  [http.services.ambrose-panel.loadBalancer]
    serversTransport = "ambrose-panel"
    # The event socket at /api/panel/events streams through this service.
    # Longer than the event socket's 60-second silent close.
    [http.services.ambrose-panel.loadBalancer.responseForwarding]
      flushInterval = "100ms"
    [[http.services.ambrose-panel.loadBalancer.servers]]
      url = "http://{panel_ip}:{panel_port}"

[http.serversTransports]
  [http.serversTransports.ambrose-panel]
    dialTimeout = "90s"
    responseHeaderTimeout = "90s"

[http.middlewares]
  # HSTS is set here and nowhere else.
  [http.middlewares.ambrose-panel-hsts.headers]
    stsSeconds = 31536000
    stsIncludeSubdomains = true
  [http.middlewares.ambrose-panel-limits.buffering]
    # Matches the panel listener's own body limit.
    maxRequestBodyBytes = {body_bytes}
)TRAEFIK";
    }

    std::string IISTemplate()
    {
        return R"IIS(<?xml version="1.0" encoding="UTF-8"?>
<!-- Project Ambrose by Imjustchico -->
<!-- Reverse proxy for the Ambrose panel: {hostname} -> {panel_ip}:{panel_port}. -->
<!-- Only the panel listener is forwarded; the apps' own control listeners and /metrics are never proxied. -->
<!-- Needs Application Request Routing with the WebSocket module; the upgrade is passed and ARR streams without buffering. -->
<configuration>
  <system.webServer>
    <rewrite>
      <rules>
        <!-- The event socket at /api/panel/events is streamed without buffering. -->
        <rule name="Ambrose panel socket" stopProcessing="true">
          <match url="^api/panel/events$" />
          <action type="Rewrite" url="http://{panel_ip}:{panel_port}/{R:0}" />
          <serverVariables>
            <set name="HTTP_X_FORWARDED_PROTO" value="https" />
            <set name="HTTP_X_FORWARDED_HOST" value="{hostname}" />
          </serverVariables>
        </rule>
        <rule name="Ambrose panel" stopProcessing="true">
          <match url="^(.*)$" />
          <action type="Rewrite" url="http://{panel_ip}:{panel_port}/{R:1}" />
          <serverVariables>
            <set name="HTTP_X_FORWARDED_PROTO" value="https" />
            <set name="HTTP_X_FORWARDED_HOST" value="{hostname}" />
          </serverVariables>
        </rule>
      </rules>
    </rewrite>
    <httpProtocol>
      <customHeaders>
        <!-- HSTS is set here and nowhere else. -->
        <add name="Strict-Transport-Security" value="{hsts}" />
      </customHeaders>
    </httpProtocol>
    <security>
      <requestFiltering>
        <!-- Matches the panel listener's own body limit. -->
        <requestLimits maxAllowedContentLength="{body_bytes}" />
      </requestFiltering>
    </security>
  </system.webServer>
  <system.web>
    <!-- Longer than the event socket's 60-second silent close. -->
    <httpRuntime executionTimeout="90" />
  </system.web>
</configuration>
)IIS";
    }

    std::string PatchserverNginx(std::map<std::string, std::string, std::less<>> const& values)
    {
        return Substitute(R"NGINX(
    # The patchserver serves patch downloads; its control listener is never proxied.
    location /patch/ {
        proxy_pass http://{panel_ip}:{patch_port}/;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_set_header X-Forwarded-Host $host;
        proxy_read_timeout {socket_timeout}s;
    }
)NGINX", values);
    }

    void MaybeAddPatchserver(std::string& config, ProxyConfigParams const& params, std::map<std::string, std::string, std::less<>> const& values)
    {
        if (!params.IncludePatchserver)
            return;
        switch (params.Product)
        {
            case ProxyProduct::Nginx:
            {
                std::string const marker = "    }\n\n    server {\n        listen 80;";
                std::size_t const at = config.find(marker);
                if (at != std::string::npos)
                    config.insert(at, PatchserverNginx(values));
                break;
            }
            case ProxyProduct::Caddy:
            {
                std::size_t const at = config.rfind('}');
                if (at != std::string::npos)
                    config.insert(at, Substitute(R"CADDY(
    # The patchserver serves patch downloads; its control listener is never proxied.
    handle /patch/* {
        uri strip_prefix /patch
        reverse_proxy {panel_ip}:{patch_port} {
            header_up Host {host}
            header_up X-Forwarded-For {remote_host}
            header_up X-Forwarded-Proto {scheme}
            header_up X-Forwarded-Host {host}
        }
    }
)CADDY", values));
                break;
            }
            default:
                break;
        }
    }

    std::string FirstToken(std::string_view value)
    {
        value = Ambrose::Trim(value);
        std::size_t const comma = value.find(',');
        return Ambrose::ToLower(Ambrose::Trim(value.substr(0, comma)));
    }

    void ParsePublicUrl(std::string_view url, std::string& scheme, std::string& host)
    {
        url = Ambrose::Trim(url);
        std::size_t const sep = url.find("://");
        if (sep == std::string_view::npos)
            return;
        scheme = Ambrose::ToLower(url.substr(0, sep));
        std::string_view rest = url.substr(sep + 3);
        std::size_t const slash = rest.find('/');
        rest = rest.substr(0, slash);
        if (rest.starts_with('['))
        {
            std::size_t const close = rest.find(']');
            host = close == std::string_view::npos ? std::string() : Ambrose::ToLower(rest.substr(1, close - 1));
            return;
        }
        std::size_t const colon = rest.find(':');
        if (colon != std::string_view::npos && rest.find(':', colon + 1) == std::string_view::npos)
            rest = rest.substr(0, colon);
        host = Ambrose::ToLower(rest);
    }
}

std::optional<ProxyProduct> ParseProxyProduct(std::string_view name)
{
    std::string const lower = Ambrose::ToLower(Ambrose::Trim(name));
    if (lower == "caddy")
        return ProxyProduct::Caddy;
    if (lower == "nginx")
        return ProxyProduct::Nginx;
    if (lower == "apache" || lower == "httpd" || lower == "apache httpd")
        return ProxyProduct::Apache;
    if (lower == "haproxy")
        return ProxyProduct::HAProxy;
    if (lower == "traefik")
        return ProxyProduct::Traefik;
    if (lower == "iis" || lower == "iis-arr" || lower == "arr")
        return ProxyProduct::IIS;
    return std::nullopt;
}

std::string_view NameOf(ProxyProduct product)
{
    switch (product)
    {
        case ProxyProduct::Caddy: return "caddy";
        case ProxyProduct::Nginx: return "nginx";
        case ProxyProduct::Apache: return "apache";
        case ProxyProduct::HAProxy: return "haproxy";
        case ProxyProduct::Traefik: return "traefik";
        case ProxyProduct::IIS: return "iis";
    }
    return "nginx";
}

std::vector<ProxyProduct> AllProxyProducts()
{
    return { ProxyProduct::Caddy, ProxyProduct::Nginx, ProxyProduct::Apache, ProxyProduct::HAProxy, ProxyProduct::Traefik, ProxyProduct::IIS };
}

std::string RenderProxyConfig(ProxyConfigParams const& params)
{
    std::map<std::string, std::string, std::less<>> const values = BaseValues(params);
    std::string config;
    switch (params.Product)
    {
        case ProxyProduct::Caddy: config = CaddyTemplate(); break;
        case ProxyProduct::Nginx: config = NginxTemplate(); break;
        case ProxyProduct::Apache: config = ApacheTemplate(); break;
        case ProxyProduct::HAProxy: config = HAProxyTemplate(); break;
        case ProxyProduct::Traefik: config = TraefikTemplate(); break;
        case ProxyProduct::IIS: config = IISTemplate(); break;
    }
    config = Substitute(config, values);
    MaybeAddPatchserver(config, params, values);
    return config;
}

nlohmann::json CheckProxyRequest(ProxyCheckInput const& input)
{
    TrustedProxies const trusted = TrustedProxies::Parse(input.TrustedProxies);
    bool const peerTrusted = trusted.Holds(input.Peer);
    bool const hasForwarded = !input.ForwardedFor.empty() || !input.ForwardedProto.empty() || !input.ForwardedHost.empty() || !input.Forwarded.empty();

    nlohmann::json forwarded = nlohmann::json::object();
    if (!input.ForwardedFor.empty())
        forwarded["x_forwarded_for"] = input.ForwardedFor;
    if (!input.ForwardedProto.empty())
        forwarded["x_forwarded_proto"] = input.ForwardedProto;
    if (!input.ForwardedHost.empty())
        forwarded["x_forwarded_host"] = input.ForwardedHost;
    if (!input.Forwarded.empty())
        forwarded["forwarded"] = input.Forwarded;

    std::string const client = trusted.ClientAddress(input.Peer, input.ForwardedFor);
    std::string scheme = input.Secure ? "https" : "http";
    if (peerTrusted && !input.ForwardedProto.empty())
        scheme = FirstToken(input.ForwardedProto);
    std::string host = Ambrose::ToLower(Ambrose::Trim(input.HostHeader));
    if (std::size_t const colon = host.find(':'); colon != std::string::npos && host.find(':', colon + 1) == std::string_view::npos)
        host = host.substr(0, colon);
    if (peerTrusted && !input.ForwardedHost.empty())
        host = FirstToken(input.ForwardedHost);

    std::string urlScheme;
    std::string urlHost;
    ParsePublicUrl(input.PublicUrl, urlScheme, urlHost);

    nlohmann::json problems = nlohmann::json::array();
    if (hasForwarded && !peerTrusted)
    {
        problems.push_back({
            { "code", "untrusted_proxy" },
            { "message", "Forwarded headers arrived from " + input.Peer + ", which is not in Panel.TrustedProxies, so they are ignored and every throttle and audit row records the proxy's address." },
            { "fix", "Set Panel.TrustedProxies to " + input.Peer + " on the reverse-proxy page after reviewing the generated config; a problem record was written for this peer." },
        });
    }
    if (peerTrusted && hasForwarded && input.ForwardedProto.empty())
    {
        problems.push_back({
            { "code", "missing_forwarded_proto" },
            { "message", "The trusted proxy did not send X-Forwarded-Proto, so the panel cannot tell whether the operator used http or https." },
            { "fix", "The generated configs set X-Forwarded-Proto; compare the proxy's running config with the one the page wrote." },
        });
    }
    if (hasForwarded && !urlScheme.empty() && scheme != urlScheme)
    {
        problems.push_back({
            { "code", "scheme_mismatch" },
            { "message", "The request arrived as " + scheme + " but the public URL is " + urlScheme + "." },
            { "fix", "Serve the panel over TLS at the proxy and let it send X-Forwarded-Proto, or correct Panel.PublicUrl." },
        });
    }
    if (hasForwarded && !urlHost.empty() && host != urlHost)
    {
        problems.push_back({
            { "code", "host_mismatch" },
            { "message", "The request arrived for host " + host + " but the public URL names " + urlHost + "." },
            { "fix", "Point the proxy's server name at " + urlHost + ", or correct Panel.PublicUrl." },
        });
    }

    return {
        { "peer", input.Peer },
        { "forwarded_headers", std::move(forwarded) },
        { "peer_trusted", peerTrusted },
        { "trusted_proxies", input.TrustedProxies },
        { "client_address", client },
        { "scheme", scheme },
        { "host", host },
        { "public_url", input.PublicUrl },
        { "socket_upgrade_received", input.UpgradeReceived },
        { "problems", std::move(problems) },
    };
}
