/*
 * Project Ambrose by Imjustchico
 * HKDF over SHA-256 (RFC 5869), which turns one master key into as many separate keys as there are purposes, each named by its info string, so a key used to seal one kind of secret is never the key that hashes another.
 */

#ifndef AMBROSE_HKDF_H
#define AMBROSE_HKDF_H

#include "Types.h"

#include <cstddef>
#include <span>
#include <vector>

namespace Hkdf
{
    std::vector<uint8> Derive(std::span<uint8 const> inputKey, std::span<uint8 const> salt, std::span<uint8 const> info, std::size_t length);
}

#endif
