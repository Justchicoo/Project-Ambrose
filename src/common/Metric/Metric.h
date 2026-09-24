/*
 * Project Ambrose by Imjustchico
 * The three things a running server counts: a counter that only ever goes up, a gauge that moves both ways, and a histogram that says how long something took by keeping how many fell into each bucket. Every update is one relaxed atomic operation on a value the caller already holds a reference to, because these sit in the hot path of message handling and a tick, and a measurement that costs more than the thing it measures is not worth taking. Reading them is exact rather than instantaneous: a reader may see one field updated and the next not, which is what a metric scrape is expected to tolerate.
 */

#ifndef AMBROSE_METRIC_H
#define AMBROSE_METRIC_H

#include "Types.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <span>

namespace Ambrose
{
    class Counter
    {
    public:
        void Add(uint64 amount = 1) noexcept { _value.fetch_add(amount, std::memory_order_relaxed); }
        uint64 Value() const noexcept { return _value.load(std::memory_order_relaxed); }
        void Reset() noexcept { _value.store(0, std::memory_order_relaxed); }

    private:
        std::atomic<uint64> _value{ 0 };
    };

    class Gauge
    {
    public:
        void Set(int64 value) noexcept { _value.store(value, std::memory_order_relaxed); }
        void Add(int64 amount) noexcept { _value.fetch_add(amount, std::memory_order_relaxed); }
        void Subtract(int64 amount) noexcept { _value.fetch_sub(amount, std::memory_order_relaxed); }
        int64 Value() const noexcept { return _value.load(std::memory_order_relaxed); }

    private:
        std::atomic<int64> _value{ 0 };
    };

    class Histogram
    {
    public:
        static constexpr std::size_t BucketCount = 11;

        static constexpr std::array<double, BucketCount> Bounds{ 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0 };

        void Observe(double seconds) noexcept
        {
            for (std::size_t bucket = 0; bucket < BucketCount; ++bucket)
                if (seconds <= Bounds[bucket])
                {
                    _buckets[bucket].fetch_add(1, std::memory_order_relaxed);
                    break;
                }
            _count.fetch_add(1, std::memory_order_relaxed);
            _sum.fetch_add(seconds, std::memory_order_relaxed);
        }

        uint64 Count() const noexcept { return _count.load(std::memory_order_relaxed); }
        double Sum() const noexcept { return _sum.load(std::memory_order_relaxed); }

        uint64 Cumulative(std::size_t bucket) const noexcept
        {
            uint64 total = 0;
            for (std::size_t index = 0; index <= bucket && index < BucketCount; ++index)
                total += _buckets[index].load(std::memory_order_relaxed);
            return total;
        }

    private:
        std::array<std::atomic<uint64>, BucketCount> _buckets{};
        std::atomic<uint64> _count{ 0 };
        std::atomic<double> _sum{ 0.0 };
    };
}

#endif
