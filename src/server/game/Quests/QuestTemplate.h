/*
 * Project Ambrose by Imjustchico
 * The quest-template fields the game uses to build quest madlibs and associate a quest's level, localized text keys, repeat behavior, goals and completion logic.
 */

#ifndef AMBROSE_QUESTTEMPLATE_H
#define AMBROSE_QUESTTEMPLATE_H

#include "GoalTemplate.h"

#include <string>
#include <vector>

namespace Quests
{
    struct QuestTemplate
    {
        std::string Name;
        uint32 NameId = 0;
        std::string Title;
        std::string Info;
        std::string Prep;
        std::string Underway;
        std::string Complete;
        std::string StartGoals;
        bool PrepAlways = false;
        std::string ClientTags;
        int32 Level = 0;
        int32 Repeat = 0;
        std::string OnStartQuestScript;
        std::string OnEndQuestScript;
        std::string MissionDoors;
        bool IsHidden = false;
        bool Outdated = false;
        bool NoQuestHelper = false;
        bool Mainline = false;
        std::string DefaultDialogAnimation;
        bool SkipQuestHelperAutoSelect = false;
        bool ForceInteraction = false;
        bool CheckInventoryForCrafting = false;
        bool PlayAsYourPetNpc = false;
        int64 ActivityType = 0;
        std::vector<QuestGoal> Goals;
        GoalCompleteLogic GoalLogic;
    };
}

#endif
