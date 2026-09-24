/*
 * Project Ambrose by Imjustchico
 * A small HTTP/1.1 client for one app's admin API on this machine: it sends one request, over TLS when the listener serves it, with the app's bearer token, a Host header the listener accepts and the caller's request id, reads the whole answer before a deadline, and reports the status, the headers, the body and the fingerprint of the certificate it was served, or why no answer came, so the supervisor reads health, asks for a shutdown and relays the panel's requests without a library of its own.
 */

#ifndef AMBROSE_ADMINCLIENT_H
#define AMBROSE_ADMINCLIENT_H

#include "Types.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct AdminClientRequest
{
    std::string Method = "GET";
    std::string Path;
    std::string Body;
    std::string ContentType = "application/json";
    std::string RequestId;
    std::vector<std::pair<std::string, std::string>> Headers = {};
};

struct AdminClientResponse
{
    bool Answered = false;
    int Status = 0;
    std::string ContentType;
    std::string RequestId;
    std::string Head;
    std::string Body;
    std::string Error;
    std::string PeerFingerprint;
};

class AdminClient
{
public:
    static constexpr std::size_t MaxResponseBytes = 16 * 1024 * 1024;

    AdminClient(std::string host, uint16 port, std::string token, bool tls = false);

    AdminClientResponse Send(AdminClientRequest const& request, std::chrono::milliseconds timeout) const;

    std::string const& GetHost() const noexcept { return _host; }
    uint16 GetPort() const noexcept { return _port; }
    bool UsesTls() const noexcept { return _tls; }

    static std::string ConnectHost(std::string_view bindIp);
    static std::optional<AdminClientResponse> Parse(std::string_view raw, std::string& error);

private:
    std::string _host;
    uint16 _port;
    std::string _token;
    bool _tls = false;
};

#endif
