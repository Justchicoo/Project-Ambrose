/*
 * Project Ambrose by Imjustchico
 * Names each experience source and its rate setting, and scales a grant by that rate, rounded to the nearest point and kept within an int, with a command's grant passed through unchanged.
 */

#include "PlayerLevel.h"
#include "Settings.h"

#include <algorithm>
#include <cmath>
#include <limits>

std::string_view PlayerLevel::RateSetting(XpSource source) noexcept
{
    switch (source)
    {
        case XpSource::Quest: return "Rate.XP.Quest";
        case XpSource::Kill: return "Rate.XP.Kill";
        case XpSource::Command: return {};
    }
    return {};
}

std::string_view PlayerLevel::Name(XpSource source) noexcept
{
    switch (source)
    {
        case XpSource::Quest: return "quest";
        case XpSource::Kill: return "kill";
        case XpSource::Command: return "command";
    }
    return "unknown";
}

int32 PlayerLevel::ScaleExperience(int32 amount, XpSource source)
{
    std::string_view const setting = RateSetting(source);
    if (setting.empty() || amount <= 0)
        return std::max(amount, 0);
    double const scaled = std::round(static_cast<double>(amount) * sSettings.Get<float>(setting));
    return static_cast<int32>(std::clamp<double>(scaled, 0.0, std::numeric_limits<int32>::max()));
}
