/*
 * Project Ambrose by Imjustchico
 * Steady-clock timer on an io_context that runs a handler at a deadline and can be moved or cancelled.
 */

#ifndef AMBROSE_DEADLINETIMER_H
#define AMBROSE_DEADLINETIMER_H

#include "IoContext.h"
#include "Strand.h"
#include "Types.h"

#include <asio/bind_executor.hpp>
#include <asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <system_error>
#include <utility>

namespace Ambrose::Asio
{
    class DeadlineTimer
    {
    public:
        using Clock = std::chrono::steady_clock;

        explicit DeadlineTimer(IoContext& context) : _timer(context.GetImpl())
        {
        }

        DeadlineTimer(IoContext& context, Clock::duration expiresAfter) : _timer(context.GetImpl(), expiresAfter)
        {
        }

        DeadlineTimer(DeadlineTimer const&) = delete;
        DeadlineTimer& operator=(DeadlineTimer const&) = delete;

        std::size_t ExpiresAfter(Clock::duration duration)
        {
            _generation->fetch_add(1, std::memory_order_relaxed);
            return _timer.expires_after(duration);
        }

        std::size_t ExpiresAt(Clock::time_point time)
        {
            _generation->fetch_add(1, std::memory_order_relaxed);
            return _timer.expires_at(time);
        }

        std::size_t Cancel()
        {
            _generation->fetch_add(1, std::memory_order_relaxed);
            return _timer.cancel();
        }

        Clock::time_point GetExpiry() const { return _timer.expiry(); }
        asio::steady_timer& GetImpl() noexcept { return _timer; }

        template<typename Handler>
        void AsyncWait(Handler&& handler)
        {
            _timer.async_wait(std::forward<Handler>(handler));
        }

        template<typename Handler>
        void AsyncWait(Strand const& strand, Handler&& handler)
        {
            _timer.async_wait(asio::bind_executor(strand, std::forward<Handler>(handler)));
        }

        template<typename Function>
        void OnExpiry(Function&& function)
        {
            std::shared_ptr<std::atomic<uint64>> const generation = _generation;
            uint64 const armed = generation->load(std::memory_order_relaxed);
            _timer.async_wait([generation, armed, function = std::forward<Function>(function)](std::error_code const& error) mutable
            {
                if (!error && generation->load(std::memory_order_relaxed) == armed)
                    function();
            });
        }

    private:
        asio::steady_timer _timer;
        std::shared_ptr<std::atomic<uint64>> _generation = std::make_shared<std::atomic<uint64>>(0);
    };
}

#endif
