/*
 * Project Ambrose by Imjustchico
 * Assembles x86-64 bytes at fixed addresses for tests on hand-built PE images: raw bytes, RIP-relative displacements, lea, direct calls and indirect branches through a memory slot, over a buffer filled with int3.
 */

#ifndef AMBROSE_CODEBUFFER_H
#define AMBROSE_CODEBUFFER_H

#include "Types.h"

#include <cstddef>
#include <initializer_list>
#include <vector>

class CodeBuffer
{
public:
    CodeBuffer(uint32 rva, std::size_t size) : _rva(rva), _bytes(size, 0xCC)
    {
    }

    void Put(uint32 at, std::initializer_list<uint8> bytes)
    {
        std::size_t offset = at - _rva;
        for (uint8 const byte : bytes)
            _bytes.at(offset++) = byte;
    }

    void Displacement(uint32 at, uint32 next, uint64 target)
    {
        uint32 const value = static_cast<uint32>(target - next);
        Put(at, { static_cast<uint8>(value), static_cast<uint8>(value >> 8), static_cast<uint8>(value >> 16), static_cast<uint8>(value >> 24) });
    }

    void Lea(uint32 at, uint8 rex, uint8 modrm, uint32 target)
    {
        Put(at, { rex, 0x8D, modrm });
        Displacement(at + 3, at + 7, target);
    }

    void Call(uint32 at, uint32 target)
    {
        Put(at, { 0xE8 });
        Displacement(at + 1, at + 5, target);
    }

    void Indirect(uint32 at, uint8 modrm, uint64 slot)
    {
        Put(at, { 0xFF, modrm });
        Displacement(at + 2, at + 6, slot);
    }

    std::vector<uint8> const& Bytes() const noexcept { return _bytes; }

private:
    uint32 _rva;
    std::vector<uint8> _bytes;
};

#endif
