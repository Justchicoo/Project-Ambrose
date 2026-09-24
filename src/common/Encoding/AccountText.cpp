/*
 * Project Ambrose by Imjustchico
 * A username is letters, digits, underscore, dash and dot, judged too long before its characters so a long name of bad characters is reported by length; a password is valid UTF-8 with no control characters and is measured in codepoints, so a name written in one script is not held shorter than the same name in another.
 */

#include "AccountText.h"
#include "Utf.h"

namespace Ambrose::AccountText
{
    bool IsUsernameCharacter(char c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    }

    bool IsStorable(std::string_view text) noexcept
    {
        if (!Utf::IsValidUtf8(text))
            return false;
        for (char const c : text)
            if (static_cast<uint8>(c) < 0x20 || c == 0x7F)
                return false;
        return true;
    }

    std::size_t CountCodepoints(std::string_view text) noexcept
    {
        std::size_t count = 0;
        for (char const c : text)
            if ((static_cast<uint8>(c) & 0xC0) != 0x80)
                ++count;
        return count;
    }

    TextProblem CheckUsername(std::string_view username, uint32 minLength) noexcept
    {
        if (username.size() > MaxUsernameLength)
            return TextProblem::TooLong;
        for (char const c : username)
            if (!IsUsernameCharacter(c))
                return TextProblem::Invalid;
        if (username.size() < minLength)
            return TextProblem::TooShort;
        return TextProblem::Ok;
    }

    TextProblem CheckPassword(std::string_view password, uint32 minLength) noexcept
    {
        if (password.size() > MaxPasswordLength)
            return TextProblem::TooLong;
        if (!IsStorable(password))
            return TextProblem::Invalid;
        if (CountCodepoints(password) < minLength)
            return TextProblem::TooShort;
        return TextProblem::Ok;
    }
}
