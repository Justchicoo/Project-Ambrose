/*
 * Project Ambrose by Imjustchico
 * The game's update loop and the sessions it owns: one thread calls Update, which is the world thread from then on, and everything the world touches happens there, so a session's queued work is drained on it rather than on the network thread that read the message; the tick carries every script's OnUpdate after the sessions have been drained, so a script sees the state the messages of that tick left behind.
 */

#ifndef AMBROSE_WORLD_H
#define AMBROSE_WORLD_H

#include "Types.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class GameSession;

class World
{
public:
    static World& Instance();

    World(World const&) = delete;
    World& operator=(World const&) = delete;

    void AddSession(std::shared_ptr<GameSession> session);
    void RemoveSession(GameSession const* session);
    std::size_t GetSessionCount() const;
    void Clear();

    void Update(std::chrono::milliseconds diff);

    std::thread::id GetWorldThreadId() const;
    bool IsWorldThread() const;
    uint64 GetTickCount() const;

private:
    World() = default;

    mutable std::mutex _mutex;
    std::vector<std::shared_ptr<GameSession>> _sessions;
    std::atomic<uint64> _ticks{ 0 };
    mutable std::mutex _threadMutex;
    std::thread::id _worldThread;
    bool _worldThreadKnown = false;
};

#define sWorld World::Instance()

#endif
