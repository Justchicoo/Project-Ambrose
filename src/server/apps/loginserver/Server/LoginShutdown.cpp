/*
 * Project Ambrose by Imjustchico
 * Stops accepting clients, sends MSG_LOGINSERVERSHUTDOWN to every accepted login session on its own network thread, closing each once the notice is flushed, and waits until every notice is written or every connection is gone, or the grace period ends.
 */

#include "LoginShutdown.h"
#include "Log.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

std::size_t LoginShutdown::NotifyAndDrain(SocketMgr<LoginSession>& sockets, std::chrono::milliseconds grace, uint32 message)
{
    sockets.StopAccepting();
    if (grace.count() <= 0 || sockets.GetConnectionCount() == 0)
        return 0;

    struct Progress
    {
        std::mutex Mutex;
        std::vector<std::weak_ptr<LoginSession>> Notified;
        std::size_t ThreadsDone = 0;
    };
    auto const progress = std::make_shared<Progress>();
    std::size_t const threads = sockets.ForEachSocket([progress, message](std::shared_ptr<LoginSession> const& session)
    {
        if (session->GetState() != SessionState::Accepted || !session->SendShutdownNotice(message))
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
        for (std::weak_ptr<LoginSession> const& weak : progress->Notified)
            if (std::shared_ptr<LoginSession> const session = weak.lock(); session && session->IsOpen() && session->GetQueuedBytes() != 0)
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
    LOG_INFO("server.loginserver", "Sent MSG_LOGINSERVERSHUTDOWN to {} session(s) after closing the listener; {} notice(s) were still unwritten after {} ms of Login.ShutdownGrace", notified, std::min(waiting, notified), waited.count());
    return notified;
}
