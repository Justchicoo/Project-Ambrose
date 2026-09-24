/*
 * Project Ambrose by Imjustchico
 * Turns a fragmented TCP byte stream into whole KI frames, enforcing live-adjustable size limits before buffering a frame and stopping at the first protocol error.
 */

#ifndef AMBROSE_FRAMEREASSEMBLER_H
#define AMBROSE_FRAMEREASSEMBLER_H

#include "Frame.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

class FrameReassembler
{
public:
    explicit FrameReassembler(FrameLimits limits = FrameLimits());

    void Feed(std::span<uint8 const> bytes);
    std::optional<Frame> Next();
    void Reset();
    void SetLimits(FrameLimits limits) noexcept { _limits = limits; }

    FrameError GetError() const noexcept { return _error; }
    bool HasError() const noexcept { return _error != FrameError::None; }
    std::size_t GetBufferedSize() const noexcept { return _buffer.size() - _offset; }
    std::size_t GetBufferCapacity() const noexcept { return _buffer.capacity(); }
    FrameLimits const& GetLimits() const noexcept { return _limits; }

private:
    void Compact();

    static constexpr std::size_t RetainedCapacity = std::size_t{ 64 } << 10;

    FrameLimits _limits;
    std::optional<uint64> _pendingSize;
    std::vector<uint8> _buffer;
    std::size_t _offset = 0;
    FrameError _error = FrameError::None;
};

#endif
