/*
 * Project Ambrose by Imjustchico
 * A console input for setup prompt tests that hands out scripted answer lines, then either closes or waits to be interrupted, counting reads and interrupts.
 */

#ifndef AMBROSE_SCRIPTEDPROMPTINPUT_H
#define AMBROSE_SCRIPTEDPROMPTINPUT_H

#include "ConsoleInput.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class ScriptedPromptInput final : public ConsoleInput
{
public:
    struct Counters
    {
        std::atomic<int> Reads{ 0 };
        std::atomic<int> Interrupts{ 0 };
    };

    ScriptedPromptInput(std::vector<std::string> lines, bool closeAtEnd, std::shared_ptr<Counters> counters)
        : _lines(std::move(lines)), _closeAtEnd(closeAtEnd), _counters(std::move(counters))
    {
    }

    ReadResult ReadLine(std::string& line, std::chrono::milliseconds timeout) override
    {
        std::unique_lock lock(_mutex);
        if (_interrupted)
            return ReadResult::Closed;
        if (_next < _lines.size())
        {
            ++_counters->Reads;
            line = _lines[_next++];
            return ReadResult::Line;
        }
        if (_closeAtEnd)
            return ReadResult::Closed;
        _wake.wait_for(lock, timeout, [this] { return _interrupted; });
        return _interrupted ? ReadResult::Closed : ReadResult::Timeout;
    }

    void Interrupt() override
    {
        std::lock_guard const lock(_mutex);
        _interrupted = true;
        ++_counters->Interrupts;
        _wake.notify_all();
    }

private:
    std::vector<std::string> _lines;
    std::size_t _next = 0;
    bool _closeAtEnd;
    bool _interrupted = false;
    std::shared_ptr<Counters> _counters;
    std::mutex _mutex;
    std::condition_variable _wake;
};

#endif
