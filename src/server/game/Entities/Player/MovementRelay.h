/*
 * Project Ambrose by Imjustchico
 * What other players are told of one wizard's movement at each flush: changed packed moves and teleports, movement states, the idle transition when moves stop arriving, and the standing state when its socket becomes link-dead.
 */

#ifndef AMBROSE_MOVEMENTRELAY_H
#define AMBROSE_MOVEMENTRELAY_H

#include "PlayerMovement.h"
#include "Types.h"

#include <optional>

struct MovementUpdate
{
    std::optional<PackedMove> Move;
    std::optional<PackedMove> Teleport;
    std::optional<int8> State;

    bool Empty() const noexcept { return !Move && !Teleport && !State; }
};

class MovementRelay
{
public:
    static constexpr int8 Standing = 0;

    void Reset(PlayerMovement const& movement) noexcept;
    MovementUpdate Stop() noexcept;
    MovementUpdate Take(PlayerMovement const& movement, uint32 idleFlushes);
    MovementUpdate Current(PlayerMovement const& movement) const;

private:
    int8 ShownState(PlayerMovement const& movement) const noexcept;

    uint64 _sentChanges = 0;
    int8 _sentState = Standing;
    int8 _saidState = Standing;
    uint32 _quietFlushes = 0;
    bool _idle = false;
};

#endif
