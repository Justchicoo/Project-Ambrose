/*
 * Project Ambrose by Imjustchico
 * What the other players in an instance are told of one wizard's movement at each flush: its move as its client packed it, when that has changed since the last flush, and its movement state, when that has changed since it was last sent. State 1 makes a viewing client carry the wizard on along its last heading until a newer move arrives, and 0 stops it at the last move, so a wizard whose client has sent no new move for the number of flushes asked for is shown as standing, told once as state 0, even when its client never says it stopped; that is the relay's own view, kept apart from the state the client said, so the wizard's next move or the client's next state shows what the client says again.
 */

#ifndef AMBROSE_MOVEMENTRELAY_H
#define AMBROSE_MOVEMENTRELAY_H

#include "PlayerMovement.h"
#include "Types.h"

#include <optional>

struct MovementUpdate
{
    std::optional<PackedMove> Move;
    std::optional<int8> State;

    bool Empty() const noexcept { return !Move && !State; }
};

class MovementRelay
{
public:
    static constexpr int8 Standing = 0;

    void Reset(PlayerMovement const& movement) noexcept;
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
