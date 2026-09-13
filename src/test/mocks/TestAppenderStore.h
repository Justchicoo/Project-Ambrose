/*
 * Project Ambrose by Imjustchico
 * Thread-safe capture of test appender records with write gates, throw switches, write hooks and flush counters.
 */

#ifndef AMBROSE_TESTAPPENDERSTORE_H
#define AMBROSE_TESTAPPENDERSTORE_H

#include "LogMessage.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class TestAppenderStore
{
public:
    void Record(std::string const& appender, LogMessage const& message);
    void SetCountOnly(std::string const& appender);
    std::vector<LogMessage> Messages(std::string const& appender) const;
    std::vector<std::string> Texts(std::string const& appender) const;
    std::size_t Count(std::string const& appender) const;
    std::size_t TotalCount() const;
    bool WaitForCount(std::string const& appender, std::size_t count, std::chrono::milliseconds timeout) const;
    uint32 FlushCount(std::string const& appender) const;
    void NoteFlush(std::string const& appender);
    void CloseGate();
    void OpenGate();
    void WaitAtGate();
    bool WaitForGateWaiters(std::size_t count, std::chrono::milliseconds timeout) const;
    void SetThrowOnWrite(bool enabled);
    bool ShouldThrow() const;
    void SetOnWrite(std::function<void(std::string const&, LogMessage const&)> hook);
    void RunOnWrite(std::string const& appender, LogMessage const& message);
    void Clear();

private:
    mutable std::mutex _mutex;
    mutable std::condition_variable _changed;
    std::condition_variable _gate;
    bool _gateClosed = false;
    std::size_t _gateWaiters = 0;
    bool _throw = false;
    std::function<void(std::string const&, LogMessage const&)> _onWrite;
    std::map<std::string, std::vector<LogMessage>> _messages;
    std::map<std::string, std::size_t> _counts;
    std::set<std::string> _countOnly;
    std::map<std::string, uint32> _flushes;
};

#endif
