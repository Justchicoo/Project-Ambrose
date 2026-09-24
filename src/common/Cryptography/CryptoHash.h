/*
 * Project Ambrose by Imjustchico
 * Incremental cryptographic hash over Botan with a selectable SHA-2 algorithm, used by the fixed-size SHA classes.
 */

#ifndef AMBROSE_CRYPTOHASH_H
#define AMBROSE_CRYPTOHASH_H

#include "Types.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>

namespace Botan
{
    class HashFunction;
}

class CryptoHash
{
public:
    enum class Algorithm : uint8
    {
        Sha256,
        Sha512
    };

    explicit CryptoHash(Algorithm algorithm);
    ~CryptoHash();

    CryptoHash(CryptoHash&& other) noexcept;
    CryptoHash& operator=(CryptoHash&& other) noexcept;
    CryptoHash(CryptoHash const&) = delete;
    CryptoHash& operator=(CryptoHash const&) = delete;

    void Update(std::span<uint8 const> data);
    void Update(std::string_view data);

    template<typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    void UpdateInteger(T value)
    {
        uint8 bytes[sizeof(T)];
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned bits = static_cast<Unsigned>(value);
        for (std::size_t i = 0; i < sizeof(T); ++i)
            bytes[i] = static_cast<uint8>(bits >> (i * 8));
        Update(std::span<uint8 const>(bytes, sizeof(T)));
    }

    void Finalize(std::span<uint8> digest);
    void Reset();
    std::size_t GetDigestLength() const noexcept;
    Algorithm GetAlgorithm() const noexcept;

    static std::size_t GetDigestLength(Algorithm algorithm) noexcept;
    static std::string_view GetBotanName(Algorithm algorithm) noexcept;

private:
    struct Impl;

    Botan::HashFunction& GetFunction();

    std::unique_ptr<Impl> _impl;
    Algorithm _algorithm;
};

#endif
