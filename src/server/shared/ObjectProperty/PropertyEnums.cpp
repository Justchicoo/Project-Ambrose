/*
 * Project Ambrose by Imjustchico
 * Accepts values from INT32_MIN to UINT32_MAX and keeps their unsigned 32-bit form, names an enum value by its option, names a Bits value by an exact option or else by every nonzero option, in dump order, whose bits it holds and no earlier chosen option covered, refusing values with unnamed bits, and parses names, '|'-joined names and integers back.
 */

#include "PropertyEnums.h"
#include "StringUtil.h"

#include <limits>

std::optional<int64> PropertyEnums::Normalize(int64 value) noexcept
{
    if (value < std::numeric_limits<int32>::min() || value > static_cast<int64>(std::numeric_limits<uint32>::max()))
        return std::nullopt;
    return value < 0 ? value + (int64{ 1 } << 32) : value;
}

std::optional<std::string> PropertyEnums::Format(PropertyInfo const& property, int64 value)
{
    std::optional<int64> const normalized = Normalize(value);
    if (!normalized)
        return std::nullopt;
    if (std::optional<std::string_view> const exact = property.FindOptionName(*normalized))
        return std::string(*exact);
    if (!property.HasFlag(PropertyFlag::Bits))
        return std::nullopt;
    if (*normalized == 0)
        return std::string();
    uint64 const bits = static_cast<uint64>(*normalized);
    uint64 covered = 0;
    std::string text;
    for (EnumOption const& option : property.Options)
    {
        uint64 const flag = static_cast<uint64>(option.Value);
        if (flag == 0 || (bits & flag) != flag || (covered & flag) != 0)
            continue;
        if (!text.empty())
            text += '|';
        text += option.Name;
        covered |= flag;
    }
    if (covered != bits)
        return std::nullopt;
    return text;
}

std::optional<int64> PropertyEnums::Parse(PropertyInfo const& property, std::string_view text)
{
    auto const single = [&property](std::string_view token) -> std::optional<int64>
    {
        std::string_view const trimmed = Ambrose::Trim(token);
        if (std::optional<int64> const value = property.FindOptionValue(trimmed))
            return value;
        std::optional<int64> const number = Ambrose::StringTo<int64>(trimmed);
        return number ? Normalize(*number) : std::nullopt;
    };
    if (!property.HasFlag(PropertyFlag::Bits))
        return single(text);
    if (Ambrose::Trim(text).empty())
        return 0;
    int64 combined = 0;
    for (std::string_view const token : Ambrose::Tokenize(text, '|', true))
    {
        std::optional<int64> const value = single(token);
        if (!value)
            return std::nullopt;
        combined |= *value;
    }
    return combined;
}
