/*
 * Project Ambrose by Imjustchico
 * The object states of a wizard that other clients are told of with MSG_ENTERSTATE, each the id the client's string hash gives the state's name in the state set a player's object uses, PlayerMobileStates, whose Jump category holds Jumping, the state that plays the jump animation and falls back to NotJumping on its own.
 */

#ifndef AMBROSE_PLAYERSTATES_H
#define AMBROSE_PLAYERSTATES_H

#include "StringHash.h"
#include "Types.h"

#include <string_view>

namespace PlayerStates
{
    inline constexpr std::string_view StateSet = "PlayerMobileStates";
    inline constexpr std::string_view JumpCategory = "Jump";
    inline constexpr std::string_view JumpingName = "Jumping";
    inline constexpr uint32 Jumping = StringHash::StringId(JumpingName);
}

#endif
