/*
 * Project Ambrose by Imjustchico
 * Streaming reflected CRC-32 (polynomial 0xEDB88320) with the client's init-0 variant and the standard zlib variant.
 */

#ifndef AMBROSE_CRC32_H
#define AMBROSE_CRC32_H

#include "Types.h"

#include <cstddef>
#include <span>
#include <string_view>

class Crc32
{
public:
    static constexpr uint32 Polynomial = 0xEDB88320u;
    static constexpr uint32 ClientInitial = 0x00000000u;
    static constexpr uint32 StandardInitial = 0xFFFFFFFFu;
    static constexpr uint32 StandardFinalXor = 0xFFFFFFFFu;

    constexpr explicit Crc32(uint32 initial = ClientInitial, uint32 finalXor = 0) noexcept : _state(initial), _initial(initial), _finalXor(finalXor)
    {
    }

    static Crc32 Client() noexcept;
    static Crc32 Standard() noexcept;

    Crc32& Update(std::span<uint8 const> data) noexcept;
    Crc32& Update(std::string_view data) noexcept;
    uint32 GetValue() const noexcept;
    void Reset() noexcept;

    static uint32 UpdateRaw(uint32 state, std::span<uint8 const> data) noexcept;
    static uint32 ComputeClient(std::span<uint8 const> data) noexcept;
    static uint32 ComputeClient(std::string_view data) noexcept;
    static uint32 ComputeStandard(std::span<uint8 const> data) noexcept;
    static uint32 ComputeStandard(std::string_view data) noexcept;
    static uint32 Combine(uint32 first, uint32 second, std::size_t secondLength, uint32 initial = ClientInitial, uint32 finalXor = 0) noexcept;
    uint32 CombineWith(uint32 first, uint32 second, std::size_t secondLength) const noexcept;

private:
    uint32 _state;
    uint32 _initial;
    uint32 _finalXor;
};

#endif
