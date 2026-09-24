/*
 * Project Ambrose by Imjustchico
 * AES-256-GCM sealing of small secrets: a random 96-bit nonce, the ciphertext and a 128-bit tag in one buffer, bound to caller-supplied associated data.
 */

#ifndef AMBROSE_AES256GCM_H
#define AMBROSE_AES256GCM_H

#include "Types.h"

#include <array>
#include <optional>
#include <span>
#include <vector>

namespace AES256GCM
{
    inline constexpr std::size_t KeySize = 32;
    inline constexpr std::size_t NonceSize = 12;
    inline constexpr std::size_t TagSize = 16;

    using Key = std::array<uint8, KeySize>;
    using Nonce = std::array<uint8, NonceSize>;

    std::vector<uint8> Seal(Key const& key, std::span<uint8 const> plaintext, std::span<uint8 const> associatedData);
    std::vector<uint8> SealWithNonce(Key const& key, Nonce const& nonce, std::span<uint8 const> plaintext, std::span<uint8 const> associatedData);
    std::optional<std::vector<uint8>> Open(Key const& key, std::span<uint8 const> sealed, std::span<uint8 const> associatedData);
}

#endif
