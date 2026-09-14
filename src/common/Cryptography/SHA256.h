/*
 * Project Ambrose by Imjustchico
 * SHA-256 with incremental updates, a fixed 32-byte digest, and one-shot helpers.
 */

#ifndef AMBROSE_SHA256_H
#define AMBROSE_SHA256_H

#include "CryptoHash.h"

#include <array>

class SHA256
{
public:
    static constexpr std::size_t DigestLength = 32;
    using Digest = std::array<uint8, DigestLength>;

    SHA256();

    SHA256& Update(std::span<uint8 const> data);
    SHA256& Update(std::string_view data);

    template<typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    SHA256& UpdateInteger(T value)
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
