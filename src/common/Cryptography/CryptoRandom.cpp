/*
 * Project Ambrose by Imjustchico
 * Reads the operating system generator through Botan's System_RNG and draws unbiased bounded integers.
 */

#include "CryptoRandom.h"

#include <botan/system_rng.h>

#include <stdexcept>

void Ambrose::Crypto::GetRandomBytes(std::span<uint8> output)
{
    if (!output.empty())
        Botan::system_rng().randomize(output.data(), output.size());
}

std::vector<uint8> Ambrose::Crypto::GetRandomBytes(std::size_t count)
{
    std::vector<uint8> bytes(count);
    GetRandomBytes(bytes);
    return bytes;
}

uint32 Ambrose::Crypto::GetRandomUInt32()
{
    std::array<uint8, 4> const bytes = GetRandomArray<4>();
    return uint32{ bytes[0] } | (uint32{ bytes[1] } << 8) | (uint32{ bytes[2] } << 16) | (uint32{ bytes[3] } << 24);
}

uint64 Ambrose::Crypto::GetRandomUInt64()
{
    return (uint64{ GetRandomUInt32() } << 32) | GetRandomUInt32();
}

uint32 Ambrose::Crypto::GetRandomBelow(uint32 upperExclusive)
{
    if (upperExclusive == 0)
        throw std::invalid_argument("GetRandomBelow needs a positive upper bound");
    uint32 const threshold = (0u - upperExclusive) % upperExclusive;
    while (true)
    {
        uint32 const value = GetRandomUInt32();
        if (value >= threshold)
            return value % upperExclusive;
    }
}
