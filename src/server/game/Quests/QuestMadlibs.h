/*
 * Project Ambrose by Imjustchico
 * The quest, goal and NPC madlib block model and the typed arguments built from quest display keys, goal progress and NPC display fields.
 */

#ifndef AMBROSE_QUESTMADLIBS_H
#define AMBROSE_QUESTMADLIBS_H

#include "QuestTemplate.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace QuestMadlibs
{
    struct ConstStringArgument
    {
        std::string Value;

        bool operator==(ConstStringArgument const&) const = default;
    };

    using ArgumentValue = std::variant<std::string, ConstStringArgument, std::u16string, int32, uint32, uint64, float, double>;

    struct Argument
    {
        std::string Token;
        ArgumentValue Value;

        bool operator==(Argument const&) const = default;
    };

    struct Block
    {
        std::string BlockToken;
        std::vector<Argument> Arguments;

        bool operator==(Block const&) const = default;
    };

    struct NpcFields
    {
        std::string Name;
        std::string FirstName;
        std::string LastName;
        std::string Title;
        std::string Nickname;
        std::string FullName;
    };

    Block BuildQuest(Quests::QuestTemplate const& quest);
    Block BuildGoal(Quests::QuestGoal const& goal, int32 count, std::optional<int32> subscriberTotal = {});
    Block BuildGoal(Quests::GoalTemplate const& goal, int32 count, int32 total, std::optional<int32> subscriberTotal = {});
    Block BuildNpc(NpcFields const& npc);
}

#endif
