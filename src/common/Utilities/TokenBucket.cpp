/*
 * Project Ambrose by Imjustchico
 * Refills tokens by elapsed time up to capacity and consumes them on request.
 */

#include "TokenBucket.h"

#include <algorithm>
#include <utility>

TokenBucket::TokenBucket(uint32 capacity, double tokensPerSecond, TimeSource timeSource)
    : _timeSource(std::move(timeSource)),
      _capacity(capacity),
      _tokensPerSecond(tokensPerSecond),
      _tokens(static_cast<double>(capacity)),
      _lastRefill(_timeSource())
{
}

bool TokenBucket::TryConsume(uint32 tokens)
{
    Refill();
    if (_tokens < static_cast<double>(tokens))
        return false;
    _tokens -= static_cast<double>(tokens);
    return true;
}

double TokenBucket::GetAvailableTokens()
{
    Refill();
    return _tokens;
}

uint32 TokenBucket::GetCapacity() const
{
    return _capacity;
}

void TokenBucket::Refill()
{
    Clock::time_point const now = _timeSource();
    if (now <= _lastRefill)
        return;
    double const elapsedSeconds = std::chrono::duration<double>(now - _lastRefill).count();
    _tokens = std::min(static_cast<double>(_capacity), _tokens + elapsedSeconds * _tokensPerSecond);
    _lastRefill = now;
}
