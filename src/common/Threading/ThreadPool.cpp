/*
 * Project Ambrose by Imjustchico
 * Starts named threads that run the pool's io_context, keeps them running past task exceptions, and joins them once.
 */

#include "ThreadPool.h"
#include "Log.h"
#include "ThreadName.h"

#include <fmt/format.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>

ThreadPool::ThreadPool(std::size_t threadCount, std::string name)
    : _context(static_cast<int>(std::clamp<std::size_t>(threadCount, 1, 1024))), _work(std::in_place, _context.get_executor()), _name(std::move(name))
{
    std::size_t const count = std::clamp<std::size_t>(threadCount, 1, 1024);
    _threads.reserve(count);
    _threadIds.reserve(count);
    try
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            _threads.emplace_back([this, i] { Run(i); });
            _threadIds.push_back(_threads.back().get_id());
        }
    }
    catch (...)
    {
        _work.reset();
        _context.stop();
        for (std::thread& thread : _threads)
            thread.join();
        throw;
    }
}

ThreadPool::~ThreadPool()
{
    if (IsPoolThread())
    {
        std::fputs("ThreadPool destroyed from one of its own threads; post the destruction to another thread\n", stderr);
        std::abort();
    }
    Join();
}

void ThreadPool::Run(std::size_t index)
{
    Ambrose::Threading::SetCurrentThreadName(fmt::format("{}-{}", _name, index));
    while (true)
    {
        try
        {
            _context.run();
            return;
        }
        catch (std::exception const& exception)
        {
            LOG_ERROR("server.threading", "task in thread pool '{}' threw: {}", _name, exception.what());
        }
        catch (...)
        {
            LOG_ERROR("server.threading", "task in thread pool '{}' threw an unknown exception", _name);
        }
    }
}

void ThreadPool::Stop()
{
    _context.stop();
}

void ThreadPool::Join()
{
    if (IsPoolThread())
        return;
    std::lock_guard lock(_joinMutex);
    if (_joined)
        return;
    _work.reset();
    for (std::thread& thread : _threads)
        if (thread.joinable())
            thread.join();
    _joined = true;
}

std::size_t ThreadPool::GetThreadCount() const noexcept
{
    return _threads.size();
}

std::string const& ThreadPool::GetName() const noexcept
{
    return _name;
}

ThreadPool::Executor ThreadPool::GetExecutor() noexcept
{
    return _context.get_executor();
}

bool ThreadPool::IsPoolThread() const noexcept
{
    return std::find(_threadIds.begin(), _threadIds.end(), std::this_thread::get_id()) != _threadIds.end();
}

std::size_t ThreadPool::DefaultThreadCount() noexcept
{
    return std::max<unsigned>(std::thread::hardware_concurrency(), 1u);
}
