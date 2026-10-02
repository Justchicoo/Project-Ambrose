/*
 * Project Ambrose by Imjustchico
 * Maps a player's named wizbang state to the identifier the GAME message broadcasts.
 */

#ifndef AMBROSE_PLAYERWIZBANG_H
#define AMBROSE_PLAYERWIZBANG_H

#include "StringHash.h"

#include <string_view>

namespace PlayerWizBang
{
    inline constexpr uint32 SpellbookId = StringHash::KiStringHash("SpellbookWizbang");

    constexpr uint32 IdForState(std::string_view stateName) noexcept
    {
        return stateName == "SpellbookWizbang" ? SpellbookId : 0;
    }
}

#endif
