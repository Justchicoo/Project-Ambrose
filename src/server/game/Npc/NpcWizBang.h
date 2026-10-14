/*
 * Project Ambrose by Imjustchico
 * The marker over an NPC's head for one wizard, with the template of the NPC it is shown over: a '?' when its menu holds a goal the wizard completes with it, outranking a '!' when it holds a quest to offer, and none otherwise, by the hash of the marker's name the client reads from MSG_WIZBANG.
 */

#ifndef AMBROSE_NPCWIZBANG_H
#define AMBROSE_NPCWIZBANG_H

#include "NpcServiceMenu.h"
#include "StringHash.h"

namespace NpcWizBang
{
    inline constexpr uint32 None = 0;
    inline constexpr uint32 CompleteQuestGoal = StringHash::KiStringHash("CompleteQuestGoal");
    inline constexpr uint32 StartQuest = StringHash::KiStringHash("StartQuest");

    struct Marker
    {
        uint32 TemplateId = 0;
        uint32 WizBang = None;
    };

    uint32 Of(NpcServiceMenu const& menu) noexcept;
    std::string_view GetName(uint32 wizBang) noexcept;
}

#endif
