/*
 * Project Ambrose by Imjustchico
 * Matches an event against each trigger's fire events case-sensitively, as the client's own data spells them, and records each firing for the trigger's count and the wizard's cooldown.
 */

#include "ZoneTriggers.h"

#include <algorithm>

std::vector<ZoneTrigger const*> ZoneTriggers::Post(std::string_view event, uint64 wizard, Clock::time_point now, bool doorsIgnoreRequirements)
{
    std::vector<ZoneTrigger const*> fired;
    for (ZoneTrigger const& trigger : _triggers)
    {
        if (std::find(trigger.FireEvents.begin(), trigger.FireEvents.end(), event) == trigger.FireEvents.end())
            continue;
        if (trigger.HasRequirements && !(doorsIgnoreRequirements && trigger.Teleports))
            continue;
        int32& count = _fired[trigger.Index];
        if (trigger.TriggerMax >= 0 && count >= trigger.TriggerMax)
            continue;
        auto const key = std::make_pair(trigger.Index, wizard);
        if (trigger.CooldownSeconds > 0.0f)
        {
            auto const last = _lastFired.find(key);
            if (last != _lastFired.end() && now - last->second < std::chrono::duration<float>(trigger.CooldownSeconds))
                continue;
        }
        ++count;
        _lastFired[key] = now;
        fired.push_back(&trigger);
    }
    return fired;
}
