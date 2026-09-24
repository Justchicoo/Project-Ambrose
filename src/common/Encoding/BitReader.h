/*
 * Project Ambrose by Imjustchico
 * Reads LSB-first bit-packed streams without throwing, copying byte-aligned scalars straight from the buffer, within a movable bit limit that fences reads into a region of the data: an overrun sets a failed flag, which a caller that resynchronizes can clear, and yields zeros.
 */

#ifndef AMBROSE_BITREADER_H
#define AMBROSE_BITREADER_H

#include "Types.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>

class BitReader
{
public:
    explicit BitReader(std::span<uint8 const> data);

    bool ReadBit();
    uint64 ReadBits(uint8 count);
    int64 ReadSignedBits(uint8 count);
    std::span<uint8 const> ReadBytes(std::size_t count);
    void Realign();
    void SeekBit(std::size_t bitPosition);
    void SetLimit(std::size_t bitLimit) noexcept;
    void ClearFailure() noexcept { _failed = false; }

    template<typename T>
        requires (std::is_integral_v<T> || std::is_floating_point_v<T>) && (!std::is_same_v<T, bool>)
    T Read()
    {
        Realign();
        if (!Ensure(sizeof(T) * 8))
            return T{};
        uint8 bytes[sizeof(T)];
        std::memcpy(bytes, _data.data() + _bitPosition / 8, sizeof(T));
        _bitPosition += sizeof(T) * 8;
        if constexpr (std::endian::native == std::endian::big)
            std::reverse(bytes, bytes + sizeof(T));
        T value;
        std::memcpy(&value, bytes, sizeof(T));
        return value;
    }

    bool Failed() const { return _failed; }
    std::size_t GetBitPosition() const { return _bitPosition; }
    std::size_t GetBitSize() const { return _data.size() * 8; }
    std::size_t GetLimit() const { return _limit; }
    std::size_t GetRemainingBits() const { return _bitPosition < _limit ? _limit - _bitPosition : 0; }

private:
    bool Ensure(std::size_t bits);

    std::span<uint8 const> _data;
    std::size_t _bitPosition = 0;
    std::size_t _limit = 0;
    bool _failed = false;
};

#endif
