/*
 * Project Ambrose by Imjustchico
 * A headless duel for tests: a DuelRound driven by a clock the test owns, one fixed-length tick at a time, with scripted participants that choose their moves through an injectable random source, and every event and phase change each tick gave recorded with the tick and time it happened at.
 */

#ifndef AMBROSE_DUELSIMHARNESS_H
#define AMBROSE_DUELSIMHARNESS_H

#include "DuelRound.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <vector>

class DuelSimHarness
{
public:
    using Random = std::function<uint32(uint32 bound)>;
    using Script = std::function<std::optional<uint8>(DuelSimHarness const& sim, DuelSeat const& seat, Random const& random)>;
    using RulesSource = std::function<DuelRoundRules()>;

    struct Record
    {
        uint64 Tick = 0;
        uint64 TimeMs = 0;
        DuelEvent Event;
    };

    struct Choice
    {
        uint64 Tick = 0;
        uint64 OwnerId = 0;
        uint8 MoveType = 0;
        DuelMoveResult Result = DuelMoveResult::NotSeated;
    };

    explicit DuelSimHarness(DuelRoundRules rules, uint64 tickMs = 100, uint32 seed = 1)
        : _rules([rules] { return rules; }), _tickMs(tickMs), _engine(seed)
    {
        _random = [this](uint32 bound) { return bound == 0 ? 0u : std::uniform_int_distribution<uint32>(0, bound - 1)(_engine); };
    }

    void SetRules(RulesSource rules) { _rules = std::move(rules); }
    void SetRandom(Random random) { _random = std::move(random); }

    void AddPlayer(uint64 ownerId, Script script)
    {
        _round.Seat(ownerId, 0, true);
        _scripts[ownerId] = std::move(script);
    }

    void AddCreature(uint64 ownerId)
    {
        _round.Seat(ownerId, 1, false);
    }

    void Tick()
    {
        ++_tick;
        _nowMs += _tickMs;
        if (_round.GetPhase() == DuelPhase::Planning)
            for (DuelSeat const& seat : _round.GetSeats())
            {
                auto const script = _scripts.find(seat.OwnerId);
                if (script == _scripts.end() || seat.Move)
                    continue;
                std::optional<uint8> const move = script->second(*this, seat, _random);
                if (move)
                    _choices.push_back({ _tick, seat.OwnerId, *move, _round.Choose(seat.OwnerId, *move) });
            }
        _round.Update(_nowMs, _rules());
        for (DuelEvent const& event : _round.GetEvents())
            _records.push_back({ _tick, _nowMs, event });
        _round.ClearEvents();
    }

    bool RunUntil(std::function<bool(DuelSimHarness const&)> const& done, uint64 maxTicks = 100000)
    {
        for (uint64 ticks = 0; ticks < maxTicks; ++ticks)
        {
            if (done(*this))
                return true;
            Tick();
        }
        return done(*this);
    }

    DuelRound& GetRound() noexcept { return _round; }
    DuelRound const& GetRound() const noexcept { return _round; }
    uint64 GetTick() const noexcept { return _tick; }
    uint64 GetNowMs() const noexcept { return _nowMs; }
    uint64 GetTickMs() const noexcept { return _tickMs; }
    std::vector<Record> const& GetRecords() const noexcept { return _records; }
    std::vector<Choice> const& GetChoices() const noexcept { return _choices; }

    std::vector<Record> RecordsOf(DuelEventKind kind) const
    {
        std::vector<Record> found;
        for (Record const& record : _records)
            if (record.Event.Kind == kind)
                found.push_back(record);
        return found;
    }

    std::optional<Record> FirstPhase(DuelPhase phase, int32 round) const
    {
        for (Record const& record : _records)
            if (record.Event.Kind == DuelEventKind::PhaseChanged && record.Event.Phase == phase && record.Event.Round == round)
                return record;
        return std::nullopt;
    }

    static Script PassAfter(uint32 ticks)
    {
        return [ticks, waited = std::make_shared<std::map<int32, uint32>>()](DuelSimHarness const& sim, DuelSeat const&, Random const&) mutable -> std::optional<uint8>
        {
            uint32& count = (*waited)[sim.GetRound().GetRound()];
            if (count++ < ticks)
                return std::nullopt;
            return static_cast<uint8>(DuelMove::Pass);
        };
    }

    static Script PassAtRandom(uint32 withinTicks)
    {
        return [withinTicks, plans = std::make_shared<std::map<int32, uint32>>()](DuelSimHarness const& sim, DuelSeat const&, Random const& random) mutable -> std::optional<uint8>
        {
            int32 const round = sim.GetRound().GetRound();
            auto planned = plans->find(round);
            if (planned == plans->end())
                planned = plans->emplace(round, random(withinTicks)).first;
            if (planned->second > 0)
            {
                --planned->second;
                return std::nullopt;
            }
            return static_cast<uint8>(DuelMove::Pass);
        };
    }

    static Script Never()
    {
        return [](DuelSimHarness const&, DuelSeat const&, Random const&) -> std::optional<uint8> { return std::nullopt; };
    }

private:
    DuelRound _round;
    RulesSource _rules;
    uint64 _tickMs = 100;
    uint64 _tick = 0;
    uint64 _nowMs = 0;
    std::mt19937 _engine;
    Random _random;
    std::map<uint64, Script> _scripts;
    std::vector<Record> _records;
    std::vector<Choice> _choices;
};

#endif
