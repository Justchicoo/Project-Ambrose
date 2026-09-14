/*
 * Project Ambrose by Imjustchico
 * Encodes and decodes control message bodies with ByteBuffer, keeps unknown trailing offer and accept bytes, and wraps bodies in control frames.
 */

#include "ControlMessages.h"
#include "FrameWriter.h"

namespace
{
    std::vector<uint8> Bytes(ByteBuffer const& buffer)
    {
        return std::vector<uint8>(buffer.GetData().begin(), buffer.GetData().end());
    }

    void WriteTimestamp(ByteBuffer& buffer, SessionTimestamp const& time)
    {
        buffer.Write(time.TimeHigh);
        buffer.Write(time.TimeLow);
        buffer.Write(time.Milliseconds);
    }

    SessionTimestamp ReadTimestamp(ByteBuffer& buffer)
    {
        SessionTimestamp time;
        time.TimeHigh = buffer.Read<int32>();
        time.TimeLow = buffer.Read<int32>();
        time.Milliseconds = buffer.Read<uint32>();
        return time;
    }

    std::vector<uint8> Remaining(ByteBuffer& buffer)
    {
        std::span<uint8 const> const rest = buffer.ReadBytes(buffer.GetRemaining());
        return std::vector<uint8>(rest.begin(), rest.end());
    }
}

SessionTimestamp SessionTimestamp::FromTimePoint(std::chrono::system_clock::time_point time) noexcept
{
    auto const sinceEpoch = time.time_since_epoch();
    int64 const milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(sinceEpoch).count();
    uint64 const seconds = milliseconds < 0 ? 0 : static_cast<uint64>(milliseconds / 1000);
    SessionTimestamp stamp;
    stamp.TimeHigh = static_cast<int32>(static_cast<uint32>(seconds >> 32));
    stamp.TimeLow = static_cast<int32>(static_cast<uint32>(seconds & 0xFFFFFFFFu));
    stamp.Milliseconds = milliseconds < 0 ? 0 : static_cast<uint32>(milliseconds % 1000);
    return stamp;
}

uint64 SessionTimestamp::GetSeconds() const noexcept
{
    return (uint64{ static_cast<uint32>(TimeHigh) } << 32) | static_cast<uint32>(TimeLow);
}

std::vector<uint8> ControlMessages::Encode(SessionOffer const& message)
{
    ByteBuffer buffer;
    buffer.Write(message.SessionId);
    WriteTimestamp(buffer, message.Time);
    buffer.WriteBytes(message.Trailing);
    return Bytes(buffer);
}

std::vector<uint8> ControlMessages::Encode(SessionAccept const& message)
{
    ByteBuffer buffer;
    buffer.Write(message.Reserved);
    WriteTimestamp(buffer, message.Time);
    buffer.Write(message.SessionId);
    buffer.WriteBytes(message.Trailing);
    return Bytes(buffer);
}

std::vector<uint8> ControlMessages::Encode(ClientKeepAlive const& message)
{
    ByteBuffer buffer;
    buffer.Write(message.SessionId);
    buffer.Write(message.Milliseconds);
    buffer.Write(message.ElapsedMinutes);
    return Bytes(buffer);
}

std::vector<uint8> ControlMessages::Encode(ServerKeepAlive const& message)
{
    ByteBuffer buffer;
    buffer.Write(message.SessionId);
    buffer.Write(message.Milliseconds);
    return Bytes(buffer);
}

std::vector<uint8> ControlMessages::Encode(KeepAliveResponse const& message)
{
    ByteBuffer buffer;
    buffer.Write(message.SessionId);
    buffer.Write(message.Milliseconds);
    buffer.Write(message.ElapsedMinutes);
    return Bytes(buffer);
}

std::optional<SessionOffer> ControlMessages::DecodeSessionOffer(std::span<uint8 const> body)
{
    if (body.size() < SessionOffer::BodySize)
        return std::nullopt;
    ByteBuffer buffer(body);
    SessionOffer message;
    message.SessionId = buffer.Read<uint16>();
    message.Time = ReadTimestamp(buffer);
    message.Trailing = Remaining(buffer);
    return message;
}

std::optional<SessionAccept> ControlMessages::DecodeSessionAccept(std::span<uint8 const> body)
{
    if (body.size() < SessionAccept::BodySize)
        return std::nullopt;
    ByteBuffer buffer(body);
    SessionAccept message;
    message.Reserved = buffer.Read<uint16>();
    message.Time = ReadTimestamp(buffer);
    message.SessionId = buffer.Read<uint16>();
    message.Trailing = Remaining(buffer);
    return message;
}

std::optional<ClientKeepAlive> ControlMessages::DecodeClientKeepAlive(std::span<uint8 const> body)
{
    if (body.size() != ClientKeepAlive::BodySize)
        return std::nullopt;
    ByteBuffer buffer(body);
    ClientKeepAlive message;
    message.SessionId = buffer.Read<uint16>();
    message.Milliseconds = buffer.Read<uint16>();
    message.ElapsedMinutes = buffer.Read<uint16>();
    return message;
}

std::optional<ServerKeepAlive> ControlMessages::DecodeServerKeepAlive(std::span<uint8 const> body)
{
    if (body.size() != ServerKeepAlive::BodySize)
        return std::nullopt;
    ByteBuffer buffer(body);
    ServerKeepAlive message;
    message.SessionId = buffer.Read<uint16>();
    message.Milliseconds = buffer.Read<uint32>();
    return message;
}

std::optional<KeepAliveResponse> ControlMessages::DecodeKeepAliveResponse(std::span<uint8 const> body)
{
    if (body.size() != KeepAliveResponse::BodySize)
        return std::nullopt;
    ByteBuffer buffer(body);
    KeepAliveResponse message;
    message.SessionId = buffer.Read<uint16>();
    message.Milliseconds = buffer.Read<uint16>();
    message.ElapsedMinutes = buffer.Read<uint16>();
    return message;
}

void ControlMessages::WriteFrame(ByteBuffer& out, SessionOffer const& message)
{
    FrameWriter::WriteControl(out, static_cast<uint8>(ControlOpcode::SessionOffer), Encode(message));
}

void ControlMessages::WriteFrame(ByteBuffer& out, SessionAccept const& message)
{
    FrameWriter::WriteControl(out, static_cast<uint8>(ControlOpcode::SessionAccept), Encode(message));
}

void ControlMessages::WriteFrame(ByteBuffer& out, ClientKeepAlive const& message)
{
    FrameWriter::WriteControl(out, static_cast<uint8>(ControlOpcode::KeepAlive), Encode(message));
}

void ControlMessages::WriteFrame(ByteBuffer& out, ServerKeepAlive const& message)
{
    FrameWriter::WriteControl(out, static_cast<uint8>(ControlOpcode::KeepAlive), Encode(message));
}

void ControlMessages::WriteFrame(ByteBuffer& out, KeepAliveResponse const& message)
{
    FrameWriter::WriteControl(out, static_cast<uint8>(ControlOpcode::KeepAliveRsp), Encode(message));
}

std::optional<ControlOpcode> ControlMessages::GetOpcode(Frame const& frame) noexcept
{
    if (!frame.IsControl)
        return std::nullopt;
    switch (frame.Opcode)
    {
        case 0: return ControlOpcode::SessionOffer;
        case 3: return ControlOpcode::KeepAlive;
        case 4: return ControlOpcode::KeepAliveRsp;
        case 5: return ControlOpcode::SessionAccept;
        default: return std::nullopt;
    }
}
