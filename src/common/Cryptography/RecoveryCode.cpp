/*
 * Project Ambrose by Imjustchico
 * Draws each character of a code from the system's secure generator without bias, groups a code with one dash in the middle, and reads a typed code through base32's Crockford spelling so it is either exactly ten characters of the alphabet or not a code at all.
 */

#include "RecoveryCode.h"
#include "Base32.h"
#include "CryptoRandom.h"

std::string RecoveryCode::Generate()
{
    std::string_view const characters = Base32::Characters(Base32::Alphabet::Crockford);
    std::string code;
    code.reserve(Length);
    for (std::size_t index = 0; index < Length; ++index)
        code.push_back(characters[Ambrose::Crypto::GetRandomBelow(static_cast<uint32>(characters.size()))]);
    return code;
}

std::string RecoveryCode::Group(std::string_view code)
{
    if (code.size() <= GroupLength)
        return std::string(code);
    return std::string(code.substr(0, GroupLength)) + "-" + std::string(code.substr(GroupLength));
}

std::optional<std::string> RecoveryCode::Normalize(std::string_view typed)
{
    std::optional<std::string> canonical = Base32::Canonical(typed, Base32::Alphabet::Crockford);
    if (!canonical || canonical->size() != Length)
        return std::nullopt;
    return canonical;
}
