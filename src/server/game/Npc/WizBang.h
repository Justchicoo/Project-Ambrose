/*
 * Project Ambrose by Imjustchico
 * Names the wizbang markers the client displays over NPCs and their priority when more than one provider offers that NPC to a wizard.
 */

#ifndef AMBROSE_WIZBANG_H
#define AMBROSE_WIZBANG_H

#include "StringHash.h"

namespace NpcWizBang
{
    enum class Id : uint32
    {
        None = 0,
        CompleteQuestGoal = StringHash::KiStringHash("CompleteQuestGoal"),
        StartQuest = StringHash::KiStringHash("StartQuest"),
        UnfinishedQuest = StringHash::KiStringHash("UnfinishedQuest"),
        Training = StringHash::KiStringHash("Training"),
        Shopping = StringHash::KiStringHash("Shopping")
    };

    constexpr uint8 Priority(Id id) noexcept
    {
        switch (id)
        {
            case Id::CompleteQuestGoal:
                return 5;
            case Id::StartQuest:
                return 4;
            case Id::UnfinishedQuest:
                return 3;
            case Id::Training:
                return 2;
            case Id::Shopping:
                return 1;
            case Id::None:
            default:
                return 0;
        }
    }
}

#endif
