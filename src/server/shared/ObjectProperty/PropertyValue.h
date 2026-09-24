/*
 * Project Ambrose by Imjustchico
 * One ObjectProperty value: any value kind the type dump uses, a list of values for List and Vector properties, or an owned child object that may be null, with deep copies, deep and exact equality, a child reachable only as const through a const value, and the compile-time index of each storage type.
 */

#ifndef AMBROSE_PROPERTYVALUE_H
#define AMBROSE_PROPERTYVALUE_H

#include "Types.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

class PropertyObject;

namespace PropertyTypes
{
    struct Vector3D
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;

        bool operator==(Vector3D const&) const = default;
    };

    struct Quaternion
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
        float W = 0.0f;

        bool operator==(Quaternion const&) const = default;
    };

    struct Matrix3x3
    {
        std::array<float, 9> Values{};

        bool operator==(Matrix3x3 const&) const = default;
    };

    struct Euler
    {
        float Pitch = 0.0f;
        float Yaw = 0.0f;
        float Roll = 0.0f;

        bool operator==(Euler const&) const = default;
    };

    struct Color
    {
        uint8 Red = 0;
        uint8 Green = 0;
        uint8 Blue = 0;
        uint8 Alpha = 0;

        bool operator==(Color const&) const = default;
    };

    struct PointInt
    {
        int32 X = 0;
        int32 Y = 0;

        bool operator==(PointInt const&) const = default;
    };

    struct PointFloat
    {
        float X = 0.0f;
        float Y = 0.0f;

        bool operator==(PointFloat const&) const = default;
    };

    struct SizeInt
    {
        int32 Width = 0;
        int32 Height = 0;

        bool operator==(SizeInt const&) const = default;
    };

    struct RectInt
    {
        int32 Left = 0;
        int32 Top = 0;
        int32 Right = 0;
        int32 Bottom = 0;

        bool operator==(RectInt const&) const = default;
    };

    struct RectFloat
    {
        float Left = 0.0f;
        float Top = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;

        bool operator==(RectFloat const&) const = default;
    };

    struct SerializedBuffer
    {
        std::vector<uint8> Bytes;

        bool operator==(SerializedBuffer const&) const = default;
    };

    struct SimpleVert
    {
        std::vector<uint8> Bytes;

        bool operator==(SimpleVert const&) const = default;
    };

    struct SimpleFace
    {
        std::vector<uint8> Bytes;

        bool operator==(SimpleFace const&) const = default;
    };
}

using PropertyObjectPtr = std::unique_ptr<PropertyObject>;

class PropertyValue
{
public:
    using List = std::vector<PropertyValue>;
    using Storage = std::variant<std::monostate, bool, int8, uint8, int16, uint16, int32, uint32, int64, uint64, float, double, char16_t, std::string, std::u16string,
        PropertyTypes::Vector3D, PropertyTypes::Quaternion, PropertyTypes::Matrix3x3, PropertyTypes::Euler, PropertyTypes::Color, PropertyTypes::PointInt, PropertyTypes::PointFloat,
        PropertyTypes::SizeInt, PropertyTypes::RectInt, PropertyTypes::RectFloat, PropertyTypes::SerializedBuffer, PropertyTypes::SimpleVert, PropertyTypes::SimpleFace, PropertyObjectPtr, List>;

    template<typename T>
    static constexpr std::size_t IndexOf() noexcept
    {
        return IndexIn<T, Storage>();
    }

    PropertyValue() noexcept;

    template<typename T>
        requires (!std::is_same_v<std::remove_cvref_t<T>, PropertyValue>) && std::is_constructible_v<Storage, T&&>
    PropertyValue(T&& value) : _value(std::forward<T>(value))
    {
    }

    PropertyValue(char const* text);
    PropertyValue(std::string_view text);
    PropertyValue(char16_t const* text);
    PropertyValue(PropertyValue const& other);
    PropertyValue(PropertyValue&& other) noexcept;
    PropertyValue& operator=(PropertyValue const& other);
    PropertyValue& operator=(PropertyValue&& other) noexcept;
    ~PropertyValue();

    std::size_t GetIndex() const noexcept { return _value.index(); }

    template<typename T>
    bool Holds() const noexcept { return std::holds_alternative<T>(_value); }

    template<typename T>
        requires (!std::is_same_v<T, PropertyObjectPtr>)
    T const* GetIf() const noexcept { return std::get_if<T>(&_value); }

    template<typename T>
    T* GetIf() noexcept { return std::get_if<T>(&_value); }

    PropertyObject const* AsObject() const noexcept;
    PropertyObject* AsObject() noexcept;
    List const* GetList() const noexcept { return GetIf<List>(); }
    bool IsNullObject() const noexcept;

    bool operator==(PropertyValue const& other) const;

private:
    template<typename T, typename Variant>
    static constexpr std::size_t IndexIn() noexcept;

    Storage _value;
};

template<typename T, typename Variant>
constexpr std::size_t PropertyValue::IndexIn() noexcept
{
    return []<typename... Types>(std::variant<Types...> const*) constexpr
    {
        std::size_t index = 0;
        for (bool const same : { std::is_same_v<T, Types>... })
        {
            if (same)
                return index;
            ++index;
        }
        return std::variant_npos;
    }(static_cast<Variant const*>(nullptr));
}

#endif
