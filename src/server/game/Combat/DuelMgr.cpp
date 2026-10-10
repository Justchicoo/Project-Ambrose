/*
 * Project Ambrose by Imjustchico
 * Holds the duels by id, finds one by participant or sigil by looking through the few that run, and hands the ended ones back once, leaving the running ones in place.
 */

#include "DuelMgr.h"

#include <utility>

DuelMgr& DuelMgr::Instance()
{
    static DuelMgr instance;
    return instance;
}

Duel const& DuelMgr::Add(Duel duel)
{
    uint64 const id = duel.GetId();
    return _duels.insert_or_assign(id, std::move(duel)).first->second;
}

Duel const* DuelMgr::Find(uint64 duelId) const noexcept
{
    auto const found = _duels.find(duelId);
    return found == _duels.end() ? nullptr : &found->second;
}

Duel const* DuelMgr::FindByParticipant(uint64 ownerId) const noexcept
{
    for (auto const& [id, duel] : _duels)
        if (!duel.IsEnded() && duel.FindParticipant(ownerId))
            return &duel;
    return nullptr;
}

Duel const* DuelMgr::FindBySigil(uint32 mapId, uint64 sigilRow) const noexcept
{
    for (auto const& [id, duel] : _duels)
        if (!duel.IsEnded() && duel.GetMapId() == mapId && duel.GetSigilRow() == sigilRow)
            return &duel;
    return nullptr;
}

std::vector<Duel const*> DuelMgr::InMap(uint32 mapId) const
{
    std::vector<Duel const*> found;
    for (auto const& [id, duel] : _duels)
        if (duel.GetMapId() == mapId)
            found.push_back(&duel);
    return found;
}

bool DuelMgr::End(uint64 duelId, int32 winningTeam)
{
    auto const found = _duels.find(duelId);
    if (found == _duels.end() || found->second.IsEnded())
        return false;
    found->second.End(winningTeam);
    return true;
}

std::optional<uint64> DuelMgr::EndFor(uint64 ownerId, int32 winningTeam)
{
    Duel const* const duel = FindByParticipant(ownerId);
    if (!duel)
        return std::nullopt;
    uint64 const id = duel->GetId();
    End(id, winningTeam);
    return id;
}

std::vector<Duel> DuelMgr::TakeEnded()
{
    std::vector<Duel> ended;
    for (auto at = _duels.begin(); at != _duels.end();)
    {
        if (!at->second.IsEnded())
        {
            ++at;
            continue;
        }
        ended.push_back(std::move(at->second));
        at = _duels.erase(at);
    }
    return ended;
}

void DuelMgr::Clear()
{
    _duels.clear();
}
