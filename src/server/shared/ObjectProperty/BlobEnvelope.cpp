/*
 * Project Ambrose by Imjustchico
 * Wraps payloads as stored or zlib-compressed envelopes, and unwraps them with every length checked against the input and the caller's cap before anything is allocated or inflated, telling truncated, oversized, mismatched, trailing and corrupt input apart from the server running out of memory.
 */

#include "BlobEnvelope.h"
#include "Compression.h"

#include <stdexcept>
#include <string>

namespace
{
    void WriteHeader(std::vector<uint8>& out, uint32 header)
    {
        out.push_back(static_cast<uint8>(header & 0xFF));
        out.push_back(static_cast<uint8>((header >> 8) & 0xFF));
        out.push_back(static_cast<uint8>((header >> 16) & 0xFF));
        out.push_back(static_cast<uint8>((header >> 24) & 0xFF));
    }

    BlobEnvelope::UnwrapResult Failure(BlobEnvelope::Status status, BlobEnvelope::Packing packing)
    {
        BlobEnvelope::UnwrapResult result;
        result.Code = status;
        result.Packed = packing;
        return result;
    }
}

std::vector<uint8> BlobEnvelope::Wrap(std::span<uint8 const> payload, Packing packing)
{
    if (payload.size() > MaxPayloadSize)
        throw std::length_error("a blob of " + std::to_string(payload.size()) + " bytes does not fit the 31-bit envelope length");
    std::vector<uint8> out;
    if (packing == Packing::Store)
    {
        out.reserve(HeaderSize + payload.size());
        WriteHeader(out, StoredFlag | static_cast<uint32>(payload.size()));
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }
    std::vector<uint8> const compressed = Ambrose::Compression::Deflate(payload, Ambrose::Compression::DefaultLevel, Ambrose::Compression::Format::Zlib);
    out.reserve(HeaderSize + compressed.size());
    WriteHeader(out, static_cast<uint32>(payload.size()));
    out.insert(out.end(), compressed.begin(), compressed.end());
    return out;
}

BlobEnvelope::UnwrapResult BlobEnvelope::Unwrap(std::span<uint8 const> blob, std::size_t maxPayloadSize)
{
    if (blob.size() < HeaderSize)
        return Failure(Status::Truncated, Packing::Store);
    uint32 const header = uint32{ blob[0] } | (uint32{ blob[1] } << 8) | (uint32{ blob[2] } << 16) | (uint32{ blob[3] } << 24);
    std::span<uint8 const> const body = blob.subspan(HeaderSize);
    std::size_t const length = header & ~StoredFlag;
    Packing const packing = (header & StoredFlag) != 0 ? Packing::Store : Packing::Compress;
    if (length > maxPayloadSize)
        return Failure(Status::TooLarge, packing);

    if (packing == Packing::Store)
    {
        if (body.size() < length)
            return Failure(Status::Truncated, packing);
        if (body.size() > length)
            return Failure(Status::SizeMismatch, packing);
        UnwrapResult result;
        result.Packed = packing;
        result.Data.assign(body.begin(), body.end());
        return result;
    }

    Ambrose::Compression::InflateResult inflated = Ambrose::Compression::InflateExact(body, length, Ambrose::Compression::Format::Zlib);
    switch (inflated.Code)
    {
        case Ambrose::Compression::Status::Ok:
        {
            UnwrapResult result;
            result.Packed = packing;
            result.Data = std::move(inflated.Data);
            return result;
        }
        case Ambrose::Compression::Status::OutputTooLarge:
        case Ambrose::Compression::Status::SizeMismatch:
            return Failure(Status::SizeMismatch, packing);
        case Ambrose::Compression::Status::TrailingData:
            return Failure(Status::TrailingData, packing);
        case Ambrose::Compression::Status::Truncated:
            return Failure(Status::Truncated, packing);
        case Ambrose::Compression::Status::OutOfMemory:
            return Failure(Status::OutOfMemory, packing);
        case Ambrose::Compression::Status::Corrupt:
            break;
    }
    return Failure(Status::Corrupt, packing);
}

std::string_view BlobEnvelope::GetStatusName(Status status) noexcept
{
    switch (status)
    {
        case Status::Ok: return "ok";
        case Status::Truncated: return "the blob ends before its declared length";
        case Status::TooLarge: return "the declared length is above the limit";
        case Status::SizeMismatch: return "the payload length disagrees with the header";
        case Status::TrailingData: return "bytes follow the end of the zlib stream";
        case Status::Corrupt: return "the zlib stream is corrupt";
        case Status::OutOfMemory: return "the server ran out of memory unwrapping it";
    }
    return "unknown";
}
