/*
 * Project Ambrose by Imjustchico
 * The pieces the quest offer and the client's quest book build from a quest's rows: a goal type as the client numbers it, a goal's madlibs at a count, a built object encoded bare into a message blob, and the worlds a quest's goals lead to; with the send types and goal statuses MSG_SENDGOAL carries.
 */

#ifndef AMBROSE_QUESTBOOK_H
#define AMBROSE_QUESTBOOK_H

#include "QuestStore.h"
#include "QuestWireEncoder.h"

#include <string>
#include <string_view>
#include <vector>

namespace QuestBook
{
    inline constexpr int32 SendResume = 0;
    inline constexpr int32 SendStart = 1;
    inline constexpr int32 SendProgress = 2;
    inline constexpr uint8 GoalActive = 0;
    inline constexpr uint8 GoalComplete = 1;

    Quests::GoalType WireType(QuestGoalType type) noexcept;
    QuestMadlibs::Block GoalMadlibs(QuestGoalRow const& row, int32 count);
    std::string Blob(QuestWireEncoder::ObjectBuildResult const& built, uint32 mask, std::string_view what, std::string& error);
    std::vector<std::string> AssociatedWorlds(QuestInfo const& quest);
}

#endif
