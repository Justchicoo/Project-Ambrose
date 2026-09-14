/*
 * Project Ambrose by Imjustchico
 * One network thread running its own io_context: it owns the sockets placed on it, counts them and pending accepts, and sweeps closed sockets nobody else holds.
 */

#ifndef AMBROSE_NETWORKTHREAD_H
#define AMBROSE_NETWORKTHREAD_H

#include "ThreadName.h"

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

template<typename SocketType>
class NetworkThread
{
public:
    static constexpr std::chrono::milliseconds SweepInterval{ 50 };

    NetworkThread() : _work(std::in_place, _context.get_executor()), _sweepTimer(_context)
    {
    }

    ~NetworkThread()
    {
        Stop();
        Wait();
    }

    NetworkThread(NetworkThread const&) = delete;
    NetworkThread& operator=(NetworkThread const&) = delete;

    void Start()
    {
        ScheduleSweep();
        _thread = std::thread([this]
        {
            Ambrose::Threading::SetCurrentThreadName("Network");
            _context.run();
        });
    }

    void Stop()
    {
        if (_stopping.exchange(true))
            return;
        asio::post(_context, [this]
        {
            for (std::shared_ptr<SocketType> const& socket : _sockets)
                socket->CloseSocket();
            _sockets.clear();
            _connections.store(0);
            _sweepTimer.cancel();
            _work.reset();
        });
    }

    void Wait()
    {
        if (_thread.joinable())
            _thread.join();
    }

    void AddSocket(std::shared_ptr<SocketType> socket)
    {
        _connections.fetch_add(1);
        asio::post(_context, [this, socket = std::move(socket)]
        {
            if (_stopping.load())
            {
                socket->CloseSocket();
                return;
            }
            _sockets.push_back(socket);
            socket->Start();
        });
    }

    asio::io_context& GetIoContext() noexcept { return _context; }
    std::size_t GetConnectionCount() const noexcept { return _connections.load(); }
    void ReserveAccept() noexcept { _pendingAccepts.fetch_add(1); }
    void ReleaseAccept() noexcept { _pendingAccepts.fetch_sub(1); }
    bool IsIdle() const noexcept { return _connections.load() == 0 && _pendingAccepts.load() == 0; }

private:
    void ScheduleSweep()
    {
        _sweepTimer.expires_after(SweepInterval);
        _sweepTimer.async_wait([this](std::error_code const& error)
        {
            if (error || _stopping.load())
                return;
            std::erase_if(_sockets, [this](std::shared_ptr<SocketType> const& socket)
            {
                if (socket->IsOpen() || socket.use_count() != 1)
                    return false;
                _connections.fetch_sub(1);
                return true;
            });
            ScheduleSweep();
        });
    }

    asio::io_context _context;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> _work;
    asio::steady_timer _sweepTimer;
    std::thread _thread;
    std::vector<std::shared_ptr<SocketType>> _sockets;
    std::atomic<std::size_t> _connections{ 0 };
    std::atomic<std::size_t> _pendingAccepts{ 0 };
    std::atomic<bool> _stopping{ false };
};

#endif
