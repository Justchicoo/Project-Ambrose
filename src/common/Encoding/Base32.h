/*
 * Project Ambrose by Imjustchico
 * Base32 in the RFC 4648 alphabet, written without padding the way an authenticator app reads a secret, and in Crockford's alphabet, read loosely the way a person types it: any case, O as zero, I and L as one, and dashes and spaces ignored.
 */

#ifndef AMBROSE_BASE32_H
#define AMBROSE_BASE32_H

#include "Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Base32
{
    enum class Alphabet
    {
        Rfc4648,
        Crockford
    };

    std::string_view Characters(Alphabet alphabet) noexcept;
    std::string Encode(std::span<uint8 const> bytes, Alphabet alphabet = Alphabet::Rfc4648);
    std::optional<std::string> Canonical(std::string_view text, Alphabet alphabet = Alphabet::Rfc4648);
    std::optional<std::vector<uint8>> Decode(std::string_view text, Alphabet alphabet = Alphabet::Rfc4648);
}

#endif
