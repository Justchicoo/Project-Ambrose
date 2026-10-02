/*
 * Project Ambrose by Imjustchico
 * Keeps the observers as one shared list replaced on each change, so a network thread takes the list under a shared lock held only for the copy and calls each observer in the order it was added; a socket's opening and closing also keep the count of sessions open across every listener of the process. The list is never destroyed, so an observer that is itself a static, such as the script manager, can take itself off it as the process exits, whichever static was built first.
 */

#include "NetworkHooks.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace
{
    using ObserverList = std::vector<NetworkObserver*>;

    struct Registry
    {
        std::shared_mutex Mutex;
        std::shared_ptr<ObserverList const> Observers = std::make_shared<ObserverList const>();
    };

    Registry& Hooks()
    {
        static Registry& registry = *new Registry;
        return registry;
    }

    std::atomic<std::size_t>& OpenCount()
    {
        static std::atomic<std::size_t> count{ 0 };
        return count;
    }

    std::shared_ptr<ObserverList const> Current()
    {
        std::shared_lock const lock(Hooks().Mutex);
        return Hooks().Observers;
    }
}

void NetworkHooks::Add(NetworkObserver* observer)
{
    if (!observer)
        return;
    std::unique_lock const lock(Hooks().Mutex);
    ObserverList const& current = *Hooks().Observers;
    if (std::find(current.begin(), current.end(), observer) != current.end())
        return;
    auto next = std::make_shared<ObserverList>(current);
    next->push_back(observer);
    Hooks().Observers = std::move(next);
}

void NetworkHooks::Remove(NetworkObserver* observer)
{
    std::unique_lock const lock(Hooks().Mutex);
    auto next = std::make_shared<ObserverList>(*Hooks().Observers);
    next->erase(std::remove(next->begin(), next->end(), observer), next->end());
    Hooks().Observers = std::move(next);
}

std::size_t NetworkHooks::Count()
{
    return Current()->size();
}

void NetworkHooks::NetworkStarted(std::string_view app)
{
    for (NetworkObserver* observer : *Current())
        observer->OnNetworkStart(app);
}

void NetworkHooks::SocketOpened(uint16 sessionId, std::string_view address)
{
    OpenCount().fetch_add(1, std::memory_order_relaxed);
    for (NetworkObserver* observer : *Current())
        observer->OnSocketOpen(sessionId, address);
}

void NetworkHooks::SocketClosed(uint16 sessionId)
{
    std::size_t open = OpenCount().load(std::memory_order_relaxed);
    while (open > 0 && !OpenCount().compare_exchange_weak(open, open - 1, std::memory_order_relaxed))
    {
    }
    for (NetworkObserver* observer : *Current())
        observer->OnSocketClose(sessionId);
}

bool NetworkHooks::CanReceive(uint16 sessionId, uint8 serviceId, uint8 order)
{
    for (NetworkObserver* observer : *Current())
        if (!observer->CanPacketReceive(sessionId, serviceId, order))
            return false;
    return true;
}

bool NetworkHooks::CanSend(uint16 sessionId, uint8 serviceId, uint8 order)
{
    for (NetworkObserver* observer : *Current())
        if (!observer->CanPacketSend(sessionId, serviceId, order))
            return false;
    return true;
}

std::size_t NetworkHooks::OpenSessions() noexcept
{
    return OpenCount().load(std::memory_order_relaxed);
}
