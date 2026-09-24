/*
 * Project Ambrose by Imjustchico
 * Turns integer and text defaults into values of the property's kind: numbers written past a signed type's range wrap to the same bits, bit fields keep their low bits, enum and flag names resolve through the property's options, integer defaults on text properties are ignored, and lists, objects and value types take no default.
 */

#include "PropertyDefaults.h"
#include "PropertyEnums.h"
#include "PropertyObject.h"
#include "StringUtil.h"
#include "Utf.h"

#include <fmt/format.h>

#include <cmath>
#include <limits>
#include <string_view>
#include <type_traits>
#include <variant>

namespace
{
    std::string Shown(PropertyInfo const& property)
    {
        if (int64 const* const number = std::get_if<int64>(&*property.Default))
            return std::to_string(*number);
        return fmt::format("'{}'", std::get<std::string>(*property.Default));
    }

    std::optional<int64> Integer(PropertyInfo const& property)
    {
        if (int64 const* const number = std::get_if<int64>(&*property.Default))
            return *number;
        std::string_view const text = Ambrose::Trim(std::get<std::string>(*property.Default));
        if (std::optional<int64> const number = Ambrose::StringTo<int64>(text))
            return number;
        return PropertyEnums::Parse(property, text);
    }

    template<typename T>
    std::optional<T> Fit(int64 value) noexcept
    {
        using Unsigned = std::make_unsigned_t<T>;
        if constexpr (sizeof(T) == sizeof(int64))
            return static_cast<T>(static_cast<uint64>(value));
        else if (value < static_cast<int64>(std::numeric_limits<std::make_signed_t<T>>::min()) || value > static_cast<int64>(std::numeric_limits<Unsigned>::max()))
            return std::nullopt;
        else
            return static_cast<T>(static_cast<Unsigned>(value));
    }

    template<typename T>
    std::optional<PropertyValue> Number(PropertyInfo const& property)
    {
        if (!property.Default)
            return PropertyValue(T{});
        std::optional<int64> const value = Integer(property);
        if (!value)
            return std::nullopt;
        std::optional<T> const fitted = Fit<T>(*value);
        return fitted ? std::optional<PropertyValue>(PropertyValue(*fitted)) : std::nullopt;
    }

    std::optional<uint64> BitField(PropertyInfo const& property, uint8 width)
    {
        if (!property.Default)
            return 0;
        std::optional<int64> const value = Integer(property);
        if (!value || *value < -(int64{ 1 } << (width - 1)) || *value >= (int64{ 1 } << width))
            return std::nullopt;
        return static_cast<uint64>(*value) & ((uint64{ 1 } << width) - 1);
    }

    std::optional<double> Real(PropertyInfo const& property)
    {
        if (!property.Default)
            return 0.0;
        if (int64 const* const number = std::get_if<int64>(&*property.Default))
            return static_cast<double>(*number);
        return Ambrose::StringTo<double>(Ambrose::Trim(std::get<std::string>(*property.Default)));
    }

    std::string const* Text(PropertyInfo const& property)
    {
        return property.Default ? std::get_if<std::string>(&*property.Default) : nullptr;
    }
}

