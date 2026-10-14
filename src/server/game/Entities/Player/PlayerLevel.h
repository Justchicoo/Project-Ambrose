/*
 * Project Ambrose by Imjustchico
 * Where a wizard's experience comes from and what each source is worth: a quest and a defeated creature each pass their grant through their own live rate, Rate.XP.Quest and Rate.XP.Kill, read at the moment of the grant, and a game master's command gives the amount it names; and what one grant did, the amount after its rate and the levels, experience, training points and overflow it moved.
 */

#ifndef AMBROSE_PLAYERLEVEL_H
#define AMBROSE_PLAYERLEVEL_H

#include "PlayerStats.h"

#include <string_view>

enum class XpSource : uint8
{
    Quest,
    Kill,
    Command
};

struct XpGrant
{
    int32 Amount = 0;
    LevelProgress Progress;
};

namespace PlayerLevel
{
    std::string_view RateSetting(XpSource source) noexcept;
    std::string_view Name(XpSource source) noexcept;
    int32 ScaleExperience(int32 amount, XpSource source);
}

#endif
