/*
 * Project Ambrose by Imjustchico
 * Looks up enum options in both directions by binary search over their name and value orders, properties by hash or name, and whether a class is or derives from another, and names every kind for logs.
 */

#include "TypeInfo.h"

#include <algorithm>

std::optional<int64> PropertyInfo::FindOptionValue(std::string_view name) const noexcept
{
    auto const found = std::lower_bound(OptionsByName.begin(), OptionsByName.end(), name, [this](uint32 index, std::string_view wanted) { return Options[index].Name < wanted; });
    if (found == OptionsByName.end() || Options[*found].Name != name)
        return std::nullopt;
    return Options[*found].Value;
}

std::optional<std::string_view> PropertyInfo::FindOptionName(int64 value) const noexcept
{
    auto const found = std::lower_bound(OptionsByValue.begin(), OptionsByValue.end(), value, [this](uint32 index, int64 wanted) { return Options[index].Value < wanted; });
    if (found == OptionsByValue.end() || Options[*found].Value != value)
        return std::nullopt;
    return std::string_view(Options[*found].Name);
}

bool ClassInfo::IsA(ClassInfo const& other) const noexcept
{
    return this == &other || std::find(Bases.begin(), Bases.end(), &other) != Bases.end();
}

PropertyInfo const* ClassInfo::FindProperty(uint32 hash) const noexcept
{
    auto const found = PropertyByHash.find(hash);
    return found == PropertyByHash.end() ? nullptr : &Properties[found->second];
}

PropertyInfo const* ClassInfo::FindProperty(std::string_view name) const noexcept
{
    auto const found = PropertyByName.find(name);
    return found == PropertyByName.end() ? nullptr : &Properties[found->second];
}

std::string_view TypeKinds::GetName(ClassKind kind) noexcept
{
    switch (kind)
    {
        case ClassKind::PropertyClass: return "property class";
        case ClassKind::Enum: return "enum";
        case ClassKind::ValueType: return "value type";
        case ClassKind::Primitive: return "primitive";
        case ClassKind::Container: return "container";
        case ClassKind::Opaque: return "opaque class";
    }
    return "unknown";
}

std::string_view TypeKinds::GetName(ValueKind kind) noexcept
{
    switch (kind)
    {
        case ValueKind::Bool: return "bool";
        case ValueKind::Int8: return "int8";
        case ValueKind::UInt8: return "uint8";
        case ValueKind::Int16: return "int16";
        case ValueKind::UInt16: return "uint16";
        case ValueKind::Int32: return "int32";
        case ValueKind::UInt32: return "uint32";
        case ValueKind::Int64: return "int64";
        case ValueKind::UInt64: return "uint64";
        case ValueKind::Gid: return "gid";
        case ValueKind::Float: return "float";
        case ValueKind::Double: return "double";
        case ValueKind::WideChar: return "wide char";
        case ValueKind::String: return "string";
        case ValueKind::WideString: return "wide string";
        case ValueKind::SignedBits: return "signed bits";
        case ValueKind::UnsignedBits: return "unsigned bits";
        case ValueKind::S24: return "s24";
        case ValueKind::U24: return "u24";
        case ValueKind::Enum: return "enum";
        case ValueKind::Object: return "object";
        case ValueKind::Vector3D: return "Vector3D";
        case ValueKind::Quaternion: return "Quaternion";
        case ValueKind::Matrix3x3: return "Matrix3x3";
        case ValueKind::Euler: return "Euler";
        case ValueKind::Color: return "Color";
        case ValueKind::PointInt: return "Point<int>";
        case ValueKind::PointFloat: return "Point<float>";
        case ValueKind::SizeInt: return "Size<int>";
        case ValueKind::RectInt: return "Rect<int>";
        case ValueKind::RectFloat: return "Rect<float>";
        case ValueKind::SerializedBuffer: return "SerializedBuffer";
        case ValueKind::SimpleVert: return "SimpleVert";
        case ValueKind::SimpleFace: return "SimpleFace";
    }
    return "unknown";
}

std::string_view TypeKinds::GetName(ContainerKind kind) noexcept
{
    switch (kind)
    {
        case ContainerKind::Static: return "Static";
        case ContainerKind::List: return "List";
        case ContainerKind::Vector: return "Vector";
    }
    return "unknown";
}
