/*
 * Project Ambrose by Imjustchico
 * Slicing-by-8 CRC-32 tables generated at compile time, plus polynomial arithmetic for combining two CRCs.
 */

#include "Crc32.h"

#include <array>

namespace
{
    using Table = std::array<std::array<uint32, 256>, 8>;

    constexpr Table MakeTables() noexcept
    {
        Table tables{};
        for (uint32 index = 0; index < 256; ++index)
        {
            uint32 value = index;
            for (int bit = 0; bit < 8; ++bit)
                value = (value & 1u) != 0 ? (value >> 1) ^ Crc32::Polynomial : value >> 1;
            tables[0][index] = value;
        }
        for (uint32 index = 0; index < 256; ++index)
            for (std::size_t slice = 1; slice < 8; ++slice)
                tables[slice][index] = (tables[slice - 1][index] >> 8) ^ tables[0][tables[slice - 1][index] & 0xFFu];
        return tables;
    }

    constinit Table const Tables = MakeTables();

    uint32 MultiplyModulo(uint32 left, uint32 right) noexcept
    {
        uint32 mask = 1u << 31;
        uint32 product = 0;
        while (true)
        {
            if ((left & mask) != 0)
            {
                product ^= right;
                if ((left & (mask - 1)) == 0)
                    break;
            }
            mask >>= 1;
            right = (right & 1u) != 0 ? (right >> 1) ^ Crc32::Polynomial : right >> 1;
        }
        return product;
    }

    uint32 PowerOfXModulo(uint64 exponentBytes) noexcept
    {
        std::array<uint32, 64> powers{};
        powers[0] = 1u << 30;
        for (std::size_t i = 1; i < powers.size(); ++i)
            powers[i] = MultiplyModulo(powers[i - 1], powers[i - 1]);
        uint32 result = 1u << 31;
        uint64 bits = exponentBytes * 8;
        for (std::size_t i = 0; bits != 0 && i < powers.size(); ++i, bits >>= 1)
            if ((bits & 1u) != 0)
                result = MultiplyModulo(powers[i], result);
        return result;
    }

    std::span<uint8 const> AsBytes(std::string_view text) noexcept
    {
        return { reinterpret_cast<uint8 const*>(text.data()), text.size() };
    }
}

Crc32 Crc32::Client() noexcept
{
    return Crc32(ClientInitial, 0);
}

Crc32 Crc32::Standard() noexcept
{
    return Crc32(StandardInitial, StandardFinalXor);
}

Crc32& Crc32::Update(std::span<uint8 const> data) noexcept
{
    _state = UpdateRaw(_state, data);
    return *this;
}

Crc32& Crc32::Update(std::string_view data) noexcept
{
    return Update(AsBytes(data));
}

uint32 Crc32::GetValue() const noexcept
{
    return _state ^ _finalXor;
}

void Crc32::Reset() noexcept
{
    _state = _initial;
}

uint32 Crc32::UpdateRaw(uint32 state, std::span<uint8 const> data) noexcept
{
    uint8 const* bytes = data.data();
    std::size_t remaining = data.size();
    while (remaining >= 8)
    {
        uint32 const low = state ^ (uint32{ bytes[0] } | (uint32{ bytes[1] } << 8) | (uint32{ bytes[2] } << 16) | (uint32{ bytes[3] } << 24));
        state = Tables[7][low & 0xFFu] ^ Tables[6][(low >> 8) & 0xFFu] ^ Tables[5][(low >> 16) & 0xFFu] ^ Tables[4][low >> 24]
            ^ Tables[3][bytes[4]] ^ Tables[2][bytes[5]] ^ Tables[1][bytes[6]] ^ Tables[0][bytes[7]];
        bytes += 8;
        remaining -= 8;
    }
    while (remaining-- > 0)
        state = (state >> 8) ^ Tables[0][(state ^ *bytes++) & 0xFFu];
    return state;
}

uint32 Crc32::ComputeClient(std::span<uint8 const> data) noexcept
{
    return UpdateRaw(ClientInitial, data);
}

uint32 Crc32::ComputeClient(std::string_view data) noexcept
{
    return ComputeClient(AsBytes(data));
}

uint32 Crc32::ComputeStandard(std::span<uint8 const> data) noexcept
{
    return UpdateRaw(StandardInitial, data) ^ StandardFinalXor;
}

uint32 Crc32::ComputeStandard(std::string_view data) noexcept
{
    return ComputeStandard(AsBytes(data));
}

uint32 Crc32::Combine(uint32 first, uint32 second, std::size_t secondLength, uint32 initial, uint32 finalXor) noexcept
{
    return MultiplyModulo(PowerOfXModulo(secondLength), first ^ initial ^ finalXor) ^ second;
}

uint32 Crc32::CombineWith(uint32 first, uint32 second, std::size_t secondLength) const noexcept
{
    return Combine(first, second, secondLength, _initial, _finalXor);
}
