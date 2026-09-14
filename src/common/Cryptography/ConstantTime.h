/*
 * Project Ambrose by Imjustchico
 * Constant-time comparison of secret byte strings such as password verifiers and message tags.
 */

#ifndef AMBROSE_CONSTANTTIME_H
#define AMBROSE_CONSTANTTIME_H

#include "Types.h"

#include <span>

namespace Ambrose::Crypto
{
    bool ConstantTimeEquals(std::span<uint8 const> left, std::span<uint8 const> right) noexcept;
}

#endif
