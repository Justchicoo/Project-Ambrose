/*
 * Project Ambrose by Imjustchico
 * Steps a duel's round once per update: out of the start grace into round one, PrePlanning into Planning when its time is up, with every creature passing at once, Planning into Execution the moment every living seat has a move or the timer runs out, a seat with none passing, then Resolution and the next round one update each; the rules are read as each round begins, so a change made mid-round applies from the next one.
 */

#include "DuelRound.h"
#include "Settings.h"

#include <algorithm>

DuelRoundRules DuelRoundRules::FromSettings()
{
    DuelRoundRules rules;
    rules.StartGraceMs = uint64{ sSettings.Get<uint32>("Combat.StartGraceSeconds") } * 1000;
    rules.PrePlanningMs = uint64{ sSettings.Get<uint32>("Combat.PrePlanningSeconds") } * 1000;
    rules.PlanningMs = uint64{ sSettings.Get<uint32>("Combat.PlanningSeconds") } * 1000;
    rules.PipsPerRound = static_cast<int32>(sSettings.Get<uint32>("Combat.PipsPerRound"));
    rules.MaxPips = static_cast<int32>(sSettings.Get<uint32>("Combat.MaxPips"));
    return rules;
}

void DuelRound::Seat(uint64 ownerId, int32 team, bool isPlayer)
{
    DuelSeat seat;
    seat.OwnerId = ownerId;
    seat.Team = team;
    seat.IsPlayer = isPlayer;
    _seats.push_back(seat);
}

void DuelRound::SetOwner(std::size_t seat, uint64 ownerId) noexcept
{
    if (seat < _seats.size())
        _seats[seat].OwnerId = ownerId;
}

DuelSeat const* DuelRound::FindSeat(uint64 ownerId) const noexcept
{
    auto const found = std::find_if(_seats.begin(), _seats.end(), [ownerId](DuelSeat const& seat) { return seat.OwnerId == ownerId; });
    return found == _seats.end() ? nullptr : &*found;
}

DuelSeat* DuelRound::EditSeat(uint64 ownerId) noexcept
{
    auto const found = std::find_if(_seats.begin(), _seats.end(), [ownerId](DuelSeat const& seat) { return seat.OwnerId == ownerId; });
    return found == _seats.end() ? nullptr : &*found;
}

void DuelRound::Enter(DuelPhase phase, uint64 nowMs, uint64 lengthMs)
{
    _phase = phase;
    _deadline = nowMs + lengthMs;
    DuelEvent changed;
    changed.Kind = DuelEventKind::PhaseChanged;
    changed.Phase = phase;
    changed.Round = _round;
    _events.push_back(changed);
}

void DuelRound::BeginRound(uint64 nowMs, DuelRoundRules const& rules)
{
    _rules = rules;
    ++_round;
    for (DuelSeat& seat : _seats)
    {
        seat.Move.reset();
        if (seat.Alive)
            seat.Pips = std::clamp(seat.Pips + _rules.PipsPerRound, 0, std::max(_rules.MaxPips, 0));
    }
    DuelEvent started;
    started.Kind = DuelEventKind::RoundStarted;
    started.Round = _round;
    started.Team = _firstTeam;
    _events.push_back(started);
    Enter(DuelPhase::PrePlanning, nowMs, _rules.PrePlanningMs);
    DuelEvent pips;
    pips.Kind = DuelEventKind::PipsChanged;
    pips.Round = _round;
    _events.push_back(pips);
}

bool DuelRound::EveryoneMoved() const noexcept
{
    return std::all_of(_seats.begin(), _seats.end(), [](DuelSeat const& seat) { return !seat.Alive || seat.Move.has_value(); });
}

void DuelRound::Update(uint64 nowMs, DuelRoundRules const& rules)
{
    if (!_started)
    {
        _started = true;
        _rules = rules;
        _deadline = nowMs + rules.StartGraceMs;
    }
    switch (_phase)
    {
        case DuelPhase::Starting:
            if (nowMs >= _deadline)
                BeginRound(nowMs, rules);
            break;
        case DuelPhase::PrePlanning:
            if (nowMs < _deadline)
                break;
            for (DuelSeat& seat : _seats)
                if (seat.Alive && !seat.IsPlayer)
                    seat.Move = DuelMove::Pass;
            Enter(DuelPhase::Planning, nowMs, _rules.PlanningMs);
            {
                DuelEvent timer;
                timer.Kind = DuelEventKind::PlanningTimer;
                timer.Round = _round;
                timer.Seconds = GetPlanningSeconds();
                _events.push_back(timer);
            }
            break;
        case DuelPhase::Planning:
            if (!EveryoneMoved() && nowMs < _deadline)
                break;
            for (DuelSeat& seat : _seats)
                if (seat.Alive && !seat.Move)
                    seat.Move = DuelMove::Pass;
            Enter(DuelPhase::Execution, nowMs, 0);
            break;
        case DuelPhase::Execution:
            Enter(DuelPhase::Resolution, nowMs, 0);
            break;
        case DuelPhase::Resolution:
            BeginRound(nowMs, rules);
            break;
        default:
            break;
    }
}

DuelMoveResult DuelRound::Choose(uint64 ownerId, uint8 moveType)
{
    DuelSeat* const seat = EditSeat(ownerId);
    if (!seat || !seat->Alive)
        return DuelMoveResult::NotSeated;
    if (_phase != DuelPhase::Planning)
        return DuelMoveResult::NotPlanning;
    DuelEvent chosen;
    chosen.Kind = DuelEventKind::MoveChosen;
    chosen.Round = _round;
    chosen.OwnerId = ownerId;
    chosen.Team = seat->Team;
    if (moveType == static_cast<uint8>(DuelMove::Pass))
    {
        seat->Move = DuelMove::Pass;
        chosen.Move = DuelMove::Pass;
        _events.push_back(chosen);
        return DuelMoveResult::Chosen;
    }
    if (moveType == static_cast<uint8>(DuelMove::ChangeMind))
    {
        seat->Move.reset();
        chosen.Move = DuelMove::ChangeMind;
        _events.push_back(chosen);
        return DuelMoveResult::Withdrawn;
    }
    return DuelMoveResult::Unsupported;
}

void DuelRound::End() noexcept
{
    _phase = DuelPhase::Ended;
}
