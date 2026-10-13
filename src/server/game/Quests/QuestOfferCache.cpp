/*
 * Project Ambrose by Imjustchico
 * Keeps each offer with whether it still stands, was declined or lapsed when the wizard left the NPC's range.
 */

#include "QuestOfferCache.h"

void QuestOfferCache::Offer(std::string_view quest, uint64 npc)
{
    _offers.insert_or_assign({ std::string(quest), npc }, QuestOfferVerdict::Standing);
}

bool QuestOfferCache::Decline(std::string_view quest)
{
    bool declined = false;
    for (auto& [key, state] : _offers)
        if (key.first == quest && state == QuestOfferVerdict::Standing)
        {
            state = QuestOfferVerdict::Declined;
            declined = true;
        }
    return declined;
}

void QuestOfferCache::LeaveRange(uint64 npc)
{
    for (auto& [key, state] : _offers)
        if (key.second == npc && state == QuestOfferVerdict::Standing)
            state = QuestOfferVerdict::LeftRange;
}

QuestOfferVerdict QuestOfferCache::Check(std::string_view quest, uint64 npc) const
{
    auto const found = _offers.find({ std::string(quest), npc });
    return found == _offers.end() ? QuestOfferVerdict::NeverOffered : found->second;
}

bool QuestOfferCache::Take(std::string_view quest, uint64 npc)
{
    auto const found = _offers.find({ std::string(quest), npc });
    if (found == _offers.end() || found->second != QuestOfferVerdict::Standing)
        return false;
    _offers.erase(found);
    return true;
}

std::string_view QuestOfferCache::GetVerdictName(QuestOfferVerdict verdict) noexcept
{
    switch (verdict)
    {
        case QuestOfferVerdict::Standing: return "standing";
        case QuestOfferVerdict::NeverOffered: return "never offered";
        case QuestOfferVerdict::Declined: return "declined";
        case QuestOfferVerdict::LeftRange: return "offered by an NPC the wizard has left";
    }
    return "unknown";
}
