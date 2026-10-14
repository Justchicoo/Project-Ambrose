/*
 * Project Ambrose by Imjustchico
 * Reads a quest's gold and experience results as the data gives them, and gathers what a result list gives into a LootInfoList with each amount passed through its live quest rate, the gold of every result summed into its gold info.
 */

#include "QuestRewards.h"
#include "PlayerLevel.h"
#include "PropertyObject.h"
#include "Settings.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

int32 QuestRewards::ScaleGold(int32 gold)
{
    if (gold <= 0)
        return 0;
    double const scaled = std::round(static_cast<double>(gold) * sSettings.Get<float>("Rate.Gold.Quest"));
    return static_cast<int32>(std::clamp<double>(scaled, 0.0, std::numeric_limits<int32>::max()));
}

std::optional<QuestWireEncoder::GoldLootInfo> QuestRewards::GoldOf(PropertyObject const& result)
{
    if (!result.IsA(GoldClass))
        return std::nullopt;
    PropertyValue const* const value = result.Get("m_gold");
    int32 const* const gold = value ? value->GetIf<int32>() : nullptr;
    if (!gold || *gold <= 0)
        return std::nullopt;
    return QuestWireEncoder::GoldLootInfo{ *gold };
}

std::optional<QuestWireEncoder::MagicXPLootInfo> QuestRewards::ExperienceOf(PropertyObject const& result)
{
    if (!result.IsA(ExperienceClass))
        return std::nullopt;
    PropertyValue const* const value = result.Get("m_experience");
    int32 const* const experience = value ? value->GetIf<int32>() : nullptr;
    if (!experience || *experience <= 0)
        return std::nullopt;
    PropertyValue const* const school = result.Get("m_magicSchool");
    std::string const* const name = school ? school->GetIf<std::string>() : nullptr;
    return QuestWireEncoder::MagicXPLootInfo{ name ? *name : std::string(), *experience };
}

void QuestRewards::AddGold(QuestWireEncoder::LootInfoList& loot, int32 gold)
{
    if (gold <= 0)
        return;
    if (!loot.GoldInfo)
        loot.GoldInfo = QuestWireEncoder::GoldLootInfo{};
    int64 const total = static_cast<int64>(loot.GoldInfo->GoldAmount) + gold;
    loot.GoldInfo->GoldAmount = static_cast<int32>(std::min<int64>(total, std::numeric_limits<int32>::max()));
}

QuestWireEncoder::LootInfoList QuestRewards::Preview(ResultList const& results)
{
    QuestWireEncoder::LootInfoList loot;
    for (ResultEntry const& entry : results)
    {
        if (!entry.Object)
            continue;
        if (std::optional<QuestWireEncoder::GoldLootInfo> const gold = GoldOf(*entry.Object))
            AddGold(loot, ScaleGold(gold->GoldAmount));
        else if (std::optional<QuestWireEncoder::MagicXPLootInfo> experience = ExperienceOf(*entry.Object))
        {
            experience->Experience = PlayerLevel::ScaleExperience(experience->Experience, XpSource::Quest);
            loot.Loot.emplace_back(std::move(*experience));
        }
    }
    return loot;
}
