/*
 * Project Ambrose by Imjustchico
 * Hex string conversion and hexdump -C style dumps for inspecting packets and binary data.
 */

#ifndef AMBROSE_HEX_H
#define AMBROSE_HEX_H

#include "Types.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Hex
{
    enum class Case
    {
        Lower,
        Upper
    };

    std::string Encode(std::span<uint8 const> bytes, Case letterCase = Case::Lower);
    std::optional<std::vector<uint8>> Decode(std::string_view text);
    std::string Dump(std::span<uint8 const> bytes, std::size_t startOffset = 0);
}

#endif
