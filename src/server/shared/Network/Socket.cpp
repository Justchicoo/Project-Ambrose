/*
 * Project Ambrose by Imjustchico
 * Reads into the reassembler until close, writes queued frames as one gathered write, and shuts down gracefully after a delayed close drains the queue.
 */

#include "Socket.h"
#include "IpAddress.h"
#include "Log.h"

#include <asio/dispatch.hpp>
#include <asio/post.hpp>
#include <asio/write.hpp>

Socket::Socket(asio::ip::tcp::socket&& socket, FrameLimits limits) : _socket(std::move(socket)), _reassembler(limits), _lingerTimer(_socket.get_executor())
{
    std::error_code error;
    asio::ip::tcp::endpoint const remote = _socket.remote_endpoint(error);
    if (!error)
    {
        _remoteAddress = Ambrose::Asio::Unmap(remote.address());
        _remotePort = remote.port();
    }
}

Socket::~Socket()
{
    std::error_code ignored;
    _socket.close(ignored);
}

void Socket::Start()
{
    asio::dispatch(_socket.get_executor(), [self = shared_from_this()]
    {
        if (!self->IsOpen())
            return;
        self->OnStart();
        if (self->IsOpen())
            self->ReadNext();
    });
}

void Socket::QueueFrame(std::vector<uint8> bytes)
{
    if (bytes.empty() || !IsOpen())
        return;
    _queuedBytes.fetch_add(bytes.size(), std::memory_order_relaxed);
    asio::post(_socket.get_executor(), [self = shared_from_this(), bytes = std::move(bytes)]() mutable
    {
        if (!self->IsOpen() || self->_delayedClose)
        {
            self->_queuedBytes.fetch_sub(bytes.size(), std::memory_order_relaxed);
            return;
        }
        self->_queue.push_back(std::move(bytes));
        self->WriteNext();
    });
}

void Socket::QueueFrame(ByteBuffer const& buffer)
{
    QueueFrame(std::vector<uint8>(buffer.GetData().begin(), buffer.GetData().end()));
}

void Socket::CloseSocket()
{
    asio::post(_socket.get_executor(), [self = shared_from_this()] { self->CloseNow(); });
}

void Socket::DelayedCloseSocket()
{
    asio::post(_socket.get_executor(), [self = shared_from_this()]
    {
        if (!self->IsOpen() || self->_delayedClose)
            return;
        self->_delayedClose = true;
        self->_lingerTimer.expires_after(DelayedCloseTimeout);
        self->_lingerTimer.async_wait([weak = std::weak_ptr<Socket>(self)](std::error_code const& error)
        {
            if (std::shared_ptr<Socket> const socket = weak.lock(); socket && !error)
                socket->CloseNow();
        });
        self->WriteNext();
    });
}

void Socket::SetFrameLimits(FrameLimits limits)
{
    asio::post(_socket.get_executor(), [self = shared_from_this(), limits] { self->_reassembler.SetLimits(limits); });
}

void Socket::OnStart()
{
}

void Socket::OnProtocolError(FrameError error)
{
    LOG_WARN("network", "Closing {}:{} after a protocol error: {}", _remoteAddress.to_string(), _remotePort, FrameLayout::GetErrorName(error));
}

void Socket::OnClose()
{
}

void Socket::ReadNext()
{
    _socket.async_read_some(asio::buffer(_readBuffer), [self = shared_from_this()](std::error_code const& error, std::size_t bytes) { self->HandleRead(error, bytes); });
}

void Socket::HandleRead(std::error_code const& error, std::size_t bytes)
{
    if (!IsOpen())
        return;
    if (error)
    {
        CloseNow();
        return;
    }
    if (!_shutdownSent)
    {
        _reassembler.Feed(std::span<uint8 const>(_readBuffer.data(), bytes));
        while (std::optional<Frame> frame = _reassembler.Next())
        {
            OnFrame(*frame);
            if (!IsOpen())
                return;
        }
        if (_reassembler.HasError())
        {
            OnProtocolError(_reassembler.GetError());
            CloseNow();
            return;
        }
    }
    ReadNext();
}

void Socket::WriteNext()
{
    if (!IsOpen() || _writeInProgress)
        return;
    if (_queue.empty())
    {
        if (_delayedClose && !_shutdownSent)
        {
            _shutdownSent = true;
            std::error_code ignored;
            _socket.shutdown(asio::ip::tcp::socket::shutdown_send, ignored);
            _lingerTimer.expires_after(LingerTimeout);
            _lingerTimer.async_wait([self = shared_from_this()](std::error_code const& error)
            {
                if (!error)
                    self->CloseNow();
            });
        }
        return;
    }

    _writing.clear();
    std::vector<asio::const_buffer> buffers;
    buffers.reserve(_queue.size());
    while (!_queue.empty())
    {
        _writing.push_back(std::move(_queue.front()));
        _queue.pop_front();
        buffers.push_back(asio::buffer(_writing.back()));
    }
    _writeInProgress = true;
    asio::async_write(_socket, buffers, [self = shared_from_this()](std::error_code const& error, std::size_t) { self->HandleWrite(error); });
}

void Socket::HandleWrite(std::error_code const& error)
{
    _writeInProgress = false;
    std::size_t written = 0;
    for (std::vector<uint8> const& bytes : _writing)
        written += bytes.size();
    _writing.clear();
    _queuedBytes.fetch_sub(written, std::memory_order_relaxed);
    if (error)
    {
        CloseNow();
        return;
    }
    WriteNext();
}

void Socket::CloseNow()
{
    if (_closed.exchange(true, std::memory_order_relaxed))
        return;
    std::error_code ignored;
    _lingerTimer.cancel();
    _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
    _socket.close(ignored);
    std::size_t pending = 0;
    for (std::vector<uint8> const& bytes : _queue)
        pending += bytes.size();
    _queue.clear();
    _queuedBytes.fetch_sub(pending, std::memory_order_relaxed);
    OnClose();
}
