/*
 * Project Ambrose by Imjustchico
 * DML wire field types from the client message definitions and their bounds-checked encoding.
 */

#ifndef AMBROSE_DMLTYPES_H
#define AMBROSE_DMLTYPES_H

#include "ByteBuffer.h"
#include "Types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

enum class DmlType : uint8
{
    Byt,
    Ubyt,
    Shrt,
    Ushrt,
    Int,
    Uint,
    Flt,
    Dbl,
    Gid,
    Str,
    Wstr
};

struct DmlTypeName
{
    DmlType Type;
    bool IsAlias;
};

using DmlValue = std::variant<int8, uint8, int16, uint16, int32, uint32, float, double, uint64, std::string, std::u16string>;

namespace Dml
{
    inline constexpr std::size_t MaxStringLength = 0xFFFF;

    std::optional<DmlTypeName> ParseType(std::string_view name);
    std::string_view GetTypeName(DmlType type);
    std::size_t GetFixedSize(DmlType type);

    void WriteStr(ByteBuffer& buffer, std::string_view bytes);
    std::string ReadStr(ByteBuffer& buffer);
    void WriteWstr(ByteBuffer& buffer, std::u16string_view text);
    std::u16string ReadWstr(ByteBuffer& buffer);

    DmlValue DefaultValue(DmlType type);
    DmlValue ReadValue(ByteBuffer& buffer, DmlType type);
    void WriteValue(ByteBuffer& buffer, DmlType type, DmlValue const& value);
}

#endif
