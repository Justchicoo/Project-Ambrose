/*
 * Project Ambrose by Imjustchico
 * A blocking loopback client for session tests, optionally with a small receive buffer, that reads frames with timeouts, answers the SessionOffer and keeps it for login hashes, sends raw bytes and waits for the server to close, plus a polling wait helper.
 */

#ifndef AMBROSE_FAKESESSIONCLIENT_H
#define AMBROSE_FAKESESSIONCLIENT_H

#include "ControlMessages.h"
#include "Frame.h"
#include "FrameReassembler.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>

#include <array>
#include <chrono>
#include <functional>
#include <optional>

bool WaitForCondition(std::function<bool()> const& condition, std::chrono::milliseconds timeout = std::chrono::seconds(30));

class FakeSessionClient
{
public:
    explicit FakeSessionClient(uint16 port, int32 receiveBufferBytes = -1) : _socket(_context)
    {
        asio::ip::tcp::endpoint const endpoint(asio::ip::make_address("127.0.0.1"), port);
        _socket.open(endpoint.protocol());
        if (receiveBufferBytes >= 0)
            _socket.set_option(asio::socket_base::receive_buffer_size(receiveBufferBytes));
        _socket.connect(endpoint);
    }

    std::optional<Frame> ReadFrame(std::chrono::milliseconds timeout = std::chrono::seconds(30))
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (true)
        {
            if (std::optional<Frame> frame = _reassembler.Next())
                return frame;
            if (_closed || _reassembler.HasError())
                return std::nullopt;
            auto const now = std::chrono::steady_clock::now();
            if (now >= deadline)
                return std::nullopt;
            bool done = false;
            std::error_code readError;
            std::size_t readBytes = 0;
            _socket.async_read_some(asio::buffer(_buffer), [&](std::error_code const& error, std::size_t bytes)
            {
                done = true;
                readError = error;
                readBytes = bytes;
            });
            _context.restart();
            _context.run_for(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
            if (!done)
            {
                _socket.cancel();
                _context.restart();
                _context.run();
                if (!done || readError == asio::error::operation_aborted)
                    return std::nullopt;
            }
            if (readError)
            {
                _closed = true;
                return std::nullopt;
            }
            _received += readBytes;
            _reassembler.Feed(std::span<uint8 const>(_buffer.data(), readBytes));
        }
    }

    std::optional<Frame> ReadControl(ControlOpcode opcode, std::chrono::milliseconds timeout = std::chrono::seconds(30))
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::optional<Frame> frame = ReadFrame(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()));
            if (!frame)
                return std::nullopt;
            if (ControlMessages::GetOpcode(*frame) == opcode)
                return frame;
        }
        return std::nullopt;
    }

    bool WaitForClose(std::chrono::milliseconds timeout = std::chrono::seconds(30))
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (!_closed && std::chrono::steady_clock::now() < deadline)
            ReadFrame(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()));
        return _closed;
    }

    void Send(ByteBuffer const& bytes)
    {
        asio::write(_socket, asio::buffer(bytes.GetData().data(), bytes.GetData().size()));
    }

    uint16 Handshake()
    {
        std::optional<Frame> const offerFrame = ReadControl(ControlOpcode::SessionOffer);
        if (!offerFrame)
            return 0;
        std::optional<SessionOffer> const offer = ControlMessages::DecodeSessionOffer(offerFrame->Payload);
        if (!offer)
            return 0;
        _offer = offer;
        SendAccept(offer->SessionId, offer->Time);
        return offer->SessionId;
    }

    void SendAccept(uint16 sessionId, SessionTimestamp time = {})
    {
        SessionAccept accept;
        accept.SessionId = sessionId;
        accept.Time = time;
        ByteBuffer out;
        ControlMessages::WriteFrame(out, accept);
        Send(out);
    }

    bool IsClosed() const noexcept { return _closed; }
    std::optional<SessionOffer> const& GetOffer() const noexcept { return _offer; }
    std::size_t GetReceivedBytes() const noexcept { return _received; }

private:
    asio::io_context _context;
    asio::ip::tcp::socket _socket;
    FrameReassembler _reassembler;
    std::array<uint8, 4096> _buffer{};
    std::size_t _received = 0;
    std::optional<SessionOffer> _offer;
    bool _closed = false;
};

#endif
