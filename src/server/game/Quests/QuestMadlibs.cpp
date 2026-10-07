/*
 * Project Ambrose by Imjustchico
 * Builds the QUEST, GOAL and NPC madlib tokens as string-key or numeric arguments using the quest and goal model values.
 */

#include "QuestMadlibs.h"

#include <type_traits>
#include <utility>

namespace QuestMadlibs
{
    namespace
    {
        Argument String(std::string token, std::string value)
        {
            return { std::move(token), std::move(value) };
        }

        Argument Integer(std::string token, int32 value)
        {
            return { std::move(token), value };
        }
    }

    Block BuildQuest(Quests::QuestTemplate const& quest)
    {
        return { "QUEST", { String("NAME", quest.Name), Integer("LEVEL", quest.Level) } };
    }

    Block BuildGoal(Quests::GoalTemplate const& goal, int32 count, int32 total, std::optional<int32> subscriberTotal)
    {
        std::string tallyText;
        std::string tallyText2;
        if (goal.TallyCounter)
        {
            tallyText = goal.TallyCounter->Descriptor;
            tallyText2 = goal.TallyCounter->Descriptor2;
        }

        Block block{ "GOAL",
            { String("NAME", goal.Name),
                String("LOCATION", goal.LocationName),
                String("TALLYTEXT", std::move(tallyText)),
                String("TALLYTEXT2", std::move(tallyText2)),
                Integer("COUNT", count),
                Integer("TOTAL", total) } };
        if (subscriberTotal)
            block.Arguments.push_back(Integer("SUBSCRIBER_TOTAL", *subscriberTotal));
        return block;
    }

    Block BuildGoal(Quests::QuestGoal const& goal, int32 count, std::optional<int32> subscriberTotal)
    {
        return std::visit([count, subscriberTotal](auto const& typedGoal)
        {
            using Goal = std::remove_cvref_t<decltype(typedGoal)>;
            int32 total = 0;
            if constexpr (std::is_same_v<Goal, Quests::BountyGoalTemplate>)
                total = typedGoal.BountyTotal;
            else if constexpr (std::is_same_v<Goal, Quests::ScavengeGoalTemplate>)
                total = typedGoal.ItemTotal;
            return BuildGoal(static_cast<Quests::GoalTemplate const&>(typedGoal), count, total, subscriberTotal);
        }, goal);
    }

    Block BuildNpc(NpcFields const& npc)
    {
        return { "NPC",
            { String("NAME", npc.Name),
                String("FIRSTNAME", npc.FirstName),
                String("LASTNAME", npc.LastName),
                String("TITLE", npc.Title),
                String("NICKNAME", npc.Nickname),
                String("FULLNAME", npc.FullName) } };
    }
}
