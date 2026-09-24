/*
 * Project Ambrose by Imjustchico
 * Records test appender writes and flushes, and blocks or throws on demand for logging tests.
 */

#include "TestAppenderStore.h"

void TestAppenderStore::Record(std::string const& appender, LogMessage const& message)
{
    {
        std::lock_guard lock(_mutex);
        ++_counts[appender];
        if (!_countOnly.contains(appender))
            _messages[appender].push_back(message);
    }
    _changed.notify_all();
}

void TestAppenderStore::SetCountOnly(std::string const& appender)
{
    std::lock_guard lock(_mutex);
    _countOnly.insert(appender);
}

std::vector<LogMessage> TestAppenderStore::Messages(std::string const& appender) const
{
    std::lock_guard lock(_mutex);
    auto const it = _messages.find(appender);
    return it == _messages.end() ? std::vector<LogMessage>() : it->second;
}

std::vector<std::string> TestAppenderStore::Texts(std::string const& appender) const
{
    std::vector<std::string> texts;
    for (LogMessage const& message : Messages(appender))
        texts.push_back(message.Text);
    return texts;
}

std::size_t TestAppenderStore::Count(std::string const& appender) const
{
    std::lock_guard lock(_mutex);
    auto const it = _counts.find(appender);
    return it == _counts.end() ? 0 : it->second;
}

std::size_t TestAppenderStore::TotalCount() const
{
    std::lock_guard lock(_mutex);
    std::size_t total = 0;
    for (auto const& [name, count] : _counts)
        total += count;
    return total;
}

bool TestAppenderStore::WaitForCount(std::string const& appender, std::size_t count, std::chrono::milliseconds timeout) const
{
    std::unique_lock lock(_mutex);
    return _changed.wait_for(lock, timeout, [&]
    {
        auto const it = _counts.find(appender);
        return it != _counts.end() && it->second >= count;
    });
}

uint32 TestAppenderStore::FlushCount(std::string const& appender) const
{
    std::lock_guard lock(_mutex);
    auto const it = _flushes.find(appender);
    return it == _flushes.end() ? 0 : it->second;
}

void TestAppenderStore::NoteFlush(std::string const& appender)
{
    std::lock_guard lock(_mutex);
    ++_flushes[appender];
}

void TestAppenderStore::CloseGate()
{
    std::lock_guard lock(_mutex);
    _gateClosed = true;
}

void TestAppenderStore::OpenGate()
{
    {
        std::lock_guard lock(_mutex);
        _gateClosed = false;
    }
    _gate.notify_all();
}

void TestAppenderStore::WaitAtGate()
{
    std::unique_lock lock(_mutex);
    if (!_gateClosed)
        return;
    ++_gateWaiters;
    _changed.notify_all();
    _gate.wait(lock, [this] { return !_gateClosed; });
    --_gateWaiters;
}

bool TestAppenderStore::WaitForGateWaiters(std::size_t count, std::chrono::milliseconds timeout) const
{
    std::unique_lock lock(_mutex);
    return _changed.wait_for(lock, timeout, [&] { return _gateWaiters >= count; });
}

void TestAppenderStore::SetThrowOnWrite(bool enabled)
{
    std::lock_guard lock(_mutex);
    _throw = enabled;
}

bool TestAppenderStore::ShouldThrow() const
{
    std::lock_guard lock(_mutex);
    return _throw;
}

void TestAppenderStore::SetOnWrite(std::function<void(std::string const&, LogMessage const&)> hook)
{
    std::lock_guard lock(_mutex);
    _onWrite = std::move(hook);
}

void TestAppenderStore::RunOnWrite(std::string const& appender, LogMessage const& message)
{
    std::function<void(std::string const&, LogMessage const&)> hook;
    {
        std::lock_guard lock(_mutex);
        hook = _onWrite;
    }
    if (hook)
        hook(appender, message);
}

void TestAppenderStore::Clear()
{
    std::lock_guard lock(_mutex);
    _messages.clear();
    _counts.clear();
    _flushes.clear();
}
