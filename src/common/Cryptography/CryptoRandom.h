/*
 * Project Ambrose by Imjustchico
 * Cryptographically secure random bytes and integers from the operating system generator.
 */

#ifndef AMBROSE_CRYPTORANDOM_H
#define AMBROSE_CRYPTORANDOM_H

#include "Types.h"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace Ambrose::Crypto
{
    void GetRandomBytes(std::span<uint8> output);
    std::vector<uint8> GetRandomBytes(std::size_t count);
    uint32 GetRandomUInt32();
    uint64 GetRandomUInt64();
    uint32 GetRandomBelow(uint32 upperExclusive);

    template<std::size_t Length>
    std::array<uint8, Length> GetRandomArray()
    {
        std::array<uint8, Length> bytes{};
        GetRandomBytes(bytes);
        return bytes;
    }
}

#endif
