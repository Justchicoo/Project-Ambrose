/*
 * Project Ambrose by Imjustchico
 * What code above the network layer may watch without the network layer knowing it: an observer is told when the network starts, when a session's socket opens and closes, and is asked before each DML message a session receives is handed on and before each one it sends is queued, and any observer refusing keeps the message from going further; observers are added and taken away rarely and read on every network thread, so the list is swapped whole under a lock and each call takes a copy of it and runs the observers with no lock held.
 */

#ifndef AMBROSE_NETWORKHOOKS_H
#define AMBROSE_NETWORKHOOKS_H

#include "Types.h"

#include <cstddef>
#include <string_view>

class NetworkObserver
{
public:
    virtual ~NetworkObserver() = default;

    virtual void OnNetworkStart(std::string_view app) { (void)app; }
    virtual void OnSocketOpen(uint16 sessionId, std::string_view address) { (void)sessionId; (void)address; }
    virtual void OnSocketClose(uint16 sessionId) { (void)sessionId; }
    virtual bool CanPacketReceive(uint16 sessionId, uint8 serviceId, uint8 order) { (void)sessionId; (void)serviceId; (void)order; return true; }
    virtual bool CanPacketSend(uint16 sessionId, uint8 serviceId, uint8 order) { (void)sessionId; (void)serviceId; (void)order; return true; }
};

namespace NetworkHooks
{
    void Add(NetworkObserver* observer);
    void Remove(NetworkObserver* observer);
    std::size_t Count();

    void NetworkStarted(std::string_view app);
    void SocketOpened(uint16 sessionId, std::string_view address);
    void SocketClosed(uint16 sessionId);
    bool CanReceive(uint16 sessionId, uint8 serviceId, uint8 order);
    bool CanSend(uint16 sessionId, uint8 serviceId, uint8 order);

    std::size_t OpenSessions() noexcept;
}

#endif
