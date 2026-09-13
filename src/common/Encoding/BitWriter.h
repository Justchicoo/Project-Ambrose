/*
 * Project Ambrose by Imjustchico
 * Writes LSB-first bit-packed streams with byte-aligned scalars and bit-position back-patching.
 */

#ifndef AMBROSE_BITWRITER_H
#define AMBROSE_BITWRITER_H

#include "Types.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

class BitWriter
{
public:
    void WriteBit(bool value);
    void WriteBits(uint64 value, uint8 count);
    void WriteSignedBits(int64 value, uint8 count);
    void WriteBytes(std::span<uint8 const> bytes);
    void Realign();
    void SeekBit(std::size_t bitPosition);

    template<typename T>
        requires (std::is_integral_v<T> || std::is_floating_point_v<T>) && (!std::is_same_v<T, bool>)
    void Write(T value)
    {
        Realign();
        uint8 bytes[sizeof(T)];
        std::memcpy(bytes, &value, sizeof(T));
        if constexpr (std::endian::native == std::endian::big)
            std::reverse(bytes, bytes + sizeof(T));
        for (uint8 byte : bytes)
            WriteBits(byte, 8);
    }

    std::size_t GetBitPosition() const { return _bitPosition; }
    std::size_t GetBitSize() const { return _bitEnd; }
    std::span<uint8 const> GetBytes() const { return _bytes; }

private:
    std::vector<uint8> _bytes;
    std::size_t _bitPosition = 0;
    std::size_t _bitEnd = 0;
};

#endif
