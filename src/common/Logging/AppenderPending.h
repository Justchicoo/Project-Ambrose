/*
 * Project Ambrose by Imjustchico
 * Stand-in for an appender whose type is not registered yet, buffering lines and replaying them on registration.
 */

#ifndef AMBROSE_APPENDERPENDING_H
#define AMBROSE_APPENDERPENDING_H

#include "Appender.h"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

class AppenderPending : public Appender
{
public:
    AppenderPending(AppenderDefinition const& definition, std::size_t capacity);

    std::string const& GetReuseKey() const noexcept;
    std::size_t GetBufferedCount() const;
    uint64 GetDroppedCount() const;
    bool HasSuccessor() const;
    Appender const* GetSink() const noexcept override;
    void HandOver(std::shared_ptr<Appender> successor);
    void Flush() override;

protected:
    void WriteMessage(LogMessage const& message) override;

private:
    std::string _reuseKey;
    mutable std::mutex _mutex;
    std::deque<LogMessage> _buffer;
    std::size_t _capacity;
    uint64 _dropped = 0;
    std::shared_ptr<Appender> _successor;
    std::atomic<Appender const*> _successorSink{ nullptr };
};

#endif
