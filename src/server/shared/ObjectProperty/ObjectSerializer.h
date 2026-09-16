/*
 * Project Ambrose by Imjustchico
 * Encodes and decodes property objects in the compact ObjectProperty format the client uses inside messages: a class hash per object, then the properties the mask selects in id order with no headers, bits packed least significant first, with every decode bounded by depth, object, list, memory and inflation limits read from live settings, the root optionally held to a set of classes or to the rules of the message field it came from, and every failure named with the property path it happened at.
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
    UnsupportedType
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
    std::optional<SerializerLimits> Limits;
    bool AllowTrailingBytes = false;
    bool AllowNullRoot = true;
    std::vector<ClassInfo const*> RootClasses;
    std::function<bool(PropertyObject const& object, PropertyInfo const& property)> IsDirty;
};

struct DecodeResult
{
    PropertyObjectPtr Object;
    SerializerStatus Status = SerializerStatus::Ok;
    std::size_t BytesRead = 0;
    std::string Detail;

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

    static DecodeResult DecodeCompact(TypeCatalogPtr const& catalog, std::span<uint8 const> bytes, SerializerOptions const& options = {});
    static EncodeResult EncodeCompact(PropertyObject const* object, SerializerOptions const& options = {});
    static DecodeResult DecodeField(TypeCatalogPtr const& catalog, ObjectField const& field, std::span<uint8 const> bytes, SerializerOptions options = {});
    static EncodeResult EncodeField(ObjectField const& field, PropertyObject const* object, SerializerOptions options = {});
    static bool IsSelected(PropertyInfo const& property, uint32 mask) noexcept;
    static std::string_view GetStatusName(SerializerStatus status) noexcept;
};

#endif
