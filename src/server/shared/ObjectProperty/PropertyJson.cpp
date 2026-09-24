/*
 * Project Ambrose by Imjustchico
 * Turns a property object into ordered JSON: its class under $class, then each property in id order, with enums and flag integers as option names when they have them, text that is not UTF-8 repaired with replacement characters, wide text as UTF-8, NaN and infinities as the strings NaN, Infinity and -Infinity, math and color types as arrays of their fields, lists as arrays and child objects nested.
 */

#include "PropertyJson.h"
#include "PropertyEnums.h"
#include "Utf.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <string>
#include <type_traits>

namespace
{
    using Json = nlohmann::ordered_json;

    template<typename T>
    Json Number(T value)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            if (!std::isfinite(value))
                return Json(std::isnan(value) ? "NaN" : value > 0 ? "Infinity" : "-Infinity");
            return Json(value);
        }
        else
        {
            return Json(value);
        }
    }

    template<typename T>
    Json Scalar(PropertyValue const& value)
    {
        T const* const scalar = value.GetIf<T>();
        return scalar ? Number(*scalar) : Json();
    }

    Json Text(std::string const& text)
    {
        if (Utf::IsValidUtf8(text))
            return Json(text);
        std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::ReplaceWithU_FFFD);
        return Json(wide ? Utf::Utf16ToUtf8(*wide, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string()) : std::string());
    }

    template<typename T, typename... Members>
    Json Tuple(PropertyValue const& value, Members T::*... members)
    {
        T const* const tuple = value.GetIf<T>();
        if (!tuple)
            return Json();
        return Json::array({ Number(tuple->*members)... });
    }

    Json Wide(std::u16string_view text)
    {
        return Json(Utf::Utf16ToUtf8(text, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string()));
    }

    Json Named(PropertyInfo const& property, int64 number)
    {
        std::optional<std::string> const name = PropertyEnums::Format(property, number);
        return name ? Json(*name) : Json(number);
    }

    bool HasNames(PropertyInfo const& property) noexcept
    {
        return !property.Options.empty() && (property.HasFlag(PropertyFlag::Bits) || property.HasFlag(PropertyFlag::Enum));
    }

    Json Element(PropertyInfo const& property, PropertyValue const& value)
    {
        switch (property.Kind)
        {
            case ValueKind::Bool: return Scalar<bool>(value);
            case ValueKind::Int8: return Scalar<int8>(value);
            case ValueKind::UInt8: return Scalar<uint8>(value);
            case ValueKind::Int16: return Scalar<int16>(value);
            case ValueKind::UInt16: return Scalar<uint16>(value);
            case ValueKind::Int32:
            {
                int32 const* const number = value.GetIf<int32>();
                if (!number)
                    return Json();
                return HasNames(property) ? Named(property, *number) : Json(*number);
            }
            case ValueKind::UInt32:
            {
                uint32 const* const number = value.GetIf<uint32>();
                if (!number)
                    return Json();
                return HasNames(property) ? Named(property, *number) : Json(*number);
            }
            case ValueKind::Int64: return Scalar<int64>(value);
            case ValueKind::UInt64:
            case ValueKind::Gid: return Scalar<uint64>(value);
            case ValueKind::Float: return Scalar<float>(value);
            case ValueKind::Double: return Scalar<double>(value);
            case ValueKind::WideChar:
            {
                char16_t const* const character = value.GetIf<char16_t>();
                return character ? Wide(std::u16string_view(character, 1)) : Json();
            }
            case ValueKind::String:
            {
                std::string const* const text = value.GetIf<std::string>();
                return text ? Text(*text) : Json();
            }
            case ValueKind::WideString:
            {
                std::u16string const* const text = value.GetIf<std::u16string>();
                return text ? Wide(*text) : Json();
            }
            case ValueKind::SignedBits:
            case ValueKind::S24: return Scalar<int32>(value);
            case ValueKind::UnsignedBits:
            case ValueKind::U24: return Scalar<uint32>(value);
            case ValueKind::Enum:
            {
                int64 const* const number = value.GetIf<int64>();
                return number ? Named(property, *number) : Json();
            }
            case ValueKind::Object: return PropertyJson::ToJson(value.AsObject());
            case ValueKind::Vector3D: return Tuple<PropertyTypes::Vector3D>(value, &PropertyTypes::Vector3D::X, &PropertyTypes::Vector3D::Y, &PropertyTypes::Vector3D::Z);
            case ValueKind::Quaternion: return Tuple<PropertyTypes::Quaternion>(value, &PropertyTypes::Quaternion::X, &PropertyTypes::Quaternion::Y, &PropertyTypes::Quaternion::Z, &PropertyTypes::Quaternion::W);
            case ValueKind::Matrix3x3:
            {
                PropertyTypes::Matrix3x3 const* const matrix = value.GetIf<PropertyTypes::Matrix3x3>();
                if (!matrix)
                    return Json();
                Json values = Json::array();
                for (float field : matrix->Values)
                    values.push_back(Number(field));
                return values;
            }
            case ValueKind::Euler: return Tuple<PropertyTypes::Euler>(value, &PropertyTypes::Euler::Pitch, &PropertyTypes::Euler::Yaw, &PropertyTypes::Euler::Roll);
            case ValueKind::Color: return Tuple<PropertyTypes::Color>(value, &PropertyTypes::Color::Red, &PropertyTypes::Color::Green, &PropertyTypes::Color::Blue, &PropertyTypes::Color::Alpha);
            case ValueKind::PointInt: return Tuple<PropertyTypes::PointInt>(value, &PropertyTypes::PointInt::X, &PropertyTypes::PointInt::Y);
            case ValueKind::PointFloat: return Tuple<PropertyTypes::PointFloat>(value, &PropertyTypes::PointFloat::X, &PropertyTypes::PointFloat::Y);
            case ValueKind::SizeInt: return Tuple<PropertyTypes::SizeInt>(value, &PropertyTypes::SizeInt::Width, &PropertyTypes::SizeInt::Height);
            case ValueKind::RectInt: return Tuple<PropertyTypes::RectInt>(value, &PropertyTypes::RectInt::Left, &PropertyTypes::RectInt::Top, &PropertyTypes::RectInt::Right, &PropertyTypes::RectInt::Bottom);
            case ValueKind::RectFloat: return Tuple<PropertyTypes::RectFloat>(value, &PropertyTypes::RectFloat::Left, &PropertyTypes::RectFloat::Top, &PropertyTypes::RectFloat::Right, &PropertyTypes::RectFloat::Bottom);
            case ValueKind::SerializedBuffer:
            case ValueKind::SimpleVert:
            case ValueKind::SimpleFace: return Json();
        }
        return Json();
    }
}

nlohmann::ordered_json PropertyJson::ToJson(PropertyObject const* object)
{
    if (!object)
        return Json();
    ClassInfo const& type = object->GetClass();
    Json json = Json::object();
    json[std::string(ClassKey)] = type.Name;
    for (std::size_t ordinal = 0; ordinal < type.Properties.size(); ++ordinal)
    {
        PropertyInfo const& property = type.Properties[ordinal];
        PropertyValue const& value = *object->GetAt(ordinal);
        if (property.Container == ContainerKind::Static)
        {
            json[property.Name] = Element(property, value);
            continue;
        }
        Json list = Json::array();
        if (PropertyValue::List const* const elements = value.GetList())
            for (PropertyValue const& element : *elements)
                list.push_back(Element(property, element));
        json[property.Name] = std::move(list);
    }
    return json;
}

std::string PropertyJson::Dump(PropertyObject const* object, int indent)
{
    return ToJson(object).dump(indent, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
}
