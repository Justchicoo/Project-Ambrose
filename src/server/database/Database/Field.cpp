/*
 * Project Ambrose by Imjustchico
 * Reads text-protocol values and binary-protocol native numbers, checking the column type and the target range and reporting every mismatch with the column name.
 */

#include "Field.h"
#include "Log.h"
#include "StringUtil.h"

#include <algorithm>
#include <cmath>
#include <cstring>

void Field::SetText(char const* data, std::size_t length, FieldMetadata const* metadata) noexcept
{
    _data = data;
    _length = length;
    _metadata = metadata;
    _binary = false;
}

void Field::SetBinary(char const* data, std::size_t length, FieldMetadata const* metadata) noexcept
{
    _data = data;
    _length = length;
    _metadata = metadata;
    _binary = true;
}

bool Field::IsNativeNumber() const noexcept
{
    if (!_binary || !_metadata || !_data)
        return false;
    switch (_metadata->Type)
    {
        case DatabaseFieldType::Int8: return _length == 1;
        case DatabaseFieldType::Int16: return _length == 2;
        case DatabaseFieldType::Int32: return _length == 4;
        case DatabaseFieldType::Int64: return _length == 8;
        case DatabaseFieldType::Float: return _length == 4;
        case DatabaseFieldType::Double: return _length == 8;
        default: return false;
    }
}

std::string Field::GetString() const
{
    if (!IsNativeNumber())
        return std::string(GetStringView());
    return DescribeValue();
}

std::string Field::DescribeValue() const
{
    if (IsNull())
        return "NULL";
    if (!IsNativeNumber())
        return std::string(GetStringView());
    if (_metadata->Type == DatabaseFieldType::Float)
    {
        float value = 0;
        std::memcpy(&value, _data, sizeof(value));
        return fmt::format("{}", value);
    }
    if (_metadata->Type == DatabaseFieldType::Double)
    {
        double value = 0;
        std::memcpy(&value, _data, sizeof(value));
        return fmt::format("{}", value);
    }
    IntegerValue const value = ReadInteger();
    return value.Negative ? fmt::format("{}", value.Signed) : fmt::format("{}", value.Unsigned);
}

std::string_view Field::GetStringView() const noexcept
{
    return _data && !IsNativeNumber() ? std::string_view(_data, _length) : std::string_view();
}

std::string_view Field::GetCheckedStringView() const
{
    if (IsNativeNumber())
    {
        ReportMismatch("string_view", "a binary-protocol number has no text to view; read it as std::string");
        return {};
    }
    return GetStringView();
}

std::vector<uint8> Field::GetBinary() const
{
    if (IsNativeNumber())
    {
        std::string const text = DescribeValue();
        return std::vector<uint8>(text.begin(), text.end());
    }
    std::span<uint8 const> const bytes = GetBinaryView();
    return std::vector<uint8>(bytes.begin(), bytes.end());
}

std::span<uint8 const> Field::GetBinaryView() const noexcept
{
    return _data && !IsNativeNumber() ? std::span<uint8 const>(reinterpret_cast<uint8 const*>(_data), _length) : std::span<uint8 const>();
}

bool Field::IsRealColumn() const noexcept
{
    return _metadata && (_metadata->Type == DatabaseFieldType::Float || _metadata->Type == DatabaseFieldType::Double);
}

