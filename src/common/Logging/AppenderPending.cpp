/*
 * Project Ambrose by Imjustchico
 * Buffers records for an unregistered appender type and replays them in order into its real appender.
 */

#include "AppenderPending.h"

#include <utility>

AppenderPending::AppenderPending(AppenderDefinition const& definition, std::size_t capacity)
    : Appender(definition.Name, definition.Type, definition.Level, definition.Flags), _reuseKey(definition.ReuseKey()), _capacity(capacity)
{
}

std::string const& AppenderPending::GetReuseKey() const noexcept
{
    return _reuseKey;
}

std::size_t AppenderPending::GetBufferedCount() const
{
    std::lock_guard lock(_mutex);
    return _buffer.size();
}

uint64 AppenderPending::GetDroppedCount() const
{
    std::lock_guard lock(_mutex);
    return _dropped;
}

bool AppenderPending::HasSuccessor() const
{
    std::lock_guard lock(_mutex);
    return _successor != nullptr;
}

Appender const* AppenderPending::GetSink() const noexcept
{
    Appender const* const successor = _successorSink.load(std::memory_order_acquire);
    return successor != nullptr ? successor : this;
}

void AppenderPending::HandOver(std::shared_ptr<Appender> successor)
{
    if (!successor)
        return;
    std::deque<LogMessage> replay;
    while (true)
    {
        {
            std::lock_guard lock(_mutex);
            if (_buffer.empty())
            {
                _successorSink.store(successor.get(), std::memory_order_release);
                _successor = std::move(successor);
                return;
            }
            replay.swap(_buffer);
        }
        for (LogMessage const& message : replay)
            if (successor->Accepts(message))
                successor->Write(message);
        replay.clear();
    }
}

void AppenderPending::Flush()
{
    std::shared_ptr<Appender> successor;
    {
        std::lock_guard lock(_mutex);
        successor = _successor;
    }
    if (successor)
        successor->Flush();
}

void AppenderPending::WriteMessage(LogMessage const& message)
{
    std::shared_ptr<Appender> successor;
    {
        std::lock_guard lock(_mutex);
        if (!_successor)
        {
            if (_capacity == 0)
            {
                ++_dropped;
                return;
            }
            if (_buffer.size() >= _capacity)
            {
                _buffer.pop_front();
                ++_dropped;
            }
            _buffer.push_back(message);
            return;
        }
        successor = _successor;
    }
    if (successor->Accepts(message))
        successor->Write(message);
}
