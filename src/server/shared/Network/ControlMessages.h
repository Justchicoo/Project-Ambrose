/*
 * Project Ambrose by Imjustchico
 * The four KI control messages (SessionOffer, KeepAlive both directions, KeepAliveRsp, SessionAccept) with byte-exact bodies, timestamps, and frame helpers.
 */

#ifndef AMBROSE_CONTROLMESSAGES_H
#define AMBROSE_CONTROLMESSAGES_H

#include "ByteBuffer.h"
#include "Frame.h"

#include <chrono>
#include <optional>
#include <span>
#include <vector>

enum class ControlOpcode : uint8
{
    SessionOffer = 0,
    KeepAlive = 3,
    KeepAliveRsp = 4,
    SessionAccept = 5
};

struct SessionTimestamp
{
    int32 TimeHigh = 0;
    int32 TimeLow = 0;
    uint32 Milliseconds = 0;

    static SessionTimestamp FromTimePoint(std::chrono::system_clock::time_point time) noexcept;
    uint64 GetSeconds() const noexcept;

    bool operator==(SessionTimestamp const&) const = default;
};

struct SessionOffer
{
    static constexpr std::size_t BodySize = 14;

    uint16 SessionId = 0;
    SessionTimestamp Time;
    std::vector<uint8> Trailing;

    bool operator==(SessionOffer const&) const = default;
};

struct SessionAccept
{
    static constexpr std::size_t BodySize = 16;

    uint16 Reserved = 0;
    SessionTimestamp Time;
    uint16 SessionId = 0;
    std::vector<uint8> Trailing;

    bool operator==(SessionAccept const&) const = default;
};

struct ClientKeepAlive
{
    static constexpr std::size_t BodySize = 6;

    uint16 SessionId = 0;
    uint16 Milliseconds = 0;
    uint16 ElapsedMinutes = 0;

    bool operator==(ClientKeepAlive const&) const = default;
};

struct ServerKeepAlive
{
    static constexpr std::size_t BodySize = 6;

    uint16 SessionId = 0;
    uint32 Milliseconds = 0;

    bool operator==(ServerKeepAlive const&) const = default;
};

struct KeepAliveResponse
{
    static constexpr std::size_t BodySize = 6;

    uint16 SessionId = 0;
    uint16 Milliseconds = 0;
    uint16 ElapsedMinutes = 0;

    bool operator==(KeepAliveResponse const&) const = default;
};

namespace ControlMessages
{
    std::vector<uint8> Encode(SessionOffer const& message);
    std::vector<uint8> Encode(SessionAccept const& message);
    std::vector<uint8> Encode(ClientKeepAlive const& message);
    std::vector<uint8> Encode(ServerKeepAlive const& message);
    std::vector<uint8> Encode(KeepAliveResponse const& message);

    std::optional<SessionOffer> DecodeSessionOffer(std::span<uint8 const> body);
    std::optional<SessionAccept> DecodeSessionAccept(std::span<uint8 const> body);
    std::optional<ClientKeepAlive> DecodeClientKeepAlive(std::span<uint8 const> body);
    std::optional<ServerKeepAlive> DecodeServerKeepAlive(std::span<uint8 const> body);
    std::optional<KeepAliveResponse> DecodeKeepAliveResponse(std::span<uint8 const> body);

    void WriteFrame(ByteBuffer& out, SessionOffer const& message);
    void WriteFrame(ByteBuffer& out, SessionAccept const& message);
    void WriteFrame(ByteBuffer& out, ClientKeepAlive const& message);
    void WriteFrame(ByteBuffer& out, ServerKeepAlive const& message);
    void WriteFrame(ByteBuffer& out, KeepAliveResponse const& message);

    std::optional<ControlOpcode> GetOpcode(Frame const& frame) noexcept;
}

#endif
