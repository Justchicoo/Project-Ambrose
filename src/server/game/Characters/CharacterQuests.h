/*
 * Project Ambrose by Imjustchico
 * A wizard's quest log as the characters database holds it: its active quests and their goals by the GIDs they keep for life, its per-quest and other registry entries, the quests it hid, and the revision of the save that wrote them.
 */

#ifndef AMBROSE_CHARACTERQUESTS_H
#define AMBROSE_CHARACTERQUESTS_H

#include "Types.h"

#include <string>
#include <vector>

struct CharacterQuestRow
{
    uint64 QuestGid = 0;
    std::string QuestName;
    uint64 AcceptedAt = 0;

    bool operator==(CharacterQuestRow const&) const = default;
};

struct CharacterQuestGoalRow
{
    uint64 GoalGid = 0;
    uint64 QuestGid = 0;
    std::string GoalName;
    bool Complete = false;
    uint32 Count = 0;

    bool operator==(CharacterQuestGoalRow const&) const = default;
};

struct CharacterRegistryRow
{
    std::string QuestName;
    std::string Entry;
    double Value = 0.0;

    bool operator==(CharacterRegistryRow const&) const = default;
};

struct CharacterQuests
{
    std::vector<CharacterQuestRow> Quests;
    std::vector<CharacterQuestGoalRow> Goals;
    std::vector<CharacterRegistryRow> QuestRegistry;
    std::vector<CharacterRegistryRow> Registry;
    std::vector<std::string> Hidden;
    uint64 Revision = 0;

    bool operator==(CharacterQuests const&) const = default;
};

#endif
