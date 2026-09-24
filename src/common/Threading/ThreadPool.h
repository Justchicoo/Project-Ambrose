/*
 * Project Ambrose by Imjustchico
 * Fixed-size pool of named threads running one asio::io_context, for posting work and waiting for it to finish.
 */

#ifndef AMBROSE_THREADPOOL_H
#define AMBROSE_THREADPOOL_H

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool
{
public:
    using Executor = asio::io_context::executor_type;

    explicit ThreadPool(std::size_t threadCount, std::string name = "pool");
    ~ThreadPool();

    ThreadPool(ThreadPool const&) = delete;
    ThreadPool& operator=(ThreadPool const&) = delete;

    template<typename Function>
    void Post(Function&& function)
    {
        asio::post(_context, std::forward<Function>(function));
    }

    void Stop();
    void Join();
    std::size_t GetThreadCount() const noexcept;
    std::string const& GetName() const noexcept;
    Executor GetExecutor() noexcept;
    bool IsPoolThread() const noexcept;

    static std::size_t DefaultThreadCount() noexcept;

private:
    void Run(std::size_t index);

    asio::io_context _context;
    std::optional<asio::executor_work_guard<Executor>> _work;
    std::string _name;
    std::vector<std::thread> _threads;
    std::vector<std::thread::id> _threadIds;
    std::mutex _joinMutex;
    bool _joined = false;
};

#endif
