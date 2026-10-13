/*
 * Project Ambrose by Imjustchico
 * One zone's triggers from zone_trigger and zone_trigger_event, with the notify texts their results show and the Dynamods they make for the wizard that fires them: an event posted for a wizard fires every trigger whose fire events name it, unless the trigger has requirements the wizard does not meet, which fail closed when nothing checks them, save for a door when Zone.DoorsIgnoreRequirements lets doors pass, has fired as often as its trigger max allows, or fired for that wizard within its cooldown; what a fired trigger does is left to the results its owning systems run.
 */

#ifndef AMBROSE_ZONETRIGGERS_H
#define AMBROSE_ZONETRIGGERS_H

#include "Types.h"

#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct ZoneNotifyText
{
    std::string Text;
    int32 Type = 0;
};

struct ZoneDynaMod
{
    std::string Tag;
    std::string State;
    bool Remove = false;
    std::string Zone;

    bool operator==(ZoneDynaMod const&) const = default;
};

struct ZoneTrigger
{
    uint32 Index = 0;
    std::string Name;
    int32 TriggerMax = -1;
    float CooldownSeconds = 0.0f;
    bool HasRequirements = false;
    std::vector<std::string> FireEvents;
    std::vector<ZoneNotifyText> NotifyTexts;
    bool Teleports = false;
    std::vector<ZoneDynaMod> DynaMods;
};

class ZoneTriggers
{
public:
    using Clock = std::chrono::steady_clock;

    ZoneTriggers() = default;
    explicit ZoneTriggers(std::vector<ZoneTrigger> triggers) : _triggers(std::move(triggers)) {}

    using RequirementCheck = std::function<bool(ZoneTrigger const&)>;

    std::vector<ZoneTrigger const*> Post(std::string_view event, uint64 wizard, Clock::time_point now, bool doorsIgnoreRequirements = false, RequirementCheck const& meets = {});
    std::vector<ZoneTrigger> const& All() const noexcept { return _triggers; }

private:
    std::vector<ZoneTrigger> _triggers;
    std::map<uint32, int32> _fired;
    std::map<std::pair<uint32, uint64>, Clock::time_point> _lastFired;
};

#endif
