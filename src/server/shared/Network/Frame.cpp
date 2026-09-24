/*
 * Project Ambrose by Imjustchico
 * Computes a frame's full size from its first bytes, rejecting bad magic, lengths, and control flags early, and splits chained DML messages.
 */

#include "Frame.h"

namespace
{
    uint16 ReadUInt16(std::span<uint8 const> bytes, std::size_t offset) noexcept
    {
        return static_cast<uint16>(bytes[offset] | (bytes[offset + 1] << 8));
    }

    uint32 ReadUInt32(std::span<uint8 const> bytes, std::size_t offset) noexcept
    {
        return uint32{ bytes[offset] } | (uint32{ bytes[offset + 1] } << 8) | (uint32{ bytes[offset + 2] } << 16) | (uint32{ bytes[offset + 3] } << 24);
    }
}

std::optional<uint64> FrameLayout::GetFrameSize(std::span<uint8 const> prefix, LongFrameLength longLength, FrameError& error) noexcept
{
    error = FrameError::None;
    if ((prefix.size() >= 1 && prefix[0] != (Magic & 0xFF)) || (prefix.size() >= 2 && prefix[1] != (Magic >> 8)))
    {
        error = FrameError::BadMagic;
        return std::nullopt;
    }
    if (prefix.size() < PrefixSize)
        return std::nullopt;

    uint16 const length = ReadUInt16(prefix, 2);
    bool const isLong = length == LongLengthMarker;
    std::size_t const headerOffset = isLong ? LongPrefixSize : PrefixSize;
    if (prefix.size() < headerOffset + 1)
        return std::nullopt;

    uint8 const controlFlag = prefix[headerOffset];
    if (controlFlag > 1)
    {
        error = FrameError::BadControlFlag;
        return std::nullopt;
    }
    uint64 const minimumAfterPrefix = FrameHeaderSize + (controlFlag == 1 ? 0 : DmlHeaderSize) + TrailerSize;

    uint64 total = 0;
    if (!isLong)
    {
        if (length < minimumAfterPrefix)
        {
            error = FrameError::BadLength;
            return std::nullopt;
        }
        total = PrefixSize + uint64{ length };
    }
    else
    {
        uint64 const declared = ReadUInt32(prefix, 4);
        if (longLength == LongFrameLength::BodyOnly)
            total = LongPrefixSize + minimumAfterPrefix + declared;
        else
        {
            if (declared + TrailerSize < minimumAfterPrefix)
            {
                error = FrameError::BadLength;
                return std::nullopt;
            }
            total = LongPrefixSize + declared + TrailerSize;
        }
    }
    return total;
}

FrameError FrameLayout::ValidateDmlPayload(std::span<uint8 const> payload, std::size_t maxMessages, std::size_t* count) noexcept
{
    if (payload.empty())
        return FrameError::BadDmlLength;
    std::size_t offset = 0;
    std::size_t messages = 0;
    while (offset < payload.size())
    {
        if (messages == maxMessages)
            return FrameError::TooManyDmlMessages;
        std::size_t const remaining = payload.size() - offset;
        if (remaining < DmlHeaderSize)
            return FrameError::BadDmlLength;
        uint16 const dmlLength = ReadUInt16(payload, offset + 2);
        if (dmlLength < DmlHeaderSize || dmlLength > remaining)
            return FrameError::BadDmlLength;
        offset += dmlLength;
        ++messages;
    }
    if (count)
        *count = messages;
    return FrameError::None;
}

FrameError FrameLayout::SplitDmlMessages(std::span<uint8 const> payload, std::vector<DmlMessageData>& messages, std::size_t maxMessages)
{
    std::size_t count = 0;
    if (FrameError const error = ValidateDmlPayload(payload, maxMessages, &count); error != FrameError::None)
        return error;
    messages.reserve(messages.size() + count);
    std::size_t offset = 0;
    while (offset < payload.size())
    {
        uint16 const dmlLength = ReadUInt16(payload, offset + 2);
        DmlMessageData message;
        message.ServiceId = payload[offset];
        message.Order = payload[offset + 1];
        message.Body.assign(payload.begin() + static_cast<std::ptrdiff_t>(offset + DmlHeaderSize), payload.begin() + static_cast<std::ptrdiff_t>(offset + dmlLength));
        messages.push_back(std::move(message));
        offset += dmlLength;
    }
    return FrameError::None;
}

std::string_view FrameLayout::GetErrorName(FrameError error) noexcept
{
    switch (error)
    {
        case FrameError::None: return "none";
        case FrameError::BadMagic: return "bad magic";
        case FrameError::BadLength: return "bad length";
        case FrameError::BadControlFlag: return "bad control flag";
        case FrameError::TooLarge: return "frame too large";
        case FrameError::BadDmlLength: return "bad DML length";
        case FrameError::TooManyDmlMessages: return "too many DML messages";
    }
    return "unknown";
}
