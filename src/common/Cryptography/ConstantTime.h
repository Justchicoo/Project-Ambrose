/*
 * Project Ambrose by Imjustchico
 * Constant-time comparison of secret byte strings and text such as password verifiers, login keys and message tags.
 */

#ifndef AMBROSE_CONSTANTTIME_H
#define AMBROSE_CONSTANTTIME_H

#include "Types.h"

#include <span>
#include <string_view>

namespace Ambrose::Crypto
{
    bool ConstantTimeEquals(std::span<uint8 const> left, std::span<uint8 const> right) noexcept;
    bool ConstantTimeEquals(std::string_view left, std::string_view right) noexcept;
}

#endif
