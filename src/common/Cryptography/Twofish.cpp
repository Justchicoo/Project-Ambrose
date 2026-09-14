/*
 * Project Ambrose by Imjustchico
 * Drives Botan's Twofish block cipher behind a small interface that keeps Botan headers private.
 */

#include "Twofish.h"

#include <botan/block_cipher.h>

#include <stdexcept>

struct Twofish::Impl
{
    std::unique_ptr<Botan::BlockCipher> Cipher;
};

Twofish::Twofish(std::span<uint8 const> key) : _impl(std::make_unique<Impl>())
{
    if (!IsValidKeyLength(key.size()))
        throw std::invalid_argument("Twofish keys are 16, 24, or 32 bytes");
    _impl->Cipher = Botan::BlockCipher::create_or_throw("Twofish");
    _impl->Cipher->set_key(key.data(), key.size());
}

Twofish::~Twofish() = default;

Twofish::Twofish(Twofish&& other) noexcept = default;

Twofish& Twofish::operator=(Twofish&& other) noexcept = default;

Twofish::Impl const& Twofish::GetImpl() const
{
    if (!_impl)
        throw std::logic_error("Twofish used after it was moved from");
    return *_impl;
}

void Twofish::EncryptBlock(std::span<uint8 const, BlockSize> input, std::span<uint8, BlockSize> output) const
{
    GetImpl().Cipher->encrypt_n(input.data(), output.data(), 1);
}

void Twofish::DecryptBlock(std::span<uint8 const, BlockSize> input, std::span<uint8, BlockSize> output) const
{
    GetImpl().Cipher->decrypt_n(input.data(), output.data(), 1);
}

Twofish::Block Twofish::EncryptBlock(Block const& input) const
{
    Block output{};
    EncryptBlock(std::span<uint8 const, BlockSize>(input), std::span<uint8, BlockSize>(output));
    return output;
}

Twofish::Block Twofish::DecryptBlock(Block const& input) const
{
    Block output{};
    DecryptBlock(std::span<uint8 const, BlockSize>(input), std::span<uint8, BlockSize>(output));
    return output;
}

bool Twofish::IsValidKeyLength(std::size_t length) noexcept
{
    return length == 16 || length == 24 || length == 32;
}
