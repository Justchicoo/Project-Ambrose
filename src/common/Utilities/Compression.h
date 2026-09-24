/*
 * Project Ambrose by Imjustchico
 * zlib, gzip, and raw deflate compression with hard output caps that reject oversized or corrupt streams.
 */

#ifndef AMBROSE_COMPRESSION_H
#define AMBROSE_COMPRESSION_H

#include "Types.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace Ambrose::Compression
{
    enum class Format : uint8
    {
        Zlib,
        Gzip,
        Raw,
        Auto
    };

    enum class Status : uint8
    {
        Ok,
        OutputTooLarge,
        SizeMismatch,
        Truncated,
        Corrupt,
        TrailingData,
        OutOfMemory
    };

    struct InflateResult
    {
        Status Code = Status::Ok;
        std::vector<uint8> Data;
        std::size_t ConsumedInput = 0;

        bool Succeeded() const noexcept { return Code == Status::Ok; }
    };

    inline constexpr int DefaultLevel = 6;

    InflateResult Inflate(std::span<uint8 const> input, std::size_t maxOutputSize, Format format = Format::Zlib, bool allowTrailingData = false);
    InflateResult InflateExact(std::span<uint8 const> input, std::size_t expectedSize, Format format = Format::Zlib);
    std::vector<uint8> Deflate(std::span<uint8 const> input, int level = DefaultLevel, Format format = Format::Zlib);
    std::string_view GetStatusName(Status status) noexcept;
}

#endif
