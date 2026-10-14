/*
 * Project Ambrose by Imjustchico
 * Picks the marker an NPC's menu earns for one wizard, a goal to complete ahead of a quest to offer, and names it for the log.
 */

#include "NpcWizBang.h"

#include <algorithm>

namespace NpcWizBang
{
    uint32 Of(NpcServiceMenu const& menu) noexcept
    {
        std::vector<NpcServiceMenu::Entry> const& entries = menu.GetEntries();
        if (std::ranges::any_of(entries, [](NpcServiceMenu::Entry const& entry) { return entry.Option.Goal.has_value(); }))
            return CompleteQuestGoal;
        if (std::ranges::any_of(entries, [](NpcServiceMenu::Entry const& entry) { return entry.Option.PrepText.has_value(); }))
            return StartQuest;
        return None;
    }

    std::string_view GetName(uint32 wizBang) noexcept
    {
        if (wizBang == CompleteQuestGoal)
            return "CompleteQuestGoal";
        if (wizBang == StartQuest)
            return "StartQuest";
        return "None";
    }
}
