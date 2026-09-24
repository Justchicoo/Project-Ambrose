/*
 * Project Ambrose by Imjustchico
 * RFC 4648 Base64 in the standard and URL-safe alphabets with strict canonical decoding.
 */

#ifndef AMBROSE_BASE64_H
#define AMBROSE_BASE64_H

#include "Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Base64
{
    enum class Alphabet
    {
        Standard,
        UrlSafe
    };

    enum class Padding
    {
        Required,
        Omitted
    };

    std::string Encode(std::span<uint8 const> bytes, Alphabet alphabet = Alphabet::Standard, Padding padding = Padding::Required);
    std::optional<std::vector<uint8>> Decode(std::string_view text, Alphabet alphabet = Alphabet::Standard, Padding padding = Padding::Required);
}

#endif
