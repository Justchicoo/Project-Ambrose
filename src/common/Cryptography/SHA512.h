/*
 * Project Ambrose by Imjustchico
 * SHA-512 with incremental updates, a fixed 64-byte digest, and one-shot helpers.
 */

#ifndef AMBROSE_SHA512_H
#define AMBROSE_SHA512_H

#include "CryptoHash.h"

#include <array>

class SHA512
{
public:
    static constexpr std::size_t DigestLength = 64;
    using Digest = std::array<uint8, DigestLength>;

    SHA512();

    SHA512& Update(std::span<uint8 const> data);
    SHA512& Update(std::string_view data);

    template<typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    SHA512& UpdateInteger(T value)
    {
        _hash.UpdateInteger(value);
        return *this;
    }

    Digest Finalize();
    void Reset();

    static Digest GetDigestOf(std::span<uint8 const> data);
    static Digest GetDigestOf(std::string_view data);

private:
    CryptoHash _hash;
};

#endif
