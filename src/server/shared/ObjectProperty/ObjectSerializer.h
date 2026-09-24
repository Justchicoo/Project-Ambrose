/*
 * Project Ambrose by Imjustchico
 * Encodes and decodes property objects in the compact ObjectProperty format the client uses inside messages, a class hash per object then the properties the mask selects in id order with no headers, and in the versionable format its data files use, where every object and property carries its size in bits and every property its hash, so unknown or unselected ones are skipped and reported and a clean dirty-encoded property is left out; bits pack least significant first, lengths are fixed-width or compact, every decode is bounded by depth, object, list, memory and inflation limits read from live settings, the root is optionally held to a set of classes or to the rules of the message field it came from, and every failure names the property path it happened at. Every field of the option and issue structures carries a default, so naming only the fields a caller cares about is the intended way to build one rather than an omission GCC refuses.
 */

#ifndef AMBROSE_OBJECTSERIALIZER_H
#define AMBROSE_OBJECTSERIALIZER_H

#include "EnumFlag.h"
#include "ObjectFields.h"
#include "PropertyFlags.h"
#include "PropertyObject.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

enum class SerializerFlag : uint32
{
    None = 0x00,
    SerializeFlags = 0x01,
    CompactLength = 0x02,
    StringEnums = 0x04,
    Compress = 0x08,
    ForceDirtyEncode = 0x10
};

DEFINE_ENUM_FLAG(SerializerFlag);

enum class SerializerStatus : uint8
{
    Ok,
    Truncated,
    TrailingBytes,
    UnknownClass,
    NotAPropertyClass,
    WrongClass,
    NullNotAllowed,
    TooDeep,
    TooManyObjects,
    ContainerTooLarge,
    BudgetExceeded,
    OutOfMemory,
    BadEnvelope,
    UnknownEnumName,
    ValueTooLong,
    UnsupportedFlags,
    UnsupportedType,
    BadSize
};

enum class DecodeIssueKind : uint8
{
    UnknownClass,
    UnknownProperty,
    SizeMismatch,
    UnsupportedType,
    UnknownEnumName,
    InvalidObject,
    UnselectedProperty,
    InvalidValue
};

class ConfigMgr;

struct SerializerLimits
{
    static constexpr uint32 DepthCeiling = 128;
    static constexpr uint32 CountCeiling = uint32{ 1 } << 24;
    static constexpr uint64 DecodedBytesFloor = uint64{ 1 } << 16;
    static constexpr uint64 DecodedBytesCeiling = uint64{ 1 } << 30;
    static constexpr uint64 InflatedSizeFloor = uint64{ 1 } << 10;
    static constexpr uint64 InflatedSizeCeiling = uint64{ 256 } << 20;

    uint32 MaxDepth = 64;
    uint32 MaxObjects = 65536;
    uint32 MaxContainerCount = 65536;
    std::size_t MaxDecodedBytes = std::size_t{ 16 } << 20;
    std::size_t MaxInflatedSize = std::size_t{ 4 } << 20;

    static SerializerLimits Load(ConfigMgr const& config, std::vector<std::string>* problems = nullptr);
    static SerializerLimits Current();
    static void Apply(SerializerLimits const& limits);

    bool operator==(SerializerLimits const&) const = default;
};

struct SerializerOptions
{
    static constexpr uint32 TransmitMask = PropertyFlags::Bit(PropertyFlag::Transmit) | PropertyFlags::Bit(PropertyFlag::AuthorityTransmit);
    static constexpr uint32 PublicMask = TransmitMask | PropertyFlags::Bit(PropertyFlag::Public);

    uint32 Mask = TransmitMask;
    SerializerFlag Flags = SerializerFlag::None;
    std::optional<SerializerLimits> Limits = {};
    bool Versionable = false;
    bool AllowTrailingBytes = false;
    bool AllowNullRoot = true;
    std::vector<ClassInfo const*> RootClasses = {};
    std::function<bool(PropertyObject const& object, PropertyInfo const& property)> IsDirty = {};
};

struct DecodeIssue
{
    DecodeIssueKind Kind = DecodeIssueKind::UnknownClass;
    uint32 Hash = 0;
    uint64 Bits = 0;
    std::string Path = {};
    std::string Detail = {};
};

struct DecodeResult
{
    PropertyObjectPtr Object;
    SerializerStatus Status = SerializerStatus::Ok;
    std::size_t BytesRead = 0;
    std::string Detail;
    std::vector<DecodeIssue> Issues;

    bool Ok() const noexcept { return Status == SerializerStatus::Ok; }
};

struct EncodeResult
{
    std::vector<uint8> Bytes;
    SerializerStatus Status = SerializerStatus::Ok;
    std::string Detail;

    bool Ok() const noexcept { return Status == SerializerStatus::Ok; }
};

class ObjectSerializer
{
public:
    ObjectSerializer() = delete;

    static DecodeResult Decode(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes, SerializerOptions const& options = {});
    static EncodeResult Encode(PropertyObject const* object, SerializerOptions const& options = {});
    static DecodeResult DecodeField(TypeCatalogPtr const& catalog, ObjectField const& field, std::span<uint8 const> bytes, SerializerOptions options = {});
    static EncodeResult EncodeField(ObjectField const& field, PropertyObject const* object, SerializerOptions options = {});
    static bool IsSelected(PropertyInfo const& property, uint32 mask) noexcept;
    static std::string_view GetStatusName(SerializerStatus status) noexcept;
    static std::string_view GetIssueName(DecodeIssueKind kind) noexcept;
};

#endif
