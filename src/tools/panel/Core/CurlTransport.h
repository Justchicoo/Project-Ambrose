/*
 * Project Ambrose by Imjustchico
 * The probe's real transport: libcurl over the OpenSSL the listeners already use, as the Outbound HTTP decision settles. A publicly trusted panel is verified the usual way; a pinned one is checked inside the TLS handshake, where the leaf certificate's SHA-256 must equal the pin, so a panel whose certificate changed fails the handshake and is sent nothing, and the fingerprint it served is kept to be named. Every request is a short GET with a timeout, and nothing is written to disk.
 */

#ifndef AMBROSE_CURLTRANSPORT_H
#define AMBROSE_CURLTRANSPORT_H

#include "PanelProbe.h"

#include <chrono>

class CurlTransport : public PanelTransport
{
public:
    static constexpr std::chrono::milliseconds Timeout{ 3000 };
    static constexpr std::size_t MaxBodyBytes = 65536;

    PanelReply Get(PanelAddress const& address, std::string_view path) override;
};

#endif
