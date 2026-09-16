/*
 * Project Ambrose by Imjustchico
 * Wiping secret bytes such as keys and verifiers in a way the optimizer cannot remove.
 */

#ifndef AMBROSE_SECUREMEMORY_H
#define AMBROSE_SECUREMEMORY_H

#include "Types.h"

#include <span>

namespace Ambrose::Crypto
{
    void SecureWipe(std::span<uint8> bytes) noexcept;
}

#endif
