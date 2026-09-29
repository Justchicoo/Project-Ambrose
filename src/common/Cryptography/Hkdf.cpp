/*
 * Project Ambrose by Imjustchico
 * Derives through Botan's HKDF(SHA-256), extract and expand in one call.
 */

#include "Hkdf.h"

#include <botan/kdf.h>

#include <memory>

std::vector<uint8> Hkdf::Derive(std::span<uint8 const> inputKey, std::span<uint8 const> salt, std::span<uint8 const> info, std::size_t length)
{
    std::unique_ptr<Botan::KDF> const kdf = Botan::KDF::create_or_throw("HKDF(SHA-256)");
    std::vector<uint8> key(length);
    kdf->derive_key(std::span<uint8>(key), inputKey, salt, info);
    return key;
}