std::optional<PropertyValue> PropertyDefaults::Resolve(PropertyInfo const& property, std::string& problem)
{
    auto const valueType = [&property, &problem](std::string_view what, PropertyValue zero) -> std::optional<PropertyValue>
    {
        if (!property.Default)
            return zero;
        problem = fmt::format("has the default {}, but {} takes none", Shown(property), what);
        return std::nullopt;
    };
    if (property.Container != ContainerKind::Static)
        return valueType("a list property", PropertyValue());
    if (property.Kind == ValueKind::Object)
        return valueType("an object property", PropertyValue());

    std::optional<PropertyValue> resolved;
    switch (property.Kind)
    {
        case ValueKind::Bool:
            if (!property.Default)
                resolved = PropertyValue(false);
            else if (int64 const* const number = std::get_if<int64>(&*property.Default))
                resolved = PropertyValue(*number != 0);
            else if (std::optional<bool> const flag = Ambrose::StringTo<bool>(Ambrose::Trim(*Text(property))))
                resolved = PropertyValue(*flag);
            break;
        case ValueKind::Int8: resolved = Number<int8>(property); break;
        case ValueKind::UInt8: resolved = Number<uint8>(property); break;
        case ValueKind::Int16: resolved = Number<int16>(property); break;
        case ValueKind::UInt16: resolved = Number<uint16>(property); break;
        case ValueKind::Int32: resolved = Number<int32>(property); break;
        case ValueKind::UInt32: resolved = Number<uint32>(property); break;
        case ValueKind::Int64: resolved = Number<int64>(property); break;
        case ValueKind::UInt64:
        case ValueKind::Gid: resolved = Number<uint64>(property); break;
        case ValueKind::WideChar: resolved = Number<char16_t>(property); break;
        case ValueKind::Float:
            if (std::optional<double> const real = Real(property); real && std::abs(*real) <= std::numeric_limits<float>::max())
                resolved = PropertyValue(static_cast<float>(*real));
            break;
        case ValueKind::Double:
            if (std::optional<double> const real = Real(property))
                resolved = PropertyValue(*real);
            break;
        case ValueKind::String:
            resolved = PropertyValue(Text(property) ? *Text(property) : std::string());
            break;
        case ValueKind::WideString:
            if (!Text(property))
                resolved = PropertyValue(std::u16string());
            else if (std::optional<std::u16string> wide = Utf::Utf8ToUtf16(*Text(property), Utf::InvalidPolicy::Reject))
                resolved = PropertyValue(std::move(*wide));
            break;
        case ValueKind::SignedBits:
        case ValueKind::S24:
        {
            uint8 const width = property.Kind == ValueKind::S24 ? uint8{ 24 } : property.BitWidth;
            if (std::optional<uint64> const bits = BitField(property, width))
            {
                uint64 const sign = uint64{ 1 } << (width - 1);
                resolved = PropertyValue(static_cast<int32>(static_cast<int64>((*bits ^ sign) - sign)));
            }
            break;
        }
        case ValueKind::UnsignedBits:
        case ValueKind::U24:
            if (std::optional<uint64> const bits = BitField(property, property.Kind == ValueKind::U24 ? uint8{ 24 } : property.BitWidth))
                resolved = PropertyValue(static_cast<uint32>(*bits));
            break;
        case ValueKind::Enum:
            if (!property.Default)
                resolved = PropertyValue(int64{ 0 });
            else if (std::optional<int64> const value = std::holds_alternative<int64>(*property.Default)
                    ? PropertyEnums::Normalize(std::get<int64>(*property.Default)) : PropertyEnums::Parse(property, std::get<std::string>(*property.Default)))
                resolved = PropertyValue(*value);
            break;
        case ValueKind::Object:
            break;
        case ValueKind::Vector3D: return valueType("a Vector3D property", PropertyTypes::Vector3D{});
        case ValueKind::Quaternion: return valueType("a Quaternion property", PropertyTypes::Quaternion{});
        case ValueKind::Matrix3x3: return valueType("a Matrix3x3 property", PropertyTypes::Matrix3x3{});
        case ValueKind::Euler: return valueType("an Euler property", PropertyTypes::Euler{});
        case ValueKind::Color: return valueType("a Color property", PropertyTypes::Color{});
        case ValueKind::PointInt: return valueType("a Point<int> property", PropertyTypes::PointInt{});
        case ValueKind::PointFloat: return valueType("a Point<float> property", PropertyTypes::PointFloat{});
        case ValueKind::SizeInt: return valueType("a Size<int> property", PropertyTypes::SizeInt{});
        case ValueKind::RectInt: return valueType("a Rect<int> property", PropertyTypes::RectInt{});
        case ValueKind::RectFloat: return valueType("a Rect<float> property", PropertyTypes::RectFloat{});
        case ValueKind::SerializedBuffer: return valueType("a SerializedBuffer property", PropertyTypes::SerializedBuffer{});
        case ValueKind::SimpleVert: return valueType("a SimpleVert property", PropertyTypes::SimpleVert{});
        case ValueKind::SimpleFace: return valueType("a SimpleFace property", PropertyTypes::SimpleFace{});
    }
    if (!resolved)
        problem = fmt::format("has the default {}, which does not resolve to a value of type {}", Shown(property), property.TypeName);
    return resolved;
}
