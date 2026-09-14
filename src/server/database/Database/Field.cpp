/*
 * Project Ambrose by Imjustchico
 * Parses text-protocol column values into numbers, checking the column type and the target range and reporting every mismatch with the column name.
 */

#include "Field.h"
#include "Log.h"
#include "StringUtil.h"

#include <algorithm>

void Field::SetText(char const* data, std::size_t length, FieldMetadata const* metadata) noexcept
{
    _data = data;
    _length = length;
    _metadata = metadata;
}

std::string_view Field::GetStringView() const noexcept
{
    return _data ? std::string_view(_data, _length) : std::string_view();
}

std::vector<uint8> Field::GetBinary() const
{
    std::span<uint8 const> const bytes = GetBinaryView();
    return std::vector<uint8>(bytes.begin(), bytes.end());
}

std::span<uint8 const> Field::GetBinaryView() const noexcept
{
    return _data ? std::span<uint8 const>(reinterpret_cast<uint8 const*>(_data), _length) : std::span<uint8 const>();
}

bool Field::IsIntegerColumn() const noexcept
{
    if (!_metadata)
        return true;
    switch (_metadata->Type)
    {
        case DatabaseFieldType::Int8:
        case DatabaseFieldType::Int16:
        case DatabaseFieldType::Int32:
        case DatabaseFieldType::Int64:
        case DatabaseFieldType::Bit:
        case DatabaseFieldType::Decimal:
        case DatabaseFieldType::Null:
            return true;
        default:
            return false;
    }
}

Field::IntegerValue Field::ReadInteger() const
{
    IntegerValue result;
    if (_metadata && _metadata->Type == DatabaseFieldType::Bit)
    {
        std::span<uint8 const> const bytes = GetBinaryView();
        result.Valid = true;
        for (uint8 const byte : bytes)
        {
            if (result.Unsigned >> 56)
                result.Overflow = true;
            result.Unsigned = (result.Unsigned << 8) | byte;
        }
        result.Signed = static_cast<int64>(result.Unsigned);
        return result;
    }

    std::string_view text = GetStringView();
    if (_metadata && _metadata->Type == DatabaseFieldType::Decimal)
    {
        std::size_t const point = text.find('.');
        if (point != std::string_view::npos)
        {
            std::string_view const fraction = text.substr(point + 1);
            result.Truncated = fraction.find_first_not_of('0') != std::string_view::npos;
            text = text.substr(0, point);
            if (text.empty() || text == "-" || text == "+")
                text = "0";
        }
    }
    result.Negative = !text.empty() && text.front() == '-';
    if (std::optional<int64> const value = Ambrose::StringTo<int64>(text))
    {
        result.Valid = true;
        result.Signed = *value;
        result.Unsigned = static_cast<uint64>(*value);
        return result;
    }
    if (std::optional<uint64> const value = Ambrose::StringTo<uint64>(text))
    {
        result.Valid = true;
        result.Unsigned = *value;
        result.Signed = std::numeric_limits<int64>::max();
        return result;
    }
    bool const digits = !text.empty() && text.find_first_not_of("+-0123456789") == std::string_view::npos && text.find_first_of("0123456789") != std::string_view::npos;
    if (digits)
    {
        result.Valid = true;
        result.Overflow = true;
        result.Signed = result.Negative ? std::numeric_limits<int64>::min() : std::numeric_limits<int64>::max();
        result.Unsigned = result.Negative ? 0 : std::numeric_limits<uint64>::max();
    }
    return result;
}

int64 Field::GetInt64(std::string_view requested, int64 minimum, int64 maximum) const
{
    if (IsNull())
        return 0;
    if (!IsIntegerColumn())
        ReportMismatch(requested, "the column is not an integer column");
    IntegerValue const value = ReadInteger();
    if (!value.Valid)
    {
        if (IsIntegerColumn())
            ReportMismatch(requested, fmt::format("value '{}' is not an integer", GetStringView()));
        return 0;
    }
    if (value.Truncated)
        ReportMismatch(requested, fmt::format("value {} loses its fraction", GetStringView()));
    if (!value.Negative && value.Unsigned > static_cast<uint64>(std::numeric_limits<int64>::max()))
    {
        ReportMismatch(requested, fmt::format("value is above {}", maximum));
        return maximum;
    }
    if (value.Overflow)
    {
        ReportMismatch(requested, fmt::format("value {} is outside {}..{}", GetStringView(), minimum, maximum));
        return value.Negative ? minimum : maximum;
    }
    if (value.Signed < minimum || value.Signed > maximum)
    {
        ReportMismatch(requested, fmt::format("value {} is outside {}..{}", value.Signed, minimum, maximum));
        return std::clamp(value.Signed, minimum, maximum);
    }
    return value.Signed;
}

uint64 Field::GetUInt64(std::string_view requested, uint64 minimum, uint64 maximum) const
{
    if (IsNull())
        return 0;
    if (!IsIntegerColumn())
        ReportMismatch(requested, "the column is not an integer column");
    IntegerValue const value = ReadInteger();
    if (!value.Valid)
    {
        if (IsIntegerColumn())
            ReportMismatch(requested, fmt::format("value '{}' is not an integer", GetStringView()));
        return minimum;
    }
    if (value.Truncated)
        ReportMismatch(requested, fmt::format("value {} loses its fraction", GetStringView()));
    if (value.Negative && value.Signed != 0)
    {
        ReportMismatch(requested, fmt::format("value {} is negative", value.Signed));
        return minimum;
    }
    if (value.Overflow)
    {
        ReportMismatch(requested, fmt::format("value {} is above {}", GetStringView(), maximum));
        return maximum;
    }
    if (value.Unsigned < minimum || value.Unsigned > maximum)
    {
        ReportMismatch(requested, fmt::format("value {} is outside {}..{}", value.Unsigned, minimum, maximum));
        return std::clamp(value.Unsigned, minimum, maximum);
    }
    return value.Unsigned;
}

double Field::GetDouble(std::string_view requested) const
{
    if (IsNull())
        return 0.0;
    bool wrongColumn = false;
    if (_metadata)
    {
        switch (_metadata->Type)
        {
            case DatabaseFieldType::Float:
            case DatabaseFieldType::Double:
            case DatabaseFieldType::Decimal:
                break;
            case DatabaseFieldType::Bit:
                return static_cast<double>(ReadInteger().Unsigned);
            default:
                wrongColumn = !IsIntegerColumn();
                break;
        }
    }
    if (wrongColumn)
        ReportMismatch(requested, "the column is not a numeric column");
    std::optional<double> const value = Ambrose::StringTo<double>(GetStringView());
    if (!value)
    {
        if (!wrongColumn)
            ReportMismatch(requested, fmt::format("value '{}' is not a number", GetStringView()));
        return 0.0;
    }
    return *value;
}

void Field::ReportMismatch(std::string_view requested, std::string_view problem) const
{
    std::string_view const name = !_metadata ? std::string_view("?") : !_metadata->Alias.empty() ? std::string_view(_metadata->Alias) : std::string_view(_metadata->Name);
    std::string_view const type = _metadata ? std::string_view(_metadata->TypeName) : std::string_view("unknown");
    LOG_ERROR("sql.sql", "Field {} of type {} read as {}: {}", name, type, requested, problem);
}
