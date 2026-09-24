/*
 * Project Ambrose by Imjustchico
 * Holds, checks, encodes, decodes, and prints the field values of a message whose layout comes from the registry.
 */

#include "DynamicMessage.h"
#include "Utf.h"

#include <fmt/format.h>

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace
{
    void AppendEscaped(std::string& out, std::string_view text, bool escapeNonAscii)
    {
        for (char const character : text)
        {
            unsigned char const byte = static_cast<unsigned char>(character);
            if (character == '"' || character == '\\')
            {
                out += '\\';
                out += character;
            }
            else if (byte < 0x20 || byte == 0x7F || (escapeNonAscii && byte >= 0x80))
                out += fmt::format("\\x{:02x}", byte);
            else
                out += character;
        }
    }
}

DynamicMessage::DynamicMessage(MessageCatalogPtr catalog, MessageInfo const& info) : _catalog(std::move(catalog)), _info(&info)
{
    if (!_catalog || !_catalog->Contains(info))
        throw std::invalid_argument("a dynamic message needs the catalog its message info belongs to");
    _values = info.Defaults;
}

std::optional<DynamicMessage> DynamicMessage::Create(MessageCatalogPtr catalog, uint8 serviceId, uint8 order)
{
    MessageInfo const* const info = catalog ? catalog->Find(serviceId, order) : nullptr;
    if (!info)
        return std::nullopt;
    return DynamicMessage(std::move(catalog), *info);
}

DmlValue const* DynamicMessage::Find(std::string_view field) const noexcept
{
    std::vector<FieldDef> const& fields = _info->Definition->Fields;
    auto const it = std::find_if(fields.begin(), fields.end(), [field](FieldDef const& candidate) { return candidate.Name == field; });
    return it == fields.end() ? nullptr : &_values[static_cast<std::size_t>(it - fields.begin())];
}

bool DynamicMessage::Set(std::size_t index, DmlValue value)
{
    std::vector<FieldDef> const& fields = _info->Definition->Fields;
    if (index >= fields.size() || !Holds(value, fields[index].Type))
        return false;
    if (std::string const* const text = std::get_if<std::string>(&value); text && text->size() > Dml::MaxStringLength)
        return false;
    if (std::u16string const* const text = std::get_if<std::u16string>(&value); text && text->size() > Dml::MaxStringLength)
        return false;
    _values[index] = std::move(value);
    return true;
}

bool DynamicMessage::Set(std::string_view field, DmlValue value)
{
    std::vector<FieldDef> const& fields = _info->Definition->Fields;
    auto const it = std::find_if(fields.begin(), fields.end(), [field](FieldDef const& candidate) { return candidate.Name == field; });
    return it != fields.end() && Set(static_cast<std::size_t>(it - fields.begin()), std::move(value));
}

void DynamicMessage::Reset()
{
    _values = _info->Defaults;
}

bool DynamicMessage::Holds(DmlValue const& value, DmlType type) noexcept
{
    switch (type)
    {
        case DmlType::Byt: return std::holds_alternative<int8>(value);
        case DmlType::Ubyt: return std::holds_alternative<uint8>(value);
        case DmlType::Shrt: return std::holds_alternative<int16>(value);
        case DmlType::Ushrt: return std::holds_alternative<uint16>(value);
        case DmlType::Int: return std::holds_alternative<int32>(value);
        case DmlType::Uint: return std::holds_alternative<uint32>(value);
        case DmlType::Flt: return std::holds_alternative<float>(value);
        case DmlType::Dbl: return std::holds_alternative<double>(value);
        case DmlType::Gid: return std::holds_alternative<uint64>(value);
        case DmlType::Str: return std::holds_alternative<std::string>(value);
        case DmlType::Wstr: return std::holds_alternative<std::u16string>(value);
    }
    return false;
}

std::size_t DynamicMessage::GetEncodedSize(DmlValue const& value) noexcept
{
    return std::visit([](auto const& held) -> std::size_t
    {
        using T = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<T, std::string>)
            return 2 + held.size();
        else if constexpr (std::is_same_v<T, std::u16string>)
            return 2 + 2 * held.size();
        else
            return sizeof(T);
    }, value);
}

std::size_t DynamicMessage::GetEncodedSize() const noexcept
{
    std::size_t size = 0;
    for (DmlValue const& value : _values)
        size += GetEncodedSize(value);
    return size;
}

void DynamicMessage::Encode(ByteBuffer& buffer) const
{
    std::vector<FieldDef> const& fields = _info->Definition->Fields;
    for (std::size_t i = 0; i < fields.size(); ++i)
        Dml::WriteValue(buffer, fields[i].Type, _values[i]);
}

bool DynamicMessage::Decode(ByteBuffer& buffer)
{
    std::vector<FieldDef> const& fields = _info->Definition->Fields;
    std::size_t const start = buffer.GetReadPosition();
    std::vector<DmlValue> values;
    values.reserve(fields.size());
    try
    {
        for (FieldDef const& field : fields)
            values.push_back(Dml::ReadValue(buffer, field.Type));
    }
    catch (ByteBufferException const&)
    {
        buffer.SetReadPosition(start);
        return false;
    }
    _values = std::move(values);
    return true;
}

MessageDecodeStatus DynamicMessage::Decode(std::span<uint8 const> body)
{
    ByteBuffer buffer(body);
    if (!Decode(buffer))
        return MessageDecodeStatus::Truncated;
    return buffer.GetRemaining() == 0 ? MessageDecodeStatus::Ok : MessageDecodeStatus::TrailingBytes;
}

std::string DynamicMessage::FormatValue(DmlValue const& value, std::size_t maxStringLength)
{
    return std::visit([maxStringLength](auto const& held) -> std::string
    {
        using T = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<T, std::string>)
        {
            std::string out = "\"";
            AppendEscaped(out, std::string_view(held).substr(0, maxStringLength), true);
            out += '"';
            if (held.size() > maxStringLength)
                out += fmt::format("...({} bytes)", held.size());
            return out;
        }
        else if constexpr (std::is_same_v<T, std::u16string>)
        {
            std::u16string_view shown = std::u16string_view(held).substr(0, maxStringLength);
            if (shown.size() < held.size() && !shown.empty() && shown.back() >= 0xD800 && shown.back() <= 0xDBFF && held[shown.size()] >= 0xDC00 && held[shown.size()] <= 0xDFFF)
                shown.remove_suffix(1);
            std::string out = "u\"";
            AppendEscaped(out, Utf::Utf16ToUtf8(shown, Utf::InvalidPolicy::ReplaceWithU_FFFD).value_or(std::string()), false);
            out += '"';
            if (held.size() > maxStringLength)
                out += fmt::format("...({} units)", held.size());
            return out;
        }
        else if constexpr (std::is_same_v<T, uint64>)
            return fmt::format("0x{:016x}", held);
        else if constexpr (std::is_same_v<T, int8> || std::is_same_v<T, uint8>)
            return fmt::format("{}", static_cast<int32>(held));
        else
            return fmt::format("{}", held);
    }, value);
}

std::string DynamicMessage::ToString(std::size_t maxStringLength) const
{
    MessageDef const& definition = *_info->Definition;
    std::string out = fmt::format("{} ({}:{}) {{", definition.Tag, _info->Protocol->ServiceId, definition.Order);
    for (std::size_t i = 0; i < definition.Fields.size(); ++i)
        out += fmt::format("{} {}={}", i == 0 ? "" : ",", definition.Fields[i].Name, FormatValue(_values[i], maxStringLength));
    out += definition.Fields.empty() ? "}" : " }";
    return out;
}
