/*
 * Project Ambrose by Imjustchico
 * A message of any loaded id held as one DML value per field, with type-checked access, encode and decode by layout, and a readable dump for logs.
 */

#ifndef AMBROSE_DYNAMICMESSAGE_H
#define AMBROSE_DYNAMICMESSAGE_H

#include "MessageRegistry.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class DynamicMessage
{
public:
    static constexpr std::size_t DefaultMaxDumpStringLength = 64;

    explicit DynamicMessage(MessageInfo const& info);

    MessageInfo const& GetInfo() const noexcept { return *_info; }
    MessageDef const& GetDefinition() const noexcept { return *_info->Definition; }
    std::vector<DmlValue> const& GetValues() const noexcept { return _values; }

    DmlValue const* Find(std::string_view field) const noexcept;
    bool Set(std::size_t index, DmlValue value);
    bool Set(std::string_view field, DmlValue value);
    void Reset();

    std::size_t GetEncodedSize() const noexcept;
    void Encode(ByteBuffer& buffer) const;
    bool Decode(ByteBuffer& buffer);
    MessageDecodeStatus Decode(std::span<uint8 const> body);

    std::string ToString(std::size_t maxStringLength = DefaultMaxDumpStringLength) const;

    static bool Holds(DmlValue const& value, DmlType type) noexcept;
    static std::size_t GetEncodedSize(DmlValue const& value) noexcept;
    static std::string FormatValue(DmlValue const& value, std::size_t maxStringLength = DefaultMaxDumpStringLength);

private:
    MessageInfo const* _info;
    std::vector<DmlValue> _values;
};

#endif
