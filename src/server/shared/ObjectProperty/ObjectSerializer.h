/*
 * Project Ambrose by Imjustchico
 * Encodes and decodes property objects in the compact ObjectProperty format the client uses inside messages: a class hash per object, then the properties the mask selects in id order with no headers, bits packed least significant first, with every decode bounded by depth, object, list and memory limits, the root optionally held to a set of classes, and every failure named with the property path it happened at.
 */

#ifndef AMBROSE_OBJECTSERIALIZER_H
#define AMBROSE_OBJECTSERIALIZER_H

#include "EnumFlag.h"
#include "PropertyFlags.h"
#include "PropertyObject.h"

#include <cstddef>
#include <functional>
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
    UnknownEnumName,
    ValueTooLong,
    UnsupportedFlags,
    UnsupportedType
};

struct SerializerLimits
{
    static constexpr uint32 DepthCeiling = 128;

    uint32 MaxDepth = 64;
    uint32 MaxObjects = 65536;
    uint32 MaxContainerCount = 65536;
    std::size_t MaxDecodedBytes = std::size_t{ 16 } << 20;
};

struct SerializerOptions
{
    static constexpr uint32 TransmitMask = PropertyFlags::Bit(PropertyFlag::Transmit) | PropertyFlags::Bit(PropertyFlag::AuthorityTransmit);
    static constexpr uint32 PublicMask = TransmitMask | PropertyFlags::Bit(PropertyFlag::Public);

    uint32 Mask = TransmitMask;
    SerializerFlag Flags = SerializerFlag::None;
    SerializerLimits Limits;
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
    static bool IsSelected(PropertyInfo const& property, uint32 mask) noexcept;
    static std::string_view GetStatusName(SerializerStatus status) noexcept;
};

#endif
