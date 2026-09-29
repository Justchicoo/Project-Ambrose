/*
 * Project Ambrose by Imjustchico
 * Stops accepting clients, sends MSG_SERVERSHUTDOWN to accepted game sessions, and waits for queued notices to flush or for the grace period to end.
 */

#include "GameShutdown.h"
#include "Log.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

std::size_t GameShutdown::NotifyAndDrain(SocketMgr<GameSession>& sockets, std::chrono::milliseconds grace, uint32 message)
{
    sockets.StopAccepting();
    if (grace.count() <= 0 || sockets.GetConnectionCount() == 0)
        return 0;

    struct Progress
    {
        std::mutex Mutex;
        std::vector<std::weak_ptr<GameSession>> Notified;
        std::size_t ThreadsDone = 0;
    };
    auto const progress = std::make_shared<Progress>();
    std::size_t const threads = sockets.ForEachSocket([progress, message](std::shared_ptr<GameSession> const& session)
    {
        if (session->GetState() != SessionState::Accepted)
            return;
        session->SetIntentionalDisconnect();
        GameMessages::ServerShutdown notice;
        notice.Message = message;
        if (!session->SendDmlMessageDelayedClose(notice))
            return;
        std::lock_guard const lock(progress->Mutex);
        progress->Notified.push_back(session);
    }, [progress]
    {
        std::lock_guard const lock(progress->Mutex);
        ++progress->ThreadsDone;
    });

    auto const unflushed = [&progress, threads]
    {
        std::lock_guard const lock(progress->Mutex);
        if (progress->ThreadsDone < threads)
            return progress->Notified.size() + 1;
        std::size_t waiting = 0;
        for (std::weak_ptr<GameSession> const& weak : progress->Notified)
            if (std::shared_ptr<GameSession> const session = weak.lock(); session && session->IsOpen() && session->GetQueuedBytes() != 0)
                ++waiting;
        return waiting;
    };
    auto const start = std::chrono::steady_clock::now();
    auto const deadline = start + grace;
    while (unflushed() != 0 && sockets.GetConnectionCount() != 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(DrainPollInterval);

    std::size_t const waiting = unflushed();
    std::size_t notified = 0;
    {
        std::lock_guard const lock(progress->Mutex);
        notified = progress->Notified.size();
    }
    auto const waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    LOG_INFO("server.gameserver", "Sent MSG_SERVERSHUTDOWN with Message={} to {} session(s); {} notice(s) were still unwritten after {} ms",
        message, notified, std::min(waiting, notified), waited.count());
    return notified;
}
