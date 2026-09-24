/*
 * Project Ambrose by Imjustchico
 * The login server's shutdown notice: stops accepting clients, tells every connected session the server is shutting down, then waits up to the grace period for the notices to be written.
 */

#ifndef AMBROSE_LOGINSHUTDOWN_H
#define AMBROSE_LOGINSHUTDOWN_H

#include "LoginSession.h"
#include "SocketMgr.h"

#include <chrono>
#include <cstddef>

namespace LoginShutdown
{
    inline constexpr std::chrono::milliseconds DrainPollInterval{ 20 };

    std::size_t NotifyAndDrain(SocketMgr<LoginSession>& sockets, std::chrono::milliseconds grace, uint32 message = 0);
}

#endif
