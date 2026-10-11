/*
 * Project Ambrose by Imjustchico
 * The round a duel is in, kept apart from the objects the client draws it with so a test can run it headless: the start grace, then each round's PrePlanning, Planning, Execution and Resolution in turn, driven by the world tick's clock rather than a thread, with the timings and pips each round takes from the rules in force as it begins, a generic pip for every living seat at the start of each round up to the most a seat may hold, the move each seat has chosen in Planning, which ends the moment every living seat has one or its timer runs out, and the events each step gives the wizards that watch it.
 */

#ifndef AMBROSE_DUELROUND_H
#define AMBROSE_DUELROUND_H

#include "Types.h"

#include <optional>
#include <vector>

enum class DuelPhase : uint8
{
    Starting = 0,
    PrePlanning = 1,
    Planning = 2,
    PreExecution = 3,
    Execution = 4,
    Resolution = 5,
    Victory = 6,
    Ended = 7
};

enum class DuelMove : uint8
{
    Cast = 0,
    Flee = 1,
    Discard = 2,
    Pass = 3,
    ChangeMind = 4
};

enum class DuelMoveResult : uint8
{
    Chosen,
    Withdrawn,
    NotPlanning,
    NotSeated,
    Unsupported
};

struct DuelRoundRules
{
    uint64 StartGraceMs = 3000;
    uint64 PrePlanningMs = 2000;
    uint64 PlanningMs = 30000;
    int32 PipsPerRound = 1;
    int32 MaxPips = 7;

    static DuelRoundRules FromSettings();
};

struct DuelSeat
{
    uint64 OwnerId = 0;
    int32 Team = 0;
    bool IsPlayer = false;
    bool Alive = true;
    int32 Pips = 0;
    std::optional<DuelMove> Move;
};

enum class DuelEventKind : uint8
{
    RoundStarted,
    PhaseChanged,
    PipsChanged,
    PlanningTimer,
    MoveChosen
};

struct DuelEvent
{
    DuelEventKind Kind = DuelEventKind::PhaseChanged;
    DuelPhase Phase = DuelPhase::Starting;
    int32 Round = 0;
    uint32 Seconds = 0;
    uint64 OwnerId = 0;
    int32 Team = 0;
    DuelMove Move = DuelMove::Pass;
};

class DuelRound
{
public:
    void Seat(uint64 ownerId, int32 team, bool isPlayer);
    void SetOwner(std::size_t seat, uint64 ownerId) noexcept;
    void Update(uint64 nowMs, DuelRoundRules const& rules);
    DuelMoveResult Choose(uint64 ownerId, uint8 moveType);
    void End() noexcept;

    DuelPhase GetPhase() const noexcept { return _phase; }
    int32 GetRound() const noexcept { return _round; }
    int32 GetFirstTeam() const noexcept { return _firstTeam; }
    uint64 GetDeadline() const noexcept { return _deadline; }
    uint32 GetPlanningSeconds() const noexcept { return static_cast<uint32>(_rules.PlanningMs / 1000); }
    std::vector<DuelSeat> const& GetSeats() const noexcept { return _seats; }
    DuelSeat const* FindSeat(uint64 ownerId) const noexcept;
    std::vector<DuelEvent> const& GetEvents() const noexcept { return _events; }
    void ClearEvents() noexcept { _events.clear(); }

private:
    DuelSeat* EditSeat(uint64 ownerId) noexcept;
    void Enter(DuelPhase phase, uint64 nowMs, uint64 lengthMs);
    void BeginRound(uint64 nowMs, DuelRoundRules const& rules);
    bool EveryoneMoved() const noexcept;

    std::vector<DuelSeat> _seats;
    std::vector<DuelEvent> _events;
    DuelRoundRules _rules;
    DuelPhase _phase = DuelPhase::Starting;
    int32 _round = 0;
    int32 _firstTeam = 0;
    bool _started = false;
    uint64 _deadline = 0;
};

#endif
