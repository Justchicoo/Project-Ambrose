/*
 * Project Ambrose by Imjustchico
 * Packs bits into bytes least significant bit first, overwriting existing bits when back-patching.
 */

#include "BitWriter.h"

#include <algorithm>
#include <stdexcept>

void BitWriter::WriteBit(bool value)
{
    WriteBits(value ? 1u : 0u, 1);
}

void BitWriter::WriteBits(uint64 value, uint8 count)
{
    if (count > 64)
        throw std::invalid_argument("BitWriter::WriteBits supports at most 64 bits");
    std::size_t const lastByte = (_bitPosition + count + 7) / 8;
    if (_bytes.size() < lastByte)
        _bytes.resize(lastByte, 0);
    while (count > 0)
    {
        std::size_t const byteIndex = _bitPosition / 8;
        uint8 const bitOffset = static_cast<uint8>(_bitPosition % 8);
        uint8 const take = std::min<uint8>(static_cast<uint8>(8 - bitOffset), count);
        uint8 const fieldMask = static_cast<uint8>(((1u << take) - 1u) << bitOffset);
        uint8 const fieldBits = static_cast<uint8>((static_cast<uint32>(value) & ((1u << take) - 1u)) << bitOffset);
        _bytes[byteIndex] = static_cast<uint8>((_bytes[byteIndex] & ~fieldMask) | fieldBits);
        value >>= take;
        count = static_cast<uint8>(count - take);
        _bitPosition += take;
    }
    _bitEnd = std::max(_bitEnd, _bitPosition);
}

void BitWriter::WriteSignedBits(int64 value, uint8 count)
{
    WriteBits(static_cast<uint64>(value), count);
}

void BitWriter::WriteBytes(std::span<uint8 const> bytes)
{
    Realign();
    for (uint8 byte : bytes)
        WriteBits(byte, 8);
}

void BitWriter::Realign()
{
    _bitPosition = (_bitPosition + 7) / 8 * 8;
    if (_bytes.size() < _bitPosition / 8)
        _bytes.resize(_bitPosition / 8, 0);
    _bitEnd = std::max(_bitEnd, _bitPosition);
}

void BitWriter::SeekBit(std::size_t bitPosition)
{
    if (bitPosition > _bitEnd)
        throw std::out_of_range("BitWriter::SeekBit past the written end");
    _bitPosition = bitPosition;
}
