/*
 * Project Ambrose by Imjustchico
 * The quest-goal data used by the game, including the client goal type values, completion logic, tally keys and the typed fields of persona, bounty, scavenge, waypoint and rank goals.
 */

#ifndef AMBROSE_GOALTEMPLATE_H
#define AMBROSE_GOALTEMPLATE_H

#include "Types.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Quests
{
    enum class GoalType : int32
    {
        Unknown = 0,
        Bounty = 1,
        BountyCollect = 2,
        Scavenge = 3,
        Persona = 4,
        Waypoint = 5,
        ScavengeFake = 6,
        AchieveRank = 7,
        Usage = 8,
        CompleteQuest = 9,
        SociaRank = 10,
        SociaCurrency = 11
    };

    struct GoalCompleteLogic
    {
        std::vector<std::string> GoalsAnd;
        std::vector<std::string> GoalsOr;
        std::vector<std::string> GoalsToAdd;
        bool CompleteQuest = false;
        int32 RequiredOrCount = 0;
    };

    struct TallyCounterTemplate
    {
        float PercentChance = 0.0f;
        std::string Descriptor;
        std::string Descriptor2;
        int32 Count = 0;
    };

    struct GoalTemplate
    {
        GoalType Type = GoalType::Unknown;
        std::string Name;
        uint32 NameId = 0;
        std::string Title;
        std::string Underway;
        std::string Hyperlink;
        std::string CompleteText;
        std::string LocationName;
        std::string DisplayImage1;
        std::string DisplayImage2;
        std::string ClientTags;
        std::string GenericEvents;
        bool AutoQualify = false;
        bool AutoComplete = false;
        std::string DestinationZone;
        bool NoQuestHelper = false;
        bool PetOnlyQuest = false;
        bool HideGoalFloatyText = false;
        bool HideGoal = false;
        std::string RequiresMagicWeavingSchool;
        std::optional<TallyCounterTemplate> TallyCounter;
    };

    struct PersonaGoalTemplate : GoalTemplate
    {
        std::string PersonaName;
        bool UsePatron = false;
    };

    struct BountyGoalTemplate : GoalTemplate
    {
        std::string NpcAdjectives;
        int32 BountyTotal = 0;
        int64 BountyType = 0;
    };

    struct ScavengeGoalTemplate : GoalTemplate
    {
        std::string ItemAdjectives;
        int32 ItemTotal = 0;
    };

    struct WaypointGoalTemplate : GoalTemplate
    {
        bool ZoneEntry = false;
        std::string ZoneTag;
        std::string ProximityTag;
        bool ZoneExit = false;
    };

    struct AchieveRankGoalTemplate : GoalTemplate
    {
        int32 Rank = 0;
    };

    using QuestGoal = std::variant<GoalTemplate, PersonaGoalTemplate, BountyGoalTemplate, ScavengeGoalTemplate, WaypointGoalTemplate, AchieveRankGoalTemplate>;
}

#endif
