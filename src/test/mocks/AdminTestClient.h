/*
 * Project Ambrose by Imjustchico
 * A bare HTTP and WebSocket client for admin API tests: sends one request on a fresh connection and reads the reply, or upgrades to a WebSocket with the bearer token and carries masked text and close frames both ways, each read bounded by a ten second timer.
 */

#ifndef AMBROSE_ADMINTESTCLIENT_H
#define AMBROSE_ADMINTESTCLIENT_H

#include "Types.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>

#include <array>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace AdminTest
{
    struct HttpReply
    {
        int Status = 0;
        std::string Head;
        std::string Body;
    };

    inline std::optional<std::size_t> ContentLength(std::string const& head)
    {
        std::string lowered;
        lowered.reserve(head.size());
        for (char const character : head)
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
        std::size_t const position = lowered.find("content-length:");
        if (position == std::string::npos)
            return std::nullopt;
        std::size_t const end = lowered.find("\r\n", position);
        std::string const value = head.substr(position + 15, end - position - 15);
        try
        {
            return static_cast<std::size_t>(std::stoul(value));
        }
        catch (std::exception const&)
        {
            return std::nullopt;
        }
    }

    inline HttpReply Send(uint16 port, std::string const& request, std::string const& address = "127.0.0.1")
    {
        HttpReply reply;
        asio::io_context context;
        asio::ip::tcp::socket socket(context);
        std::error_code code;
        socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address(address), port), code);
        if (code)
            return reply;
        asio::write(socket, asio::buffer(request), code);
        if (code)
            return reply;

        std::string data;
        std::array<char, 2048> chunk{};
        std::size_t headEnd = std::string::npos;
        std::size_t expected = std::string::npos;
        for (;;)
        {
            if (headEnd == std::string::npos)
                headEnd = data.find("\r\n\r\n");
            if (headEnd != std::string::npos)
            {
                if (expected == std::string::npos)
                    expected = ContentLength(data.substr(0, headEnd + 4)).value_or(0);
                if (data.size() >= headEnd + 4 + expected)
                    break;
            }
            std::size_t const read = socket.read_some(asio::buffer(chunk), code);
            data.append(chunk.data(), read);
            if (code || read == 0)
                break;
        }
        socket.close(code);

        if (headEnd == std::string::npos)
            headEnd = data.find("\r\n\r\n");
        reply.Head = headEnd == std::string::npos ? data : data.substr(0, headEnd);
        reply.Body = headEnd == std::string::npos ? std::string() : data.substr(headEnd + 4);
        std::size_t const space = reply.Head.find(' ');
        if (space != std::string::npos)
            reply.Status = std::atoi(reply.Head.c_str() + space + 1);
        return reply;
    }

    inline HttpReply Ask(uint16 port, std::string const& method, std::string const& path, std::string const& token, std::string const& body = std::string())
    {
        std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
        if (!token.empty())
            request += "Authorization: Bearer " + token + "\r\n";
        if (!body.empty())
            request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        request += "\r\n" + body;
        return Send(port, request);
    }

    inline HttpReply Get(uint16 port, std::string const& path, std::string const& token)
    {
        return Ask(port, "GET", path, token);
    }

    inline std::string UpgradeRequest(std::string const& path, std::string const& token)
    {
        std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n";
        if (!token.empty())
            request += "Authorization: Bearer " + token + "\r\n";
        request += "\r\n";
        return request;
    }

    class SocketClient
    {
    public:
        ~SocketClient()
        {
            std::error_code code;
            _socket.close(code);
        }

        bool Open(uint16 port, std::string const& path, std::string const& token)
        {
            std::error_code code;
            _socket.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port), code);
            if (code)
                return false;
            asio::write(_socket, asio::buffer(UpgradeRequest(path, token)), code);
            if (code)
                return false;
            std::size_t head = _buffer.find("\r\n\r\n");
            while (head == std::string::npos)
            {
                if (!Fill())
                    return false;
                head = _buffer.find("\r\n\r\n");
            }
            std::string const reply = _buffer.substr(0, head);
            _buffer.erase(0, head + 4);
            return reply.find(" 101 ") != std::string::npos;
        }

        bool SendText(std::string const& text) { return SendFrame(0x81, text); }

        bool SendClose(uint16 status)
        {
            std::string payload;
            payload.push_back(static_cast<char>(status >> 8));
            payload.push_back(static_cast<char>(status & 0xFF));
            return SendFrame(0x88, payload);
        }

        std::optional<std::string> ReadText()
        {
            for (;;)
            {
                std::optional<std::pair<uint8, std::string>> const frame = ReadFrame();
                if (!frame)
                    return std::nullopt;
                if (frame->first == 0x1)
                    return frame->second;
                if (frame->first == 0x8)
                    return std::nullopt;
            }
        }

        std::optional<std::pair<uint8, std::string>> ReadFrame()
        {
            while (_buffer.size() < 2)
                if (!Fill())
                    return std::nullopt;
            uint8 const opcode = static_cast<uint8>(_buffer[0]) & 0x0F;
            std::size_t length = static_cast<uint8>(_buffer[1]) & 0x7F;
            std::size_t header = 2;
            if (length == 126)
            {
                while (_buffer.size() < 4)
                    if (!Fill())
                        return std::nullopt;
                length = (static_cast<std::size_t>(static_cast<uint8>(_buffer[2])) << 8) | static_cast<uint8>(_buffer[3]);
                header = 4;
            }
            else if (length == 127)
            {
                while (_buffer.size() < 10)
                    if (!Fill())
                        return std::nullopt;
                length = 0;
                for (std::size_t index = 2; index < 10; ++index)
                    length = (length << 8) | static_cast<uint8>(_buffer[index]);
                header = 10;
            }
            while (_buffer.size() < header + length)
                if (!Fill())
                    return std::nullopt;
            std::string const payload = _buffer.substr(header, length);
            _buffer.erase(0, header + length);
            return std::make_pair(opcode, payload);
        }

        void SetReadTimeout(std::chrono::milliseconds timeout) { _timeout = timeout; }

    private:
        bool SendFrame(uint8 header, std::string const& payload)
        {
            std::array<uint8, 4> const mask{ 0x12, 0x34, 0x56, 0x78 };
            std::string frame;
            frame.push_back(static_cast<char>(header));
            if (payload.size() < 126)
            {
                frame.push_back(static_cast<char>(0x80 | static_cast<uint8>(payload.size())));
            }
            else
            {
                frame.push_back(static_cast<char>(0x80 | 126));
                frame.push_back(static_cast<char>(payload.size() >> 8));
                frame.push_back(static_cast<char>(payload.size() & 0xFF));
            }
            for (uint8 const byte : mask)
                frame.push_back(static_cast<char>(byte));
            for (std::size_t index = 0; index < payload.size(); ++index)
                frame.push_back(static_cast<char>(static_cast<uint8>(payload[index]) ^ mask[index % mask.size()]));
            std::error_code code;
            asio::write(_socket, asio::buffer(frame), code);
            return !code;
        }

        bool Fill()
        {
            std::array<char, 4096> chunk{};
            std::optional<std::error_code> result;
            std::size_t read = 0;
            asio::steady_timer timer(_context);
            _socket.async_read_some(asio::buffer(chunk), [&](std::error_code code, std::size_t size)
            {
                result = code;
                read = size;
                timer.cancel();
            });
            timer.expires_after(_timeout);
            timer.async_wait([&](std::error_code code)
            {
                if (code == asio::error::operation_aborted)
                    return;
                std::error_code ignored;
                _socket.cancel(ignored);
            });
            _context.restart();
            _context.run();
            if (!result || *result)
                return false;
            _buffer.append(chunk.data(), read);
            return read != 0;
        }

        asio::io_context _context;
        asio::ip::tcp::socket _socket{ _context };
        std::string _buffer;
        std::chrono::milliseconds _timeout{ 10000 };
    };
}

#endif
