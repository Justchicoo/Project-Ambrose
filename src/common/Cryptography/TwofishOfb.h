/*
 * Project Ambrose by Imjustchico
 * Twofish in output feedback mode: a keystream cipher with no padding where encrypting and decrypting are the same operation.
 */

#ifndef AMBROSE_TWOFISHOFB_H
#define AMBROSE_TWOFISHOFB_H

#include "Twofish.h"

#include <span>
#include <vector>

class TwofishOfb
{
public:
    TwofishOfb(std::span<uint8 const> key, std::span<uint8 const, Twofish::BlockSize> iv);

    void Apply(std::span<uint8> data);
    void Apply(std::span<uint8 const> input, std::span<uint8> output);
    std::vector<uint8> Process(std::span<uint8 const> data);
    void Restart(std::span<uint8 const, Twofish::BlockSize> iv);

private:
    Twofish _cipher;
    Twofish::Block _feedback{};
    std::size_t _used = Twofish::BlockSize;
};

#endif
