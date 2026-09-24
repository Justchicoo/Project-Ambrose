/*
 * Project Ambrose by Imjustchico
 * The 4-byte envelope the client unwraps before parsing an ObjectProperty blob: a little-endian header whose top bit marks a stored payload of the low 31 bits' length, or else holds the uncompressed size of the zlib stream that follows.
 */

#ifndef AMBROSE_BLOBENVELOPE_H
#define AMBROSE_BLOBENVELOPE_H

#include "Types.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace BlobEnvelope
{
    inline constexpr std::size_t HeaderSize = 4;
    inline constexpr uint32 StoredFlag = 0x80000000u;
    inline constexpr std::size_t MaxPayloadSize = 0x7FFFFFFFu;

    enum class Packing : uint8
    {
        Store,
        Compress
    };

    enum class Status : uint8
    {
        Ok,
        Truncated,
        TooLarge,
        SizeMismatch,
        TrailingData,
        Corrupt,
        OutOfMemory
    };

    struct UnwrapResult
    {
        Status Code = Status::Ok;
        Packing Packed = Packing::Store;
        std::vector<uint8> Data;

        bool Succeeded() const noexcept { return Code == Status::Ok; }
    };

    std::vector<uint8> Wrap(std::span<uint8 const> payload, Packing packing);
    UnwrapResult Unwrap(std::span<uint8 const> blob, std::size_t maxPayloadSize);
    std::string_view GetStatusName(Status status) noexcept;
}

#endif
