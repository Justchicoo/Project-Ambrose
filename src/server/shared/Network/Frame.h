/*
 * Project Ambrose by Imjustchico
 * KI 0xF00D frame layout: constants, frame and DML message values, length rules for short and long frames, and error names.
 */

#ifndef AMBROSE_FRAME_H
#define AMBROSE_FRAME_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

enum class LongFrameLength : uint8
{
    BodyOnly,
    HeaderAndBody
};

enum class FrameError : uint8
{
    None,
    BadMagic,
    BadLength,
    BadControlFlag,
    TooLarge,
    BadDmlLength,
    TooManyDmlMessages
};

struct FrameLimits
{
    static constexpr std::size_t DefaultMaxFrameSize = std::size_t{ 4 } << 20;
    static constexpr std::size_t DefaultMaxDmlMessages = 1024;

    std::size_t MaxFrameSize = DefaultMaxFrameSize;
    LongFrameLength LongLength = LongFrameLength::BodyOnly;
    std::size_t MaxDmlMessages = DefaultMaxDmlMessages;
};

struct DmlMessageData
{
    uint8 ServiceId = 0;
    uint8 Order = 0;
    std::vector<uint8> Body;

    bool operator==(DmlMessageData const&) const = default;
};

struct Frame
{
    bool IsControl = false;
    uint8 Opcode = 0;
    uint16 Reserved = 0;
    uint8 Trailer = 0;
    bool IsLong = false;
    std::vector<uint8> Payload;

    bool operator==(Frame const&) const = default;
};

namespace FrameLayout
{
    inline constexpr uint16 Magic = 0xF00D;
    inline constexpr uint16 LongLengthMarker = 0x8000;
    inline constexpr std::size_t PrefixSize = 4;
    inline constexpr std::size_t LongPrefixSize = 8;
    inline constexpr std::size_t FrameHeaderSize = 4;
    inline constexpr std::size_t DmlHeaderSize = 4;
    inline constexpr std::size_t TrailerSize = 1;
    inline constexpr std::size_t MaxShortBody = 0x777F;
    inline constexpr std::size_t MaxDmlBody = 0xFFFF - DmlHeaderSize;

    std::optional<uint64> GetFrameSize(std::span<uint8 const> prefix, LongFrameLength longLength, FrameError& error) noexcept;
    inline constexpr std::size_t Unlimited = static_cast<std::size_t>(-1);

    FrameError ValidateDmlPayload(std::span<uint8 const> payload, std::size_t maxMessages = Unlimited, std::size_t* count = nullptr) noexcept;
    FrameError SplitDmlMessages(std::span<uint8 const> payload, std::vector<DmlMessageData>& messages, std::size_t maxMessages = Unlimited);
    std::string_view GetErrorName(FrameError error) noexcept;
}

#endif
