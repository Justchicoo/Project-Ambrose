/*
 * Project Ambrose by Imjustchico
 * A lock for critical sections of a few dozen instructions that a hot path takes on every call: one atomic exchange to take it, one store to release it, and a yield while it waits, so an uncontended lock costs less than a mutex and a contended one never sleeps in the kernel.
 */

#ifndef AMBROSE_SPINLOCK_H
#define AMBROSE_SPINLOCK_H

#include <atomic>
#include <thread>

namespace Ambrose
{
    class SpinLock
    {
    public:
        SpinLock() = default;

        SpinLock(SpinLock const&) = delete;
        SpinLock& operator=(SpinLock const&) = delete;

        void lock() noexcept
        {
            while (_held.exchange(true, std::memory_order_acquire))
            {
                unsigned spins = 0;
                while (_held.load(std::memory_order_relaxed))
                {
                    if (++spins >= SpinsBeforeYield)
                    {
                        spins = 0;
                        std::this_thread::yield();
                    }
                }
            }
        }

        bool try_lock() noexcept
        {
            return !_held.exchange(true, std::memory_order_acquire);
        }

        void unlock() noexcept
        {
            _held.store(false, std::memory_order_release);
        }

    private:
        static constexpr unsigned SpinsBeforeYield = 64;

        std::atomic<bool> _held{ false };
    };
}

#endif
