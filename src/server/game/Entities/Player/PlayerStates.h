/*
 * Project Ambrose by Imjustchico
 * The object states of a wizard that other clients are told of with MSG_ENTERSTATE, each the id the client's string hash gives the state's name in the state set a player's object uses, PlayerMobileStates, whose Jump category holds Jumping, the state that plays the jump animation and falls back to NotJumping on its own, and whose Expression category holds Emoting, an EmoteGameState with no animation of its own that plays the one the EmoteStateOverrideInfo sent with it names on entry, and Unremarkable, the category's base state. Nothing takes another client's copy of a wizard out of Emoting on its own, and a client ignores an MSG_ENTERSTATE naming the state an object already stands in, so each emote first puts the object back in Unremarkable, or only the first would play.
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
    inline constexpr std::string_view ExpressionCategory = "Expression";
    inline constexpr std::string_view EmotingName = "Emoting";
    inline constexpr uint32 Emoting = StringHash::StringId(EmotingName);
    inline constexpr std::string_view UnremarkableName = "Unremarkable";
    inline constexpr uint32 Unremarkable = StringHash::StringId(UnremarkableName);
    inline constexpr std::string_view EmoteOverrideClass = "class EmoteStateOverrideInfo";
}

#endif
