/*
 * Project Ambrose by Imjustchico
 * The one-use codes that sign somebody in when their authenticator is lost: ten of them at a time, each ten random characters of Crockford's base32, fifty bits that are too many to guess and few enough to type, shown in two groups of five and read back in any case, with or without the dash, and with the letters people confuse for digits taken as those digits.
 */

#ifndef AMBROSE_RECOVERYCODE_H
#define AMBROSE_RECOVERYCODE_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace RecoveryCode
{
    inline constexpr std::size_t Count = 10;
    inline constexpr std::size_t Length = 10;
    inline constexpr std::size_t GroupLength = 5;

    std::string Generate();
    std::string Group(std::string_view code);
    std::optional<std::string> Normalize(std::string_view typed);
}

#endif
