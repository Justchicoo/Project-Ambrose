/*
 * Project Ambrose by Imjustchico
 * Polls the input in short reads on a shared state the thread keeps alive, delivers lines under a lock that Stop takes before interrupting, and detaches a reader that does not finish within the join timeout.
 */

#include "ConsoleReader.h"
#include "ConsoleInput.h"

#include <condition_variable>
#include <mutex>

struct ConsoleReader::State
{
    std::unique_ptr<ConsoleInput> Input;
    LineHandler OnLine;
    ClosedHandler OnClosed;
    std::mutex Mutex;
    std::condition_variable Finished;
    bool Stopping = false;
    bool Done = false;
};

ConsoleReader::ConsoleReader(std::unique_ptr<ConsoleInput> input, LineHandler onLine, ClosedHandler onClosed)
    : _state(std::make_shared<State>())
{
    _state->Input = std::move(input);
    _state->OnLine = std::move(onLine);
    _state->OnClosed = std::move(onClosed);
}

ConsoleReader::~ConsoleReader()
{
    Stop();
}

void ConsoleReader::Start()
{
    if (_thread.joinable() || !_state->Input)
        return;
    _thread = std::thread([state = _state]
    {
        std::string line;
        for (;;)
        {
            ConsoleInput::ReadResult const result = state->Input->ReadLine(line, std::chrono::milliseconds(200));
            std::lock_guard const lock(state->Mutex);
            if (state->Stopping)
                break;
            if (result == ConsoleInput::ReadResult::Line)
            {
                if (state->OnLine)
                    state->OnLine(std::move(line));
                line.clear();
            }
            else if (result == ConsoleInput::ReadResult::Closed)
            {
                if (state->OnClosed)
                    state->OnClosed();
                break;
            }
        }
        std::lock_guard const lock(state->Mutex);
        state->Done = true;
        state->Finished.notify_all();
    });
}

void ConsoleReader::Stop(std::chrono::milliseconds joinTimeout)
{
    {
        std::lock_guard const lock(_state->Mutex);
        _state->Stopping = true;
    }
    if (!_thread.joinable())
        return;
    if (_state->Input)
        _state->Input->Interrupt();
    std::unique_lock lock(_state->Mutex);
    if (_state->Finished.wait_for(lock, joinTimeout, [this] { return _state->Done; }))
    {
        lock.unlock();
        _thread.join();
    }
    else
    {
        lock.unlock();
        _thread.detach();
    }
}
