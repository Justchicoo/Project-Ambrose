/*
 * Project Ambrose by Imjustchico
 * The first call to Update decides which thread the world runs on and every later call is expected on it, so a test and a running server agree on what "the world thread" means; sessions are drained under a lock held only long enough to take a copy of the list, because a handler may add or remove a session while it runs, and a session that has closed is dropped after its last queued work has run.
 */

#include "World.h"
#include "GameSession.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <utility>

World& World::Instance()
{
    static World instance;
    return instance;
}

void World::AddSession(std::shared_ptr<GameSession> session)
{
    if (!session)
        return;
    std::lock_guard const lock(_mutex);
    _sessions.push_back(std::move(session));
}

void World::RemoveSession(GameSession const* session)
{
    std::lock_guard const lock(_mutex);
    std::erase_if(_sessions, [session](std::shared_ptr<GameSession> const& held) { return held.get() == session; });
}

std::size_t World::GetSessionCount() const
{
    std::lock_guard const lock(_mutex);
    return _sessions.size();
}

void World::Clear()
{
    std::lock_guard const lock(_mutex);
    _sessions.clear();
}

std::thread::id World::GetWorldThreadId() const
{
    std::lock_guard const lock(_threadMutex);
    return _worldThread;
}

bool World::IsWorldThread() const
{
    std::lock_guard const lock(_threadMutex);
    return _worldThreadKnown && _worldThread == std::this_thread::get_id();
}

uint64 World::GetTickCount() const
{
    return _ticks.load(std::memory_order_relaxed);
}

void World::Update(std::chrono::milliseconds diff)
{
    {
        std::lock_guard const lock(_threadMutex);
        _worldThread = std::this_thread::get_id();
        _worldThreadKnown = true;
    }
    _ticks.fetch_add(1, std::memory_order_relaxed);

    std::vector<std::shared_ptr<GameSession>> sessions;
    {
        std::lock_guard const lock(_mutex);
        sessions = _sessions;
    }
    for (std::shared_ptr<GameSession> const& session : sessions)
        session->DrainQueue();

    sScriptMgr.OnWorldUpdate(diff);
}