Field::IntegerValue Field::FromReal(double real) noexcept
{
    IntegerValue result;
    if (!std::isfinite(real))
        return result;
    result.Valid = true;
    double const whole = std::trunc(real);
    result.Truncated = whole != real;
    result.Negative = whole < 0;
    if (whole < -9223372036854775808.0)
    {
        result.Overflow = true;
        result.Signed = std::numeric_limits<int64>::min();
        return result;
    }
    if (whole >= 18446744073709551616.0)
    {
        result.Overflow = true;
        result.Signed = std::numeric_limits<int64>::max();
        result.Unsigned = std::numeric_limits<uint64>::max();
        return result;
    }
    if (result.Negative)
    {
        result.Signed = static_cast<int64>(whole);
        result.Unsigned = static_cast<uint64>(result.Signed);
    }
    else
    {
        result.Unsigned = static_cast<uint64>(whole);
        result.Signed = whole >= 9223372036854775808.0 ? std::numeric_limits<int64>::max() : static_cast<int64>(whole);
    }
    return result;
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
    if (IsNativeNumber())
    {
        if (_metadata->Type == DatabaseFieldType::Float)
        {
            float real = 0;
            std::memcpy(&real, _data, sizeof(real));
            return FromReal(real);
        }
        if (_metadata->Type == DatabaseFieldType::Double)
        {
            double real = 0;
            std::memcpy(&real, _data, sizeof(real));
            return FromReal(real);
        }
        bool const isUnsigned = _metadata->Unsigned;
        auto store = [&result, isUnsigned](int64 signedValue, uint64 unsignedValue)
        {
            result.Valid = true;
            if (isUnsigned)
            {
                result.Unsigned = unsignedValue;
                result.Signed = unsignedValue > static_cast<uint64>(std::numeric_limits<int64>::max()) ? std::numeric_limits<int64>::max() : static_cast<int64>(unsignedValue);
            }
            else
            {
                result.Signed = signedValue;
                result.Negative = signedValue < 0;
                result.Unsigned = static_cast<uint64>(signedValue);
            }
        };
        switch (_length)
        {
            case 1: { uint8 raw = 0; std::memcpy(&raw, _data, 1); store(static_cast<int8>(raw), raw); break; }
            case 2: { uint16 raw = 0; std::memcpy(&raw, _data, 2); store(static_cast<int16>(raw), raw); break; }
            case 4: { uint32 raw = 0; std::memcpy(&raw, _data, 4); store(static_cast<int32>(raw), raw); break; }
            default: { uint64 raw = 0; std::memcpy(&raw, _data, 8); store(static_cast<int64>(raw), raw); break; }
        }
        return result;
    }
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

    if (IsRealColumn())
    {
        std::optional<double> const real = Ambrose::StringTo<double>(GetStringView());
        return real ? FromReal(*real) : result;
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
    if (!IsIntegerColumn() && !IsRealColumn())
        ReportMismatch(requested, "the column is not a numeric column");
    IntegerValue const value = ReadInteger();
    if (!value.Valid)
    {
        if (IsIntegerColumn() || IsRealColumn())
            ReportMismatch(requested, fmt::format("value '{}' is not an integer", DescribeValue()));
        return 0;
    }
    if (value.Truncated)
        ReportMismatch(requested, fmt::format("value {} loses its fraction", DescribeValue()));
    if (!value.Negative && value.Unsigned > static_cast<uint64>(std::numeric_limits<int64>::max()))
    {
        ReportMismatch(requested, fmt::format("value is above {}", maximum));
        return maximum;
    }
    if (value.Overflow)
    {
        ReportMismatch(requested, fmt::format("value {} is outside {}..{}", DescribeValue(), minimum, maximum));
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
    if (!IsIntegerColumn() && !IsRealColumn())
        ReportMismatch(requested, "the column is not a numeric column");
    IntegerValue const value = ReadInteger();
    if (!value.Valid)
    {
        if (IsIntegerColumn() || IsRealColumn())
            ReportMismatch(requested, fmt::format("value '{}' is not an integer", DescribeValue()));
        return minimum;
    }
    if (value.Truncated)
        ReportMismatch(requested, fmt::format("value {} loses its fraction", DescribeValue()));
    if (value.Negative && value.Signed != 0)
    {
        ReportMismatch(requested, fmt::format("value {} is negative", value.Signed));
        return minimum;
    }
    if (value.Overflow)
    {
        ReportMismatch(requested, fmt::format("value {} is above {}", DescribeValue(), maximum));
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
    if (IsNativeNumber())
    {
        if (_metadata->Type == DatabaseFieldType::Float)
        {
            float value = 0;
            std::memcpy(&value, _data, sizeof(value));
            return value;
        }
        if (_metadata->Type == DatabaseFieldType::Double)
        {
            double value = 0;
            std::memcpy(&value, _data, sizeof(value));
            return value;
        }
        IntegerValue const value = ReadInteger();
        return value.Negative ? static_cast<double>(value.Signed) : static_cast<double>(value.Unsigned);
    }
    std::optional<double> const value = Ambrose::StringTo<double>(GetStringView());
    if (!value)
    {
        if (!wrongColumn)
            ReportMismatch(requested, fmt::format("value '{}' is not a number", DescribeValue()));
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
