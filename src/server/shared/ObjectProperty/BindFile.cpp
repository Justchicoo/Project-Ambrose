/*
 * Project Ambrose by Imjustchico
 * Checks a BINd file's magic and flags, inflates a compressed one to exactly its recorded size within the inflation limit, notes the root class hash, and decodes the versionable object, which must not be null, with the file's own length and enum modes under generous default limits meant for the user's own client data; writes an object the same way under the Save mask and the same limits, treating a dirty-encoded property equal to its default as clean unless ForceDirtyEncode is set, and compressing it when asked.
 */

#include "BindFile.h"
#include "Compression.h"

#include <fmt/format.h>

#include <algorithm>
#include <exception>
#include <limits>
#include <new>

namespace
{
    constexpr SerializerFlag PayloadFlags = SerializerFlag::CompactLength | SerializerFlag::StringEnums;
    constexpr uint8 PaddingByte = 0x01;

    bool HasFlag(SerializerFlag flags, SerializerFlag flag) noexcept
    {
        return (flags & flag) != SerializerFlag::None;
    }

    uint32 ReadU32(std::span<uint8 const> bytes, std::size_t at) noexcept
    {
        return uint32{ bytes[at] } | (uint32{ bytes[at + 1] } << 8) | (uint32{ bytes[at + 2] } << 16) | (uint32{ bytes[at + 3] } << 24);
    }

    void AppendU32(std::vector<uint8>& bytes, uint32 value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<uint8>(value >> shift));
    }

    BindReadResult Refuse(BindStatus status, SerializerFlag flags, std::string detail)
    {
        BindReadResult result;
        result.Status = status;
        result.Flags = flags;
        result.Detail = std::move(detail);
        return result;
    }

    EncodeResult RefuseWrite(SerializerStatus status, std::string detail)
    {
        EncodeResult result;
        result.Status = status;
        result.Detail = std::move(detail);
        return result;
    }
}

bool BindFile::IsBind(std::span<uint8 const> bytes) noexcept
{
    return bytes.size() >= Magic.size() && std::equal(Magic.begin(), Magic.end(), bytes.begin());
}

SerializerLimits BindFile::GetDefaultLimits() noexcept
{
    SerializerLimits limits;
    limits.MaxDepth = SerializerLimits::DepthCeiling;
    limits.MaxObjects = SerializerLimits::CountCeiling;
    limits.MaxContainerCount = SerializerLimits::CountCeiling;
    limits.MaxDecodedBytes = static_cast<std::size_t>(SerializerLimits::DecodedBytesCeiling);
    limits.MaxInflatedSize = static_cast<std::size_t>(SerializerLimits::InflatedSizeCeiling);
    return limits;
}

BindReadResult BindFile::Read(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes, std::optional<SerializerLimits> limits)
{
    if (!IsBind(bytes))
        return Refuse(BindStatus::NotBind, SerializerFlag::None, "does not start with BINd");
    if (bytes.size() < HeaderSize)
        return Refuse(BindStatus::Truncated, SerializerFlag::None, fmt::format("is {} bytes, shorter than the {}-byte BINd header", bytes.size(), HeaderSize));
    SerializerFlag const flags = static_cast<SerializerFlag>(ReadU32(bytes, Magic.size()));
    if ((flags & ~KnownFlags) != SerializerFlag::None)
        return Refuse(BindStatus::UnknownFlags, flags, fmt::format("carries serializer flags {:#x}, which include bits no known mode uses", static_cast<uint32>(flags)));
    SerializerLimits const applied = limits.value_or(GetDefaultLimits());

    std::span<uint8 const> payload = bytes.subspan(HeaderSize);
    std::vector<uint8> inflated;
    if (HasFlag(flags, SerializerFlag::Compress))
    {
        if (bytes.size() < CompressedHeaderSize)
            return Refuse(BindStatus::Truncated, flags, fmt::format("is {} bytes, shorter than the {}-byte header of a compressed BINd file", bytes.size(), CompressedHeaderSize));
        uint32 const size = ReadU32(bytes, HeaderSize + 1);
        if (size > applied.MaxInflatedSize)
            return Refuse(BindStatus::TooLarge, flags, fmt::format("inflates to {} bytes, above the limit of {}", size, applied.MaxInflatedSize));
        try
        {
            Ambrose::Compression::InflateResult result = Ambrose::Compression::InflateExact(bytes.subspan(CompressedHeaderSize), size, Ambrose::Compression::Format::Zlib);
            if (result.Code == Ambrose::Compression::Status::OutOfMemory)
                return Refuse(BindStatus::OutOfMemory, flags, "ran out of memory while inflating");
            if (!result.Succeeded())
                return Refuse(BindStatus::BadCompression, flags, fmt::format("holds a zlib stream that does not inflate to its {} bytes: {}", size, Ambrose::Compression::GetStatusName(result.Code)));
            inflated = std::move(result.Data);
        }
        catch (std::bad_alloc const&)
        {
            return Refuse(BindStatus::OutOfMemory, flags, "ran out of memory while inflating");
        }
        payload = inflated;
    }

    SerializerOptions options;
    options.Versionable = true;
    options.Mask = SaveMask;
    options.Flags = flags & PayloadFlags;
    options.Limits = applied;
    options.AllowNullRoot = false;
    BindReadResult result;
    result.Flags = flags;
    if (payload.size() >= sizeof(uint32))
        result.RootClassHash = ReadU32(payload, 0);
    result.Decoded = ObjectSerializer::Decode(catalog, payload, options);
    if (!result.Decoded.Ok())
    {
        result.Status = result.Decoded.Status == SerializerStatus::OutOfMemory ? BindStatus::OutOfMemory : BindStatus::DecodeFailed;
        result.Detail = result.Decoded.Detail;
    }
    return result;
}

