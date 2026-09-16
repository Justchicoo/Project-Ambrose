/*
 * Project Ambrose by Imjustchico
 * Reads and writes the client's BINd data files: the BINd magic, the u32 serializer flags the object was written with, and when those flags ask for compression a padding byte, the u32 inflated size and a zlib stream, around one versionable ObjectProperty object; reading refuses anything that is not such a file or inflates past its limit and returns the root class hash and the decoded object with the issues its decode reported, and writing leaves out dirty-encoded properties at their defaults as the client's own files do.
 */

#ifndef AMBROSE_BINDFILE_H
#define AMBROSE_BINDFILE_H

#include "ObjectSerializer.h"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

enum class BindStatus : uint8
{
    Ok,
    NotBind,
    Truncated,
    UnknownFlags,
    TooLarge,
    BadCompression,
    OutOfMemory,
    DecodeFailed
};

struct BindReadResult
{
    BindStatus Status = BindStatus::Ok;
    SerializerFlag Flags = SerializerFlag::None;
    uint32 RootClassHash = 0;
    DecodeResult Decoded;
    std::string Detail;

    bool Ok() const noexcept { return Status == BindStatus::Ok; }
};

class BindFile
{
public:
    static constexpr std::array<uint8, 4> Magic{ 'B', 'I', 'N', 'd' };
    static constexpr std::size_t HeaderSize = 8;
    static constexpr std::size_t CompressedHeaderSize = 13;
    static constexpr SerializerFlag KnownFlags = SerializerFlag::SerializeFlags | SerializerFlag::CompactLength | SerializerFlag::StringEnums | SerializerFlag::Compress | SerializerFlag::ForceDirtyEncode;
    static constexpr SerializerFlag DefaultFlags = SerializerFlag::SerializeFlags | SerializerFlag::CompactLength | SerializerFlag::StringEnums;
    static constexpr uint32 SaveMask = PropertyFlags::Bit(PropertyFlag::Save);

    BindFile() = delete;

    static bool IsBind(std::span<uint8 const> bytes) noexcept;
    static SerializerLimits GetDefaultLimits() noexcept;
    static BindReadResult Read(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes, std::optional<SerializerLimits> limits = std::nullopt);
    static EncodeResult Write(PropertyObject const* object, SerializerFlag flags = DefaultFlags, uint32 mask = SaveMask);
    static std::string_view GetStatusName(BindStatus status) noexcept;
};

#endif
