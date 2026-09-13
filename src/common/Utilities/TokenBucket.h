/*
 * Project Ambrose by Imjustchico
 * Token bucket rate limiter with a replaceable time source for deterministic tests.
 */

#ifndef AMBROSE_TOKENBUCKET_H
#define AMBROSE_TOKENBUCKET_H

#include "Types.h"

#include <chrono>
#include <functional>

class TokenBucket
{
public:
    using Clock = std::chrono::steady_clock;
    using TimeSource = std::function<Clock::time_point()>;

    TokenBucket(uint32 capacity, double tokensPerSecond, TimeSource timeSource = [] { return Clock::now(); });

    bool TryConsume(uint32 tokens = 1);
    double GetAvailableTokens();
    uint32 GetCapacity() const;

private:
    void Refill();

    TimeSource _timeSource;
    uint32 _capacity;
    double _tokensPerSecond;
    double _tokens;
    Clock::time_point _lastRefill;
};

#endif