EncodeResult BindFile::Write(PropertyObject const* object, SerializerFlag flags, uint32 mask)
{
    flags |= SerializerFlag::SerializeFlags;
    if ((flags & ~KnownFlags) != SerializerFlag::None)
        return RefuseWrite(SerializerStatus::UnsupportedFlags, fmt::format("serializer flags {:#x} include bits no known mode uses", static_cast<uint32>(flags)));
    SerializerOptions options;
    options.Versionable = true;
    options.Mask = mask;
    options.Flags = flags & (PayloadFlags | SerializerFlag::ForceDirtyEncode);
    options.Limits = GetDefaultLimits();
    options.IsDirty = [](PropertyObject const& owner, PropertyInfo const& property)
    {
        if (owner.IsPresent(property.Id))
            return true;
        PropertyValue const* const value = owner.Get(property.Hash);
        return !value || !(*value == PropertyObject::MakeDefault(owner.GetCatalog(), property));
    };
    EncodeResult encoded = ObjectSerializer::Encode(object, options);
    if (!encoded.Ok())
        return encoded;

    try
    {
        std::vector<uint8> file(Magic.begin(), Magic.end());
        AppendU32(file, static_cast<uint32>(flags));
        if (!HasFlag(flags, SerializerFlag::Compress))
        {
            file.insert(file.end(), encoded.Bytes.begin(), encoded.Bytes.end());
            encoded.Bytes = std::move(file);
            return encoded;
        }
        if (encoded.Bytes.size() > std::numeric_limits<uint32>::max())
            return RefuseWrite(SerializerStatus::ValueTooLong, fmt::format("the object takes {} bytes, more than a compressed BINd file's u32 size can hold", encoded.Bytes.size()));
        file.push_back(PaddingByte);
        AppendU32(file, static_cast<uint32>(encoded.Bytes.size()));
        std::vector<uint8> const compressed = Ambrose::Compression::Deflate(encoded.Bytes, Ambrose::Compression::DefaultLevel, Ambrose::Compression::Format::Zlib);
        file.insert(file.end(), compressed.begin(), compressed.end());
        encoded.Bytes = std::move(file);
        return encoded;
    }
    catch (std::exception const& error)
    {
        return RefuseWrite(SerializerStatus::OutOfMemory, fmt::format("the file could not be assembled: {}", error.what()));
    }
}

std::string_view BindFile::GetStatusName(BindStatus status) noexcept
{
    switch (status)
    {
        case BindStatus::Ok: return "ok";
        case BindStatus::NotBind: return "the data is not a BINd file";
        case BindStatus::Truncated: return "the file ends inside its header";
        case BindStatus::UnknownFlags: return "the file carries unknown serializer flags";
        case BindStatus::TooLarge: return "the file inflates past the limit";
        case BindStatus::BadCompression: return "the file's zlib stream is corrupt";
        case BindStatus::OutOfMemory: return "memory ran out";
        case BindStatus::DecodeFailed: return "the file's object could not be decoded";
    }
    return "unknown";
}
