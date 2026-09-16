/*
 * Project Ambrose by Imjustchico
 * The schema of one client type dump: class kinds, value kinds, container kinds, per-property enum options indexed by name and value, text options, the default as the dump writes it and as the value new objects start with, the memory a default value and a default object take, the base class hint, and the class and property descriptions every ObjectProperty lookup answers from, each class knowing the catalog it belongs to.
 */

#ifndef AMBROSE_TYPEINFO_H
#define AMBROSE_TYPEINFO_H

#include "PropertyFlags.h"
#include "PropertyValue.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

enum class ClassKind : uint8
{
    PropertyClass,
    Enum,
    ValueType,
    Primitive,
    Container,
    Opaque
};

enum class ContainerKind : uint8
{
    Static,
    List,
    Vector
};

enum class ValueKind : uint8
{
    Bool,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Gid,
    Float,
    Double,
    WideChar,
    String,
    WideString,
    SignedBits,
    UnsignedBits,
    S24,
    U24,
    Enum,
    Object,
    Vector3D,
    Quaternion,
    Matrix3x3,
    Euler,
    Color,
    PointInt,
    PointFloat,
    SizeInt,
    RectInt,
    RectFloat,
    SerializedBuffer,
    SimpleVert,
    SimpleFace
};

struct EnumOption
{
    std::string Name;
    int64 Value = 0;
};

struct TextOption
{
    std::string Name;
    std::string Text;
};

struct ClassInfo;
class TypeCatalog;

struct PropertyInfo
{
    std::string Name;
    uint32 Hash = 0;
    uint32 Id = 0;
    uint32 Offset = 0;
    uint32 Flags = 0;
    ContainerKind Container = ContainerKind::Static;
    bool Dynamic = false;
    bool Singleton = false;
    bool Pointer = false;
    std::string TypeName;
    ValueKind Kind = ValueKind::Bool;
    uint8 BitWidth = 0;
    ClassInfo const* Type = nullptr;
    std::vector<EnumOption> Options;
    std::vector<uint32> OptionsByName;
    std::vector<uint32> OptionsByValue;
    std::vector<TextOption> TextOptions;
    std::optional<std::variant<int64, std::string>> Default;
    PropertyValue DefaultValue;
    std::size_t DefaultBytes = 0;
    std::string OptionBaseClass;

    bool HasFlag(PropertyFlag flag) const noexcept { return PropertyFlags::Has(Flags, flag); }
    std::optional<int64> FindOptionValue(std::string_view name) const noexcept;
    std::optional<std::string_view> FindOptionName(int64 value) const noexcept;
};

struct ClassInfo
{
    ClassInfo() = default;
    ClassInfo(ClassInfo const&) = delete;
    ClassInfo& operator=(ClassInfo const&) = delete;

    std::string Name;
    uint32 Hash = 0;
    ClassKind Kind = ClassKind::Opaque;
    TypeCatalog const* Owner = nullptr;
    std::size_t DefaultBytes = 0;
    std::vector<ClassInfo const*> Bases;
    std::vector<PropertyInfo> Properties;
    std::unordered_map<uint32, uint32> PropertyByHash;
    std::unordered_map<std::string_view, uint32> PropertyByName;

    bool IsA(ClassInfo const& other) const noexcept;
    PropertyInfo const* FindProperty(uint32 hash) const noexcept;
    PropertyInfo const* FindProperty(std::string_view name) const noexcept;
};

namespace TypeKinds
{
    std::string_view GetName(ClassKind kind) noexcept;
    std::string_view GetName(ValueKind kind) noexcept;
    std::string_view GetName(ContainerKind kind) noexcept;
}

#endif
