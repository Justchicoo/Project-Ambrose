/*
 * Project Ambrose by Imjustchico
 * The quests one wizard has been offered, by quest name and the NPC that offered it, so an accept is taken only for an offer still standing: never offered, declined since, or offered by an NPC the wizard has since walked away from, it is refused with the reason, and the wizard's state does not change.
 */

#ifndef AMBROSE_QUESTOFFERCACHE_H
#define AMBROSE_QUESTOFFERCACHE_H

#include "Types.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>

enum class QuestOfferVerdict : uint8
{
    Standing,
    NeverOffered,
    Declined,
    LeftRange
};

class QuestOfferCache
{
public:
    void Offer(std::string_view quest, uint64 npc);
    bool Decline(std::string_view quest);
    void LeaveRange(uint64 npc);
    QuestOfferVerdict Check(std::string_view quest, uint64 npc) const;
    bool Take(std::string_view quest, uint64 npc);
    void Clear() noexcept { _offers.clear(); }

    static std::string_view GetVerdictName(QuestOfferVerdict verdict) noexcept;

private:
    std::map<std::pair<std::string, uint64>, QuestOfferVerdict> _offers;
};

#endif
