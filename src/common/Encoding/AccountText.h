/*
 * Project Ambrose by Imjustchico
 * The rules a username and a password are held to, kept where everything that sets one can reach them: which characters a name may hold and how long it may be, what makes text storable, and how many codepoints a password has, so a game account and a panel user are judged by the same rules rather than by two copies of them that drift.
 */

#ifndef AMBROSE_ACCOUNTTEXT_H
#define AMBROSE_ACCOUNTTEXT_H

#include "Types.h"

#include <cstddef>
#include <string_view>

namespace Ambrose::AccountText
{
    inline constexpr uint32 MaxUsernameLength = 32;
    inline constexpr uint32 MaxPasswordLength = 128;
    inline constexpr uint32 MaxEmailLength = 255;
    inline constexpr uint32 DefaultUsernameMinLength = 3;
    inline constexpr uint32 DefaultPasswordMinLength = 4;

    enum class TextProblem : uint8
    {
        Ok,
        TooShort,
        TooLong,
        Invalid
    };

    bool IsUsernameCharacter(char c) noexcept;
    bool IsStorable(std::string_view text) noexcept;
    std::size_t CountCodepoints(std::string_view text) noexcept;

    TextProblem CheckUsername(std::string_view username, uint32 minLength) noexcept;
    TextProblem CheckPassword(std::string_view password, uint32 minLength) noexcept;
}

#endif
