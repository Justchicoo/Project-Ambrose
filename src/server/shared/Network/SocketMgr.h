/*
 * Project Ambrose by Imjustchico
 * Starts an app's listener and network threads, places each accepted socket on the least-loaded thread, and applies setting changes live.
 */

#ifndef AMBROSE_SOCKETMGR_H
#define AMBROSE_SOCKETMGR_H

#include "AsyncAcceptor.h"
#include "Log.h"
#include "NetworkSettings.h"
#include "NetworkThread.h"

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

template<typename SocketType>
class SocketMgr
{
public:
    using Factory = std::function<std::shared_ptr<SocketType>(asio::ip::tcp::socket&&, FrameLimits const&)>;
    using ThreadPtr = std::unique_ptr<NetworkThread<SocketType>>;

    static constexpr std::chrono::milliseconds RetireSweepInterval{ 100 };

    explicit SocketMgr(Factory factory = DefaultFactory()) : _factory(std::move(factory))
    {
    }

    virtual ~SocketMgr()
    {
        StopNetwork();
    }

    SocketMgr(SocketMgr const&) = delete;
    SocketMgr& operator=(SocketMgr const&) = delete;

    bool StartNetwork(NetworkSettings const& settings, std::string& error)
    {
        std::lock_guard<std::mutex> lifecycle(_lifecycleMutex);
        if (IsRunning())
        {
            error = "the network is already running";
            return false;
        }
        _acceptContext.emplace();
        auto acceptor = std::make_shared<AsyncAcceptor>(*_acceptContext);
        if (!acceptor->Bind(settings.BindIp, settings.Port, error))
        {
            LOG_ERROR("network", "Network start failed: {}", error);
            acceptor.reset();
            _acceptContext.reset();
            return false;
        }
        {
            std::lock_guard<std::mutex> state(_stateMutex);
            _settings = settings;
            _settings.Threads = std::clamp<std::size_t>(settings.Threads, 1, NetworkSettings::MaxThreads);
            for (std::size_t i = 0; i < _settings.Threads; ++i)
                StartThread();
            _acceptor = acceptor;
            _running = true;
        }
        _acceptWork.emplace(_acceptContext->get_executor());
        _retireTimer.emplace(*_acceptContext);
        StartAccepting(acceptor);
        ScheduleRetireSweep();
        _acceptThread = std::thread([this]
        {
            Ambrose::Threading::SetCurrentThreadName("NetworkAccept");
            _acceptContext->run();
        });
        LOG_INFO("network", "Listening on {} with {} network thread(s)", acceptor->GetEndpointText(), std::clamp<std::size_t>(settings.Threads, 1, NetworkSettings::MaxThreads));
        return true;
    }

