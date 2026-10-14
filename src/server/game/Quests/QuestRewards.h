/*
 * Project Ambrose by Imjustchico
 * The gold and experience a quest's results give: ResAddGold and ResAddMagicXP read from a decoded result as the data gives them, quest gold passed through the live Rate.Gold.Quest when it is given, and the rewards a result list will give, each through its live rate, gathered as the LootInfoList the offer window shows, all its gold in one gold info.
 */

#ifndef AMBROSE_QUESTREWARDS_H
#define AMBROSE_QUESTREWARDS_H

#include "QuestWireEncoder.h"
#include "ResultMgr.h"

#include <optional>
#include <string_view>

class PropertyObject;

namespace QuestRewards
{
    inline constexpr std::string_view GoldClass = "class ResAddGold";
    inline constexpr std::string_view ExperienceClass = "class ResAddMagicXP";

    int32 ScaleGold(int32 gold);
    std::optional<QuestWireEncoder::GoldLootInfo> GoldOf(PropertyObject const& result);
    std::optional<QuestWireEncoder::MagicXPLootInfo> ExperienceOf(PropertyObject const& result);
    void AddGold(QuestWireEncoder::LootInfoList& loot, int32 gold);
    QuestWireEncoder::LootInfoList Preview(ResultList const& results);
}

#endif
