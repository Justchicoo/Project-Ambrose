/*
 * Project Ambrose by Imjustchico
 * Stops new game connections, sends MSG_SERVERSHUTDOWN on each session's network thread, and waits a bounded time for notices to flush.
 */

#ifndef AMBROSE_GAMESHUTDOWN_H
#define AMBROSE_GAMESHUTDOWN_H

#include "GameSession.h"
#include "SocketMgr.h"

#include <chrono>
#include <cstddef>

namespace GameShutdown
{
    inline constexpr std::chrono::milliseconds DrainPollInterval{ 20 };

    std::size_t NotifyAndDrain(SocketMgr<GameSession>& sockets, std::chrono::milliseconds grace, uint32 message = 0);
}

#endif
