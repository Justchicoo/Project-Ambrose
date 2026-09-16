/*
 * Project Ambrose by Imjustchico
 * Extracts bits least significant bit first and latches failure instead of reading past the data or the current bit limit.
 */

#include "BitReader.h"

BitReader::BitReader(std::span<uint8 const> data) : _data(data), _limit(data.size() * 8)
{
}

bool BitReader::Ensure(std::size_t bits)
{
    if (_failed || bits > GetRemainingBits())
    {
        _failed = true;
        return false;
    }
    return true;
}

bool BitReader::ReadBit()
{
    return ReadBits(1) != 0;
}

uint64 BitReader::ReadBits(uint8 count)
{
    if (count > 64)
    {
        _failed = true;
        return 0;
    }
    if (!Ensure(count))
        return 0;
    uint64 value = 0;
    uint8 produced = 0;
    while (produced < count)
    {
        std::size_t const byteIndex = _bitPosition / 8;
        uint8 const bitOffset = static_cast<uint8>(_bitPosition % 8);
        uint8 const take = std::min<uint8>(static_cast<uint8>(8 - bitOffset), static_cast<uint8>(count - produced));
        uint64 const field = (static_cast<uint32>(_data[byteIndex]) >> bitOffset) & ((1u << take) - 1u);
        value |= field << produced;
        produced = static_cast<uint8>(produced + take);
        _bitPosition += take;
    }
    return value;
}

int64 BitReader::ReadSignedBits(uint8 count)
{
    uint64 const raw = ReadBits(count);
    if (_failed || count == 0 || count >= 64)
        return static_cast<int64>(raw);
    uint64 const signBit = uint64(1) << (count - 1);
    return static_cast<int64>((raw ^ signBit) - signBit);
}

std::span<uint8 const> BitReader::ReadBytes(std::size_t count)
{
    Realign();
    if (count > GetRemainingBits() / 8 || !Ensure(count * 8))
    {
        _failed = true;
        return {};
    }
    std::span<uint8 const> const view = _data.subspan(_bitPosition / 8, count);
    _bitPosition += count * 8;
    return view;
}

void BitReader::Realign()
{
    _bitPosition = (_bitPosition + 7) / 8 * 8;
}

void BitReader::SeekBit(std::size_t bitPosition)
{
    if (bitPosition > _limit)
    {
        _failed = true;
        return;
    }
    _bitPosition = bitPosition;
}

void BitReader::SetLimit(std::size_t bitLimit) noexcept
{
    _limit = std::min(bitLimit, GetBitSize());
}
