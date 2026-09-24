/*
 * Project Ambrose by Imjustchico
 * Writes frame prefixes, headers, DML sub-headers, payloads, and trailers, checking every length fits its field, and finishes a DML frame in place once its body is written.
 */

#include "FrameWriter.h"

#include <array>
#include <stdexcept>
#include <string>

void FrameWriter::WriteControl(ByteBuffer& out, uint8 opcode, std::span<uint8 const> body, LongFrameLength longLength)
{
    Frame frame;
    frame.IsControl = true;
    frame.Opcode = opcode;
    frame.Payload.assign(body.begin(), body.end());
    WriteFrame(out, frame, longLength);
}

void FrameWriter::WriteDml(ByteBuffer& out, uint8 serviceId, uint8 order, std::span<uint8 const> body, LongFrameLength longLength)
{
    if (body.size() > FrameLayout::MaxDmlBody)
        throw std::length_error("a DML body of " + std::to_string(body.size()) + " bytes does not fit the 16-bit DML length");
    out.Reserve(out.GetSize() + FrameLayout::LongPrefixSize + FrameLayout::FrameHeaderSize + FrameLayout::DmlHeaderSize + body.size() + FrameLayout::TrailerSize);
    std::size_t const start = BeginDml(out, serviceId, order);
    out.WriteBytes(body);
    EndDml(out, start, longLength);
}

std::size_t FrameWriter::BeginDml(ByteBuffer& out, uint8 serviceId, uint8 order)
{
    std::size_t const start = out.GetSize();
    out.Write(FrameLayout::Magic);
    out.Write(uint16{ 0 });
    out.Write(uint8{ 0 });
    out.Write(uint8{ 0 });
    out.Write(uint16{ 0 });
    out.Write(serviceId);
    out.Write(order);
    out.Write(uint16{ 0 });
    return start;
}

void FrameWriter::EndDml(ByteBuffer& out, std::size_t start, LongFrameLength longLength)
{
    std::size_t const headers = FrameLayout::PrefixSize + FrameLayout::FrameHeaderSize + FrameLayout::DmlHeaderSize;
    if (start > out.GetSize() || out.GetSize() - start < headers)
        throw std::invalid_argument("a DML frame can only be finished after BeginDml wrote its headers");
    std::size_t const body = out.GetSize() - start - headers;
    if (body > FrameLayout::MaxDmlBody)
        throw std::length_error("a DML body of " + std::to_string(body) + " bytes does not fit the 16-bit DML length");
    out.Put<uint16>(start + FrameLayout::PrefixSize + FrameLayout::FrameHeaderSize + 2, static_cast<uint16>(body + FrameLayout::DmlHeaderSize));
    out.Write(uint8{ 0 });
    if (body <= FrameLayout::MaxShortBody)
    {
        out.Put<uint16>(start + 2, static_cast<uint16>(FrameLayout::FrameHeaderSize + FrameLayout::DmlHeaderSize + body + FrameLayout::TrailerSize));
        return;
    }
    std::size_t const declared = longLength == LongFrameLength::BodyOnly ? body : FrameLayout::FrameHeaderSize + FrameLayout::DmlHeaderSize + body;
    std::array<uint8, FrameLayout::LongPrefixSize - FrameLayout::PrefixSize> const lengthBytes{};
    out.InsertBytes(start + FrameLayout::PrefixSize, lengthBytes);
    out.Put<uint16>(start + 2, FrameLayout::LongLengthMarker);
    out.Put<uint32>(start + FrameLayout::PrefixSize, static_cast<uint32>(declared));
}

void FrameWriter::WriteDml(ByteBuffer& out, std::span<DmlMessageData const> messages, LongFrameLength longLength)
{
    if (messages.empty())
        throw std::invalid_argument("a DML frame needs at least one message");
    Frame frame;
    for (DmlMessageData const& message : messages)
    {
        if (message.Body.size() > FrameLayout::MaxDmlBody)
            throw std::length_error("a DML body of " + std::to_string(message.Body.size()) + " bytes does not fit the 16-bit DML length");
        uint16 const dmlLength = static_cast<uint16>(message.Body.size() + FrameLayout::DmlHeaderSize);
        frame.Payload.push_back(message.ServiceId);
        frame.Payload.push_back(message.Order);
        frame.Payload.push_back(static_cast<uint8>(dmlLength & 0xFF));
        frame.Payload.push_back(static_cast<uint8>(dmlLength >> 8));
        frame.Payload.insert(frame.Payload.end(), message.Body.begin(), message.Body.end());
    }
    WriteFrame(out, frame, longLength);
}

void FrameWriter::WriteFrame(ByteBuffer& out, Frame const& frame, LongFrameLength longLength)
{
    std::size_t const headerBytes = frame.IsControl ? 0 : FrameLayout::DmlHeaderSize;
    if (!frame.IsControl && frame.Payload.size() < headerBytes)
        throw std::invalid_argument("a DML frame payload must hold at least one DML header");
    std::size_t const body = frame.Payload.size() - headerBytes;
    bool const isLong = frame.IsLong || body > FrameLayout::MaxShortBody;
    WritePrefix(out, frame, isLong, longLength);
    out.Write(static_cast<uint8>(frame.IsControl ? 1 : 0));
    out.Write(frame.Opcode);
    out.Write(frame.Reserved);
    out.WriteBytes(frame.Payload);
    out.Write(frame.Trailer);
}

void FrameWriter::WritePrefix(ByteBuffer& out, Frame const& frame, bool isLong, LongFrameLength longLength)
{
    out.Write(FrameLayout::Magic);
    std::size_t const afterLength = FrameLayout::FrameHeaderSize + frame.Payload.size() + FrameLayout::TrailerSize;
    if (!isLong)
    {
        out.Write(static_cast<uint16>(afterLength));
        return;
    }
    std::size_t const declared = longLength == LongFrameLength::BodyOnly
        ? frame.Payload.size() - (frame.IsControl ? 0 : FrameLayout::DmlHeaderSize)
        : FrameLayout::FrameHeaderSize + frame.Payload.size();
    if (declared > 0xFFFFFFFFull)
        throw std::length_error("a frame of " + std::to_string(declared) + " bytes does not fit the 32-bit long length");
    out.Write(FrameLayout::LongLengthMarker);
    out.Write(static_cast<uint32>(declared));
}