    bool ApplySettings(NetworkSettings const& settings, std::string& error)
    {
        std::lock_guard<std::mutex> lifecycle(_lifecycleMutex);
        std::shared_ptr<AsyncAcceptor> current;
        NetworkSettings previous;
        bool shrunk = false;
        {
            std::lock_guard<std::mutex> state(_stateMutex);
            if (!_running)
            {
                error = "the network is not running";
                return false;
            }
            previous = _settings;
            _settings.Limits = settings.Limits;
            _settings.OutKBuff = settings.OutKBuff;
            _settings.TcpNoDelay = settings.TcpNoDelay;
            std::size_t const threads = std::clamp<std::size_t>(settings.Threads, 1, NetworkSettings::MaxThreads);
            shrunk = threads < _threads.size();
            ResizeThreads(threads);
            _settings.Threads = _threads.size();
            current = _acceptor;
        }
        bool const listening = current && current->IsOpen();
        if (shrunk && listening)
            current->Cancel();

        if (listening && settings.BindIp == previous.BindIp && settings.Port == previous.Port)
            return true;

        auto replacement = std::make_shared<AsyncAcceptor>(*_acceptContext);
        std::string bindError;
        bool bound = replacement->Bind(settings.BindIp, settings.Port, bindError);
        bool closedCurrent = false;
        if (!bound && listening && settings.Port != 0 && settings.Port == current->GetPort())
        {
            current->CloseAndWait();
            closedCurrent = true;
            replacement = std::make_shared<AsyncAcceptor>(*_acceptContext);
            bound = replacement->Bind(settings.BindIp, settings.Port, bindError);
        }
        if (!bound)
        {
            error = bindError;
            if (closedCurrent)
            {
                auto restored = std::make_shared<AsyncAcceptor>(*_acceptContext);
                std::string restoreError;
                if (restored->Bind(previous.BindIp, current->GetPort(), restoreError))
                {
                    SetAcceptor(restored);
                    StartAccepting(restored);
                    LOG_ERROR("network", "Rebinding the listener failed, restored {}: {}", restored->GetEndpointText(), error);
                }
                else
                {
                    SetAcceptor(nullptr);
                    LOG_ERROR("network", "Rebinding the listener failed and restoring {}:{} also failed, so nothing is listening: {}; {}", previous.BindIp, current->GetPort(), error, restoreError);
                }
            }
            else if (listening)
                LOG_ERROR("network", "Rebinding the listener failed, keeping {}: {}", current->GetEndpointText(), error);
            else
                LOG_ERROR("network", "Binding the listener failed, so nothing is listening: {}", error);
            return false;
        }
        if (listening && !closedCurrent)
            current->Close();
        {
            std::lock_guard<std::mutex> state(_stateMutex);
            _settings.BindIp = settings.BindIp;
            _settings.Port = settings.Port;
            _acceptor = replacement;
        }
        StartAccepting(replacement);
        LOG_INFO("network", "Listener moved to {}", replacement->GetEndpointText());
        return true;
    }

    void StopNetwork()
    {
        std::lock_guard<std::mutex> lifecycle(_lifecycleMutex);
        std::shared_ptr<AsyncAcceptor> acceptor;
        std::vector<ThreadPtr> threads;
        std::vector<ThreadPtr> retiring;
        {
            std::lock_guard<std::mutex> state(_stateMutex);
            if (!_running)
                return;
            _running = false;
            acceptor = std::move(_acceptor);
            threads = std::move(_threads);
            retiring = std::move(_retiring);
            _threads.clear();
            _retiring.clear();
        }
        if (acceptor)
            acceptor->Close();
        asio::post(*_acceptContext, [this]
        {
            _retireTimer->cancel();
            _acceptWork.reset();
        });
        if (_acceptThread.joinable())
            _acceptThread.join();
        for (ThreadPtr const& thread : threads)
            thread->Stop();
        for (ThreadPtr const& thread : retiring)
            thread->Stop();
        threads.clear();
        retiring.clear();
        acceptor.reset();
        _retireTimer.reset();
        _acceptContext.reset();
        LOG_INFO("network", "Network stopped");
    }

    bool IsRunning() const
    {
        std::lock_guard<std::mutex> state(_stateMutex);
        return _running;
    }

    bool IsListening() const
    {
        std::lock_guard<std::mutex> state(_stateMutex);
        return _acceptor && _acceptor->IsOpen();
    }

    uint16 GetPort() const
    {
        std::lock_guard<std::mutex> state(_stateMutex);
        return _acceptor ? _acceptor->GetPort() : 0;
    }

    std::size_t GetThreadCount() const
    {
        std::lock_guard<std::mutex> state(_stateMutex);
        return _threads.size();
    }

    std::size_t GetRetiringThreadCount() const
    {
        std::lock_guard<std::mutex> state(_stateMutex);
        return _retiring.size();
    }

    std::size_t GetConnectionCount() const
    {
        std::lock_guard<std::mutex> state(_stateMutex);
        std::size_t count = 0;
        for (ThreadPtr const& thread : _threads)
            count += thread->GetConnectionCount();
        for (ThreadPtr const& thread : _retiring)
            count += thread->GetConnectionCount();
        return count;
    }

