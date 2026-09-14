/*
 * Project Ambrose by Imjustchico
 * One TCP connection: an async read loop into the frame reassembler, a coalescing write queue, and immediate or delayed close, all on its network thread.
 */

#ifndef AMBROSE_SOCKET_H
#define AMBROSE_SOCKET_H

#include "ByteBuffer.h"
#include "FrameReassembler.h"

#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <system_error>
#include <vector>

class Socket : public std::enable_shared_from_this<Socket>
{
public:
    static constexpr std::size_t ReadBufferSize = 4096;
    static constexpr std::chrono::seconds LingerTimeout{ 5 };
    static constexpr std::chrono::seconds DelayedCloseTimeout{ 30 };

    Socket(asio::ip::tcp::socket&& socket, FrameLimits limits);
    virtual ~Socket();
    Socket(Socket const&) = delete;
    Socket& operator=(Socket const&) = delete;

    void Start();
    void QueueFrame(std::vector<uint8> bytes);
    void QueueFrame(ByteBuffer const& buffer);
    void CloseSocket();
    void DelayedCloseSocket();
    void SetFrameLimits(FrameLimits limits);

    bool IsOpen() const noexcept { return !_closed.load(std::memory_order_relaxed); }
    asio::ip::address const& GetRemoteAddress() const noexcept { return _remoteAddress; }
    uint16 GetRemotePort() const noexcept { return _remotePort; }
    std::size_t GetQueuedBytes() const noexcept { return _queuedBytes.load(std::memory_order_relaxed); }

protected:
    virtual void OnStart();
    virtual void OnFrame(Frame& frame) = 0;
    virtual void OnProtocolError(FrameError error);
    virtual void OnClose();

private:
    void ReadNext();
    void HandleRead(std::error_code const& error, std::size_t bytes);
    void WriteNext();
    void HandleWrite(std::error_code const& error);
    void CloseNow();

    asio::ip::tcp::socket _socket;
    asio::ip::address _remoteAddress;
    uint16 _remotePort = 0;
    FrameReassembler _reassembler;
    asio::steady_timer _lingerTimer;
    std::array<uint8, ReadBufferSize> _readBuffer{};
    std::deque<std::vector<uint8>> _queue;
    std::vector<std::vector<uint8>> _writing;
    bool _writeInProgress = false;
    bool _delayedClose = false;
    bool _shutdownSent = false;
    std::atomic<bool> _closed{ false };
    std::atomic<std::size_t> _queuedBytes{ 0 };
};

#endif
