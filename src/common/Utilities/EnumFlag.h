/*
 * Project Ambrose by Imjustchico
 * Type-safe bit flag operations for enums that opt in with DEFINE_ENUM_FLAG.
 */

#ifndef AMBROSE_ENUMFLAG_H
#define AMBROSE_ENUMFLAG_H

#include <type_traits>

template<typename T>
struct EnumTraitsFlag
{
    static constexpr bool IsFlag = false;
};

#define DEFINE_ENUM_FLAG(enumType) template<> struct EnumTraitsFlag<enumType> { static constexpr bool IsFlag = true; }

template<typename T>
concept FlagEnum = std::is_enum_v<T> && EnumTraitsFlag<T>::IsFlag;

template<FlagEnum T>
constexpr T operator|(T left, T right)
{
    return static_cast<T>(static_cast<std::underlying_type_t<T>>(left) | static_cast<std::underlying_type_t<T>>(right));
}

template<FlagEnum T>
constexpr T operator&(T left, T right)
{
    return static_cast<T>(static_cast<std::underlying_type_t<T>>(left) & static_cast<std::underlying_type_t<T>>(right));
}

template<FlagEnum T>
constexpr T operator~(T value)
{
    return static_cast<T>(~static_cast<std::underlying_type_t<T>>(value));
}

template<FlagEnum T>
constexpr T& operator|=(T& left, T right)
{
    return left = left | right;
}

template<FlagEnum T>
constexpr T& operator&=(T& left, T right)
{
    return left = left & right;
}

template<FlagEnum T>
class EnumFlag
{
public:
    using UnderlyingType = std::underlying_type_t<T>;

    constexpr EnumFlag() = default;

    constexpr EnumFlag(T value) : _value(value) { }

    constexpr bool HasFlag(T flag) const
    {
        return static_cast<UnderlyingType>(_value & flag) != 0;
    }

    constexpr bool HasAllFlags(T flags) const
    {
        return (_value & flags) == flags;
    }

    constexpr void SetFlag(T flag)
    {
        _value |= flag;
    }

    constexpr void RemoveFlag(T flag)
    {
        _value &= ~flag;
    }

    constexpr T AsEnum() const
    {
        return _value;
    }

    constexpr UnderlyingType AsUnderlyingType() const
    {
        return static_cast<UnderlyingType>(_value);
    }

private:
    T _value{};
};

#endif
