/*
 * Project Ambrose by Imjustchico
 * Twofish block cipher with 128, 192, or 256-bit keys, encrypting and decrypting 16-byte blocks.
 */

#ifndef AMBROSE_TWOFISH_H
#define AMBROSE_TWOFISH_H

#include "Types.h"

#include <array>
#include <cstddef>
#include <memory>
#include <span>

class Twofish
{
public:
    static constexpr std::size_t BlockSize = 16;
    using Block = std::array<uint8, BlockSize>;

    explicit Twofish(std::span<uint8 const> key);
    ~Twofish();

    Twofish(Twofish&& other) noexcept;
    Twofish& operator=(Twofish&& other) noexcept;
    Twofish(Twofish const&) = delete;
    Twofish& operator=(Twofish const&) = delete;

    void EncryptBlock(std::span<uint8 const, BlockSize> input, std::span<uint8, BlockSize> output) const;
    void DecryptBlock(std::span<uint8 const, BlockSize> input, std::span<uint8, BlockSize> output) const;
    Block EncryptBlock(Block const& input) const;
    Block DecryptBlock(Block const& input) const;

    static bool IsValidKeyLength(std::size_t length) noexcept;

private:
    struct Impl;

    Impl const& GetImpl() const;

    std::unique_ptr<Impl> _impl;
};

#endif
