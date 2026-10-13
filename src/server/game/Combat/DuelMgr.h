/*
 * Project Ambrose by Imjustchico
 * Every duel running in the world (sDuelMgr), kept on the world thread: a duel is found by its id, by any participant or by the sigil row it stands on, and listed by instance; each is stepped once per world tick, a participant's move is put to its duel, and the events a tick gives are kept until every session has been shown them; ending one keeps it until the sessions that were shown it have been told and the world takes it away with its circle and creatures.
 */

#ifndef AMBROSE_DUELMGR_H
#define AMBROSE_DUELMGR_H

#include "Duel.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

class DuelMgr
{
public:
    static DuelMgr& Instance();

    DuelMgr() = default;
    DuelMgr(DuelMgr const&) = delete;
    DuelMgr& operator=(DuelMgr const&) = delete;

    Duel const& Add(Duel duel);
    Duel const* Find(uint64 duelId) const noexcept;
    Duel const* FindByParticipant(uint64 ownerId) const noexcept;
    Duel const* FindBySigil(uint32 mapId, uint64 sigilRow) const noexcept;
    std::vector<Duel const*> InMap(uint32 mapId) const;
    bool End(uint64 duelId, int32 winningTeam);
    std::optional<uint64> EndFor(uint64 ownerId, int32 winningTeam);
    std::vector<Duel> TakeEnded();
    void Update(uint64 nowMs, DuelRoundRules const& rules);
    std::optional<std::pair<uint64, DuelMoveResult>> Choose(uint64 ownerId, uint8 moveType);
    void ClearEvents() noexcept;
    std::size_t GetCount() const noexcept { return _duels.size(); }
    void Clear();

private:
    std::map<uint64, Duel> _duels;
};

#define sDuelMgr DuelMgr::Instance()

#endif
