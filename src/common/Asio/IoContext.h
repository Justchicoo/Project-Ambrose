/*
 * Project Ambrose by Imjustchico
 * Thin wrapper over asio::io_context with run and stop helpers, work guards, and post and dispatch functions.
 */

#ifndef AMBROSE_IOCONTEXT_H
#define AMBROSE_IOCONTEXT_H

#include <asio/dispatch.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#include <chrono>
#include <cstddef>
#include <utility>

namespace Ambrose::Asio
{
    class IoContext
    {
    public:
        using Executor = asio::io_context::executor_type;
        using WorkGuard = asio::executor_work_guard<Executor>;

        IoContext() = default;

        explicit IoContext(int concurrencyHint) : _impl(concurrencyHint)
        {
        }

        IoContext(IoContext const&) = delete;
        IoContext& operator=(IoContext const&) = delete;

        operator asio::io_context&() noexcept { return _impl; }
        asio::io_context& GetImpl() noexcept { return _impl; }
        Executor GetExecutor() noexcept { return _impl.get_executor(); }

        std::size_t Run() { return _impl.run(); }
        std::size_t RunOne() { return _impl.run_one(); }
        std::size_t Poll() { return _impl.poll(); }
        std::size_t PollOne() { return _impl.poll_one(); }

        template<typename Rep, typename Period>
        std::size_t RunFor(std::chrono::duration<Rep, Period> duration)
        {
            return _impl.run_for(duration);
        }

        void Stop() { _impl.stop(); }
        bool IsStopped() const { return _impl.stopped(); }
        void Restart() { _impl.restart(); }
        WorkGuard MakeWorkGuard() { return WorkGuard(_impl.get_executor()); }

    private:
        asio::io_context _impl;
    };

    template<typename Handler>
    void Post(IoContext& context, Handler&& handler)
    {
        asio::post(context.GetExecutor(), std::forward<Handler>(handler));
    }

    template<typename Handler>
    void Dispatch(IoContext& context, Handler&& handler)
    {
        asio::dispatch(context.GetExecutor(), std::forward<Handler>(handler));
    }
}

#endif
