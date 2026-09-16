/*
 * Project Ambrose by Imjustchico
 * Converts enum and Bits property values between the dump's option names and one canonical integer form, the value's 32 bits read as unsigned, with a Bits value written as its options' names joined by '|'.
 */

#ifndef AMBROSE_PROPERTYENUMS_H
#define AMBROSE_PROPERTYENUMS_H

#include "TypeInfo.h"

#include <optional>
#include <string>
#include <string_view>

namespace PropertyEnums
{
    std::optional<int64> Normalize(int64 value) noexcept;
    std::optional<std::string> Format(PropertyInfo const& property, int64 value);
    std::optional<int64> Parse(PropertyInfo const& property, std::string_view text);
}

#endif
