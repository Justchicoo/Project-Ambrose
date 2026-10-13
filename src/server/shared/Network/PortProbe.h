/*
 * Project Ambrose by Imjustchico
 * Proves a port is free by binding it the way a listener would, for TCP and UDP on IPv4 and IPv6, so the panel refuses an allocation another program already holds and the apps refuse a listen setting that would not bind.
 */

#ifndef AMBROSE_PORTPROBE_H
#define AMBROSE_PORTPROBE_H

#include "Types.h"

#include <optional>
#include <string>
#include <string_view>

namespace PortProbe
{

enum class Protocol : uint8
{
    Tcp,
    Udp
};

std::optional<Protocol> Parse(std::string_view text) noexcept;

bool IsFree(Protocol protocol, std::string_view address, uint16 port, std::string& error);

}

#endif
