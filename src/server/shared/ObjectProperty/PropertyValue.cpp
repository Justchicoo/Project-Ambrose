/*
 * Project Ambrose by Imjustchico
 * Treats a null C string as empty, copies property values deeply, cloning child objects and list elements, and compares them deeply and exactly, floating values by bit pattern so a NaN equals the same NaN and 0.0 differs from -0.0, treating two null objects as equal.
 */

#include "PropertyValue.h"
#include "PropertyObject.h"

#include <cstring>

namespace
{
    template<typename T>
    constexpr bool IsFloatLayout = std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_same_v<T, PropertyTypes::Vector3D> || std::is_same_v<T, PropertyTypes::Quaternion>
        || std::is_same_v<T, PropertyTypes::Matrix3x3> || std::is_same_v<T, PropertyTypes::Euler> || std::is_same_v<T, PropertyTypes::PointFloat> || std::is_same_v<T, PropertyTypes::RectFloat>;

    static_assert(sizeof(PropertyTypes::Vector3D) == 3 * sizeof(float) && sizeof(PropertyTypes::Quaternion) == 4 * sizeof(float) && sizeof(PropertyTypes::Matrix3x3) == 9 * sizeof(float)
        && sizeof(PropertyTypes::Euler) == 3 * sizeof(float) && sizeof(PropertyTypes::PointFloat) == 2 * sizeof(float) && sizeof(PropertyTypes::RectFloat) == 4 * sizeof(float));

    PropertyValue::Storage Copy(PropertyValue::Storage const& source)
    {
        return std::visit([](auto const& value) -> PropertyValue::Storage
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, PropertyObjectPtr>)
                return value ? value->Clone() : PropertyObjectPtr();
            else
                return value;
        }, source);
    }
}

PropertyValue::PropertyValue() noexcept = default;

PropertyValue::PropertyValue(char const* text) : _value(text ? std::string(text) : std::string())
{
}

PropertyValue::PropertyValue(std::string_view text) : _value(std::string(text))
{
}

PropertyValue::PropertyValue(char16_t const* text) : _value(text ? std::u16string(text) : std::u16string())
{
}

PropertyValue::PropertyValue(PropertyValue const& other) : _value(Copy(other._value))
{
}

PropertyValue::PropertyValue(PropertyValue&& other) noexcept = default;

PropertyValue& PropertyValue::operator=(PropertyValue const& other)
{
    if (this != &other)
        _value = Copy(other._value);
    return *this;
}

PropertyValue& PropertyValue::operator=(PropertyValue&& other) noexcept = default;

PropertyValue::~PropertyValue() = default;

PropertyObject const* PropertyValue::AsObject() const noexcept
{
    PropertyObjectPtr const* const object = std::get_if<PropertyObjectPtr>(&_value);
    return object ? object->get() : nullptr;
}

PropertyObject* PropertyValue::AsObject() noexcept
{
    PropertyObjectPtr* const object = GetIf<PropertyObjectPtr>();
    return object ? object->get() : nullptr;
}

bool PropertyValue::IsNullObject() const noexcept
{
    PropertyObjectPtr const* const object = std::get_if<PropertyObjectPtr>(&_value);
    return object && !*object;
}

bool PropertyValue::operator==(PropertyValue const& other) const
{
    if (_value.index() != other._value.index())
        return false;
    return std::visit([&other](auto const& value) -> bool
    {
        using T = std::decay_t<decltype(value)>;
        T const& right = std::get<T>(other._value);
        if constexpr (std::is_same_v<T, PropertyObjectPtr>)
            return !value || !right ? !value && !right : *value == *right;
        else if constexpr (IsFloatLayout<T>)
            return std::memcmp(&value, &right, sizeof(T)) == 0;
        else
            return value == right;
    }, _value);
}
