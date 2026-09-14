/*
 * Project Ambrose by Imjustchico
 * One column value of a result row with typed getters that check the column type and range, logging mismatches to sql.sql instead of aborting.
 */

#ifndef AMBROSE_FIELD_H
#define AMBROSE_FIELD_H

#include "Types.h"

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

enum class DatabaseFieldType : uint8
{
    Null,
    Int8,
    Int16,
    Int32,
    Int64,
    Float,
    Double,
    Decimal,
    Date,
    Bit,
    Binary
};

struct FieldMetadata
{
    std::string TableName;
    std::string TableAlias;
    std::string Name;
    std::string Alias;
    std::string TypeName;
    DatabaseFieldType Type = DatabaseFieldType::Null;
    bool Unsigned = false;
    uint32 Index = 0;
};

class Field
{
public:
    Field() = default;

    void SetText(char const* data, std::size_t length, FieldMetadata const* metadata) noexcept;

    bool IsNull() const noexcept { return _data == nullptr; }
    FieldMetadata const* GetMetadata() const noexcept { return _metadata; }

    template<typename T>
    T Get() const
    {
        if constexpr (std::is_same_v<T, bool>)
            return GetUInt64("bool", 0, 1) != 0;
        else if constexpr (std::is_same_v<T, std::string>)
            return std::string(GetStringView());
        else if constexpr (std::is_same_v<T, std::string_view>)
            return GetStringView();
        else if constexpr (std::is_same_v<T, std::vector<uint8>>)
            return GetBinary();
        else if constexpr (std::is_floating_point_v<T>)
            return static_cast<T>(GetDouble(std::is_same_v<T, float> ? "float" : "double"));
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
            return static_cast<T>(GetInt64(TypeName<T>(), static_cast<int64>(std::numeric_limits<T>::min()), static_cast<int64>(std::numeric_limits<T>::max())));
        else if constexpr (std::is_integral_v<T>)
            return static_cast<T>(GetUInt64(TypeName<T>(), 0, static_cast<uint64>(std::numeric_limits<T>::max())));
        else
            return T::FieldTypeIsNotSupported;
    }

    std::string_view GetStringView() const noexcept;
    std::vector<uint8> GetBinary() const;
    std::span<uint8 const> GetBinaryView() const noexcept;

private:
    template<typename T>
    static constexpr std::string_view TypeName() noexcept
    {
        switch (sizeof(T))
        {
            case 1: return std::is_signed_v<T> ? "int8" : "uint8";
            case 2: return std::is_signed_v<T> ? "int16" : "uint16";
            case 4: return std::is_signed_v<T> ? "int32" : "uint32";
            default: return std::is_signed_v<T> ? "int64" : "uint64";
        }
    }

    struct IntegerValue
    {
        bool Valid = false;
        bool Negative = false;
        bool Truncated = false;
        bool Overflow = false;
        int64 Signed = 0;
        uint64 Unsigned = 0;
    };

    IntegerValue ReadInteger() const;
    int64 GetInt64(std::string_view requested, int64 minimum, int64 maximum) const;
    uint64 GetUInt64(std::string_view requested, uint64 minimum, uint64 maximum) const;
    double GetDouble(std::string_view requested) const;
    bool IsIntegerColumn() const noexcept;
    void ReportMismatch(std::string_view requested, std::string_view problem) const;

    char const* _data = nullptr;
    std::size_t _length = 0;
    FieldMetadata const* _metadata = nullptr;
};

#endif
