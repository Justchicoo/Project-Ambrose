/*
 * Project Ambrose by Imjustchico
 * The rows of the world database's quest tables as plain records, one struct per table, together with each quest's optional requirement-list reference and the object_template ids and names they are checked against, gathered into one set that the database reader or a test fills and the validator and the quest store read.
 */

#ifndef AMBROSE_QUESTROWS_H
#define AMBROSE_QUESTROWS_H

#include "Types.h"

#include <string>
#include <vector>

struct QuestTemplateRow
{
    std::string Name;
    uint32 NameId = 0;
    std::string TitleKey;
    std::string InfoKey;
    std::string PrepKey;
    std::string UnderwayKey;
    std::string CompleteKey;
    uint32 Level = 0;
    uint32 Repeat = 0;
    bool Mainline = false;
    bool NoQuestHelper = false;
    bool SkipQhAutoselect = false;
    bool PetOnly = false;
    uint32 ActivityType = 0;
    bool PrepAlways = false;
    bool IsHidden = false;
    std::string RequirementListId;
};

struct QuestStartGoalRow
{
    std::string Quest;
    std::string Goal;
};

struct QuestGoalRow
{
    std::string Quest;
    std::string Goal;
    uint32 NameId = 0;
    std::string Type;
    std::string TitleKey;
    std::string UnderwayKey;
    std::string CompleteKey;
    std::string LocationKey;
    std::string DestinationZone;
    std::string Image1;
    std::string Image2;
    std::string PersonaName;
    bool UsePatron = false;
    uint32 TallyCount = 0;
    uint32 TallyPercent = 0;
    std::string TallyDescriptorKey;
    std::string TallyDescriptor2Key;
    bool ZoneEntry = false;
    bool ZoneExit = false;
    std::string ZoneTag;
    std::string ProximityTag;
    uint32 Rank = 0;
    bool HideFloatyText = false;
    bool HideLocation = false;
};

struct QuestGoalAdjectiveRow
{
    std::string Quest;
    std::string Goal;
    uint32 Position = 0;
    std::string Adjective;
};

struct QuestGoalClientTagRow
{
    std::string Quest;
    std::string Goal;
    uint32 Position = 0;
    std::string Tag;
};

struct QuestGoalLogicRow
{
    std::string Quest;
    std::string Goal;
    uint32 Position = 0;
    bool CompleteQuest = false;
};

struct QuestGoalLogicMemberRow
{
    std::string Quest;
    std::string Goal;
    uint32 Logic = 0;
    std::string Kind;
    uint32 Position = 0;
    std::string Member;
};

struct QuestDialogRow
{
    std::string Quest;
    std::string Owner;
    std::string Goal;
    std::string Tag;
};

struct QuestDialogEntryRow
{
    std::string Quest;
    std::string Owner;
    std::string Goal;
    std::string Tag;
    uint32 Position = 0;
    uint32 ActorTemplateId = 0;
    std::string DialogKey;
    std::string Picture;
    std::string Sound;
    std::string Action;
    std::string DialogEvent;
    std::string Animation;
};

struct QuestDialogMadlibRow
{
    std::string Quest;
    std::string Owner;
    std::string Goal;
    std::string Tag;
    uint32 Entry = 0;
    std::string Identifier;
    std::string Value;
};

struct QuestObjectRow
{
    uint32 TemplateId = 0;
    std::string ObjectName;
};

struct QuestRows
{
    std::vector<QuestTemplateRow> Quests;
    std::vector<QuestStartGoalRow> StartGoals;
    std::vector<QuestGoalRow> Goals;
    std::vector<QuestGoalAdjectiveRow> Adjectives;
    std::vector<QuestGoalClientTagRow> ClientTags;
    std::vector<QuestGoalLogicRow> Logic;
    std::vector<QuestGoalLogicMemberRow> LogicMembers;
    std::vector<QuestDialogRow> Dialogs;
    std::vector<QuestDialogEntryRow> DialogEntries;
    std::vector<QuestDialogMadlibRow> Madlibs;
    std::vector<QuestObjectRow> Objects;
};

#endif