    NetworkSettings GetSettings() const
    {
        std::lock_guard<std::mutex> state(_stateMutex);
        return _settings;
    }

private:
    static Factory DefaultFactory()
    {
        return [](asio::ip::tcp::socket&& socket, FrameLimits const& limits) { return std::make_shared<SocketType>(std::move(socket), limits); };
    }

    void StartThread()
    {
        auto thread = std::make_unique<NetworkThread<SocketType>>();
        thread->Start();
        _threads.push_back(std::move(thread));
    }

    void ResizeThreads(std::size_t count)
    {
        while (_threads.size() < count)
            StartThread();
        while (_threads.size() > count)
        {
            _retiring.push_back(std::move(_threads.back()));
            _threads.pop_back();
        }
    }

    void SetAcceptor(std::shared_ptr<AsyncAcceptor> acceptor)
    {
        std::lock_guard<std::mutex> state(_stateMutex);
        _acceptor = std::move(acceptor);
    }

    void StartAccepting(std::shared_ptr<AsyncAcceptor> const& acceptor)
    {
        acceptor->Start(
            [this]() -> AsyncAcceptor::Target
            {
                std::lock_guard<std::mutex> state(_stateMutex);
                if (!_running || _threads.empty())
                    return {};
                auto const least = std::min_element(_threads.begin(), _threads.end(), [](ThreadPtr const& left, ThreadPtr const& right) { return left->GetConnectionCount() < right->GetConnectionCount(); });
                (*least)->ReserveAccept();
                return { &(*least)->GetIoContext(), least->get() };
            },
            [this](std::error_code const& error, asio::ip::tcp::socket&& socket, void* tag) { OnAccept(error, std::move(socket), static_cast<NetworkThread<SocketType>*>(tag)); });
    }

    void OnAccept(std::error_code const& error, asio::ip::tcp::socket&& socket, NetworkThread<SocketType>* thread)
    {
        if (!error)
        {
            FrameLimits limits;
            int32 outKBuff = -1;
            bool noDelay = true;
            {
                std::lock_guard<std::mutex> state(_stateMutex);
                limits = _settings.Limits;
                outKBuff = _settings.OutKBuff;
                noDelay = _settings.TcpNoDelay;
            }
            std::error_code ignored;
            socket.set_option(asio::ip::tcp::no_delay(noDelay), ignored);
            if (outKBuff >= 0)
                socket.set_option(asio::socket_base::send_buffer_size(outKBuff), ignored);
            try
            {
                std::shared_ptr<SocketType> created = _factory(std::move(socket), limits);
                if (created)
                    thread->AddSocket(std::move(created));
            }
            catch (std::exception const& exception)
            {
                LOG_ERROR("network", "Creating a socket failed: {}", exception.what());
            }
        }
        thread->ReleaseAccept();
    }

    void ScheduleRetireSweep()
    {
        _retireTimer->expires_after(RetireSweepInterval);
        _retireTimer->async_wait([this](std::error_code const& error)
        {
            if (error)
                return;
            std::vector<ThreadPtr> finished;
            {
                std::lock_guard<std::mutex> state(_stateMutex);
                if (!_running)
                    return;
                auto const idle = std::stable_partition(_retiring.begin(), _retiring.end(), [](ThreadPtr const& thread) { return !thread->IsIdle(); });
                for (auto it = idle; it != _retiring.end(); ++it)
                    finished.push_back(std::move(*it));
                _retiring.erase(idle, _retiring.end());
            }
            finished.clear();
            ScheduleRetireSweep();
        });
    }

    Factory _factory;
    std::mutex _lifecycleMutex;
    mutable std::mutex _stateMutex;
    NetworkSettings _settings;
    bool _running = false;
    std::optional<asio::io_context> _acceptContext;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> _acceptWork;
    std::optional<asio::steady_timer> _retireTimer;
    std::thread _acceptThread;
    std::shared_ptr<AsyncAcceptor> _acceptor;
    std::vector<ThreadPtr> _threads;
    std::vector<ThreadPtr> _retiring;
};

#endif
