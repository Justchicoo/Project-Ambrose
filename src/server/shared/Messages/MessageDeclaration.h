/*
 * Project Ambrose by Imjustchico
 * Compile-time message declarations: C++ members bound to client message fields by name, with DML type compatibility and member codecs.
 */

#ifndef AMBROSE_MESSAGEDECLARATION_H
#define AMBROSE_MESSAGEDECLARATION_H

#include "ByteBuffer.h"
#include "DmlTypes.h"
#include "Types.h"

#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

template<typename Message, typename Member>
struct MessageField
{
    std::string_view Name;
    Member Message::* Pointer;
};

template<typename Message, typename Member>
constexpr MessageField<Message, Member> DmlField(std::string_view name, Member Message::* pointer) noexcept
{
    return MessageField<Message, Member>{ name, pointer };
}

namespace MessageMember
{
    template<typename T>
    concept FixedEnum = std::is_enum_v<T> && requires { T{ std::underlying_type_t<T>{} }; };

    template<typename T>
    concept PlainInteger = std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> && !std::is_same_v<T, wchar_t>
        && !std::is_same_v<T, char8_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

    template<typename T>
    struct Wire
    {
        using Type = T;
    };

    template<typename T>
        requires std::is_enum_v<T>
    struct Wire<T>
    {
        using Type = std::underlying_type_t<T>;
    };

    template<>
    struct Wire<bool>
    {
        using Type = uint8;
    };

    template<typename T>
    using WireType = typename Wire<T>::Type;

    template<typename W>
    constexpr bool IsInteger(std::size_t size, bool isSigned) noexcept
    {
        return PlainInteger<W> && sizeof(W) == size && std::is_signed_v<W> == isSigned;
    }

    template<typename T>
    constexpr bool IsCompatible(DmlType type) noexcept
    {
        using W = WireType<T>;
        switch (type)
        {
            case DmlType::Byt: return IsInteger<W>(1, true);
            case DmlType::Ubyt: return IsInteger<W>(1, false);
            case DmlType::Shrt: return IsInteger<W>(2, true);
            case DmlType::Ushrt: return IsInteger<W>(2, false);
            case DmlType::Int: return IsInteger<W>(4, true);
            case DmlType::Uint: return IsInteger<W>(4, false);
            case DmlType::Flt: return std::is_same_v<W, float>;
            case DmlType::Dbl: return std::is_same_v<W, double>;
            case DmlType::Gid: return IsInteger<W>(8, false);
            case DmlType::Str: return std::is_same_v<W, std::string>;
            case DmlType::Wstr: return std::is_same_v<W, std::u16string>;
        }
        return false;
    }

    template<typename T>
    constexpr bool IsSupported() noexcept
    {
        using W = WireType<T>;
        return !std::is_const_v<T> && !std::is_volatile_v<T> && (!std::is_enum_v<T> || FixedEnum<T>)
            && (PlainInteger<W> || std::is_same_v<W, float> || std::is_same_v<W, double> || std::is_same_v<W, std::string> || std::is_same_v<W, std::u16string>);
    }

    template<typename T>
    constexpr void CheckEncodable(T const&) noexcept
    {
    }

    inline void CheckEncodable(std::string const& value)
    {
        if (value.size() > Dml::MaxStringLength)
            throw std::length_error("a STR member of " + std::to_string(value.size()) + " bytes exceeds the 65535 byte limit");
    }

    inline void CheckEncodable(std::u16string const& value)
    {
        if (value.size() > Dml::MaxStringLength)
            throw std::length_error("a WSTR member of " + std::to_string(value.size()) + " code units exceeds the 65535 unit limit");
    }

    template<typename T>
    void Write(ByteBuffer& buffer, T const& value)
    {
        if constexpr (std::is_same_v<T, bool>)
            buffer.Write(static_cast<uint8>(value ? 1 : 0));
        else if constexpr (std::is_enum_v<T>)
            buffer.Write(static_cast<WireType<T>>(value));
        else
            buffer.Write(value);
    }

    inline void Write(ByteBuffer& buffer, std::string const& value)
    {
        Dml::WriteStr(buffer, value);
    }

    inline void Write(ByteBuffer& buffer, std::u16string const& value)
    {
        Dml::WriteWstr(buffer, value);
    }

    template<typename T>
    void Read(ByteBuffer& buffer, T& value)
    {
        if constexpr (std::is_same_v<T, bool>)
            value = buffer.Read<uint8>() != 0;
        else if constexpr (std::is_enum_v<T>)
            value = static_cast<T>(buffer.Read<WireType<T>>());
        else
            value = buffer.Read<T>();
    }

    inline void Read(ByteBuffer& buffer, std::string& value)
    {
        value = Dml::ReadStr(buffer);
    }

    inline void Read(ByteBuffer& buffer, std::u16string& value)
    {
        value = Dml::ReadWstr(buffer);
    }
}

template<typename T>
concept DeclaredMessage = std::is_class_v<T> && std::default_initializable<T> && requires {
    { T::ServiceId } -> std::convertible_to<uint8>;
    { T::Tag } -> std::convertible_to<std::string_view>;
    { T::Fields() };
};

template<typename Message, typename Fields>
struct IsMessageFieldTuple : std::false_type
{
};

template<typename Message, typename... Owners, typename... Members>
struct IsMessageFieldTuple<Message, std::tuple<MessageField<Owners, Members>...>>
    : std::bool_constant<((MessageMember::IsSupported<Members>() && (std::is_same_v<Owners, Message> || std::is_base_of_v<Owners, Message>)) && ...)>
{
};

namespace MessageDeclarationDetail
{
    std::size_t NextTypeIndex() noexcept;

    template<typename T>
    std::size_t TypeIndex() noexcept
    {
        static std::size_t const index = NextTypeIndex();
        return index;
    }

    template<typename Fields, typename Visitor>
    void VisitField(Fields const& fields, std::size_t index, Visitor&& visitor)
    {
        std::apply([&](auto const&... field)
        {
            std::size_t current = 0;
            ((current++ == index ? (visitor(field), void()) : void()), ...);
            (void)current;
        }, fields);
        (void)index;
    }

    template<typename Fields, typename Visitor>
    void ForEachField(Fields const& fields, Visitor&& visitor)
    {
        std::apply([&](auto const&... field)
        {
            std::size_t index = 0;
            ((visitor(index++, field)), ...);
            (void)index;
        }, fields);
        (void)visitor;
    }
}

#endif
