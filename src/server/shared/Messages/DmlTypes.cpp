/*
 * Project Ambrose by Imjustchico
 * Parses DML type names and encodes each field type, checking length prefixes before allocating.
 */

#include "DmlTypes.h"

#include <fmt/format.h>

#include <array>
#include <stdexcept>

namespace
{
    struct TypeEntry
    {
        std::string_view Name;
        DmlType Type;
        bool IsAlias;
    };

    constexpr std::array<TypeEntry, 14> TypeTable{ {
        { "BYT", DmlType::Byt, false },
        { "UBYT", DmlType::Ubyt, false },
        { "SHRT", DmlType::Shrt, false },
        { "USHRT", DmlType::Ushrt, false },
        { "INT", DmlType::Int, false },
        { "UINT", DmlType::Uint, false },
        { "FLT", DmlType::Flt, false },
        { "DBL", DmlType::Dbl, false },
        { "GID", DmlType::Gid, false },
        { "STR", DmlType::Str, false },
        { "WSTR", DmlType::Wstr, false },
        { "UBYTE", DmlType::Ubyt, true },
        { "USHORT", DmlType::Ushrt, true },
        { "BOOL", DmlType::Ubyt, true },
    } };

    template<typename T>
    T const& Expect(DmlValue const& value, DmlType type)
    {
        if (T const* typed = std::get_if<T>(&value))
            return *typed;
        throw std::invalid_argument(fmt::format("DML value does not hold the C++ type for {}", Dml::GetTypeName(type)));
    }
}

std::optional<DmlTypeName> Dml::ParseType(std::string_view name)
{
    for (TypeEntry const& entry : TypeTable)
        if (entry.Name == name)
            return DmlTypeName{ entry.Type, entry.IsAlias };
    return std::nullopt;
}

std::string_view Dml::GetTypeName(DmlType type)
{
    for (TypeEntry const& entry : TypeTable)
        if (entry.Type == type && !entry.IsAlias)
            return entry.Name;
    return "UNKNOWN";
}

std::size_t Dml::GetFixedSize(DmlType type)
{
    switch (type)
    {
        case DmlType::Byt:
        case DmlType::Ubyt:
            return 1;
        case DmlType::Shrt:
        case DmlType::Ushrt:
            return 2;
        case DmlType::Int:
        case DmlType::Uint:
        case DmlType::Flt:
            return 4;
        case DmlType::Dbl:
        case DmlType::Gid:
            return 8;
        case DmlType::Str:
        case DmlType::Wstr:
            return 0;
    }
    return 0;
}

void Dml::WriteStr(ByteBuffer& buffer, std::string_view bytes)
{
    if (bytes.size() > MaxStringLength)
        throw std::length_error(fmt::format("STR of {} bytes exceeds the {} byte limit", bytes.size(), MaxStringLength));
    buffer.Write(static_cast<uint16>(bytes.size()));
    buffer.WriteBytes(std::span<uint8 const>(reinterpret_cast<uint8 const*>(bytes.data()), bytes.size()));
}

std::string Dml::ReadStr(ByteBuffer& buffer)
{
    uint16 const length = buffer.Read<uint16>();
    std::span<uint8 const> const bytes = buffer.ReadBytes(length);
    return std::string(reinterpret_cast<char const*>(bytes.data()), bytes.size());
}

void Dml::WriteWstr(ByteBuffer& buffer, std::u16string_view text)
{
    if (text.size() > MaxStringLength)
        throw std::length_error(fmt::format("WSTR of {} code units exceeds the {} unit limit", text.size(), MaxStringLength));
    buffer.Write(static_cast<uint16>(text.size()));
    for (char16_t unit : text)
        buffer.Write(static_cast<uint16>(unit));
}

std::u16string Dml::ReadWstr(ByteBuffer& buffer)
{
    uint16 const count = buffer.Read<uint16>();
    buffer.EnsureReadable(std::size_t(count) * 2);
    std::u16string text;
    text.reserve(count);
    for (uint16 i = 0; i < count; ++i)
        text.push_back(static_cast<char16_t>(buffer.Read<uint16>()));
    return text;
}

DmlValue Dml::DefaultValue(DmlType type)
{
    switch (type)
    {
        case DmlType::Byt: return int8(0);
        case DmlType::Ubyt: return uint8(0);
        case DmlType::Shrt: return int16(0);
        case DmlType::Ushrt: return uint16(0);
        case DmlType::Int: return int32(0);
        case DmlType::Uint: return uint32(0);
        case DmlType::Flt: return 0.0f;
        case DmlType::Dbl: return 0.0;
        case DmlType::Gid: return uint64(0);
        case DmlType::Str: return std::string();
        case DmlType::Wstr: return std::u16string();
    }
    return uint8(0);
}

DmlValue Dml::ReadValue(ByteBuffer& buffer, DmlType type)
{
    switch (type)
    {
        case DmlType::Byt: return buffer.Read<int8>();
        case DmlType::Ubyt: return buffer.Read<uint8>();
        case DmlType::Shrt: return buffer.Read<int16>();
        case DmlType::Ushrt: return buffer.Read<uint16>();
        case DmlType::Int: return buffer.Read<int32>();
        case DmlType::Uint: return buffer.Read<uint32>();
        case DmlType::Flt: return buffer.Read<float>();
        case DmlType::Dbl: return buffer.Read<double>();
        case DmlType::Gid: return buffer.Read<uint64>();
        case DmlType::Str: return ReadStr(buffer);
        case DmlType::Wstr: return ReadWstr(buffer);
    }
    throw std::invalid_argument("unknown DML type");
}

void Dml::WriteValue(ByteBuffer& buffer, DmlType type, DmlValue const& value)
{
    switch (type)
    {
        case DmlType::Byt: buffer.Write(Expect<int8>(value, type)); return;
        case DmlType::Ubyt: buffer.Write(Expect<uint8>(value, type)); return;
        case DmlType::Shrt: buffer.Write(Expect<int16>(value, type)); return;
        case DmlType::Ushrt: buffer.Write(Expect<uint16>(value, type)); return;
        case DmlType::Int: buffer.Write(Expect<int32>(value, type)); return;
        case DmlType::Uint: buffer.Write(Expect<uint32>(value, type)); return;
        case DmlType::Flt: buffer.Write(Expect<float>(value, type)); return;
        case DmlType::Dbl: buffer.Write(Expect<double>(value, type)); return;
        case DmlType::Gid: buffer.Write(Expect<uint64>(value, type)); return;
        case DmlType::Str: WriteStr(buffer, Expect<std::string>(value, type)); return;
        case DmlType::Wstr: WriteWstr(buffer, Expect<std::u16string>(value, type)); return;
    }
    throw std::invalid_argument("unknown DML type");
}
