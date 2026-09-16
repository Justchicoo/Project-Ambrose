/*
 * Project Ambrose by Imjustchico
 * A thread that reads console lines and hands each one to a callback until it is stopped or the input closes, never calling back once Stop has begun.
 */

#ifndef AMBROSE_CONSOLEREADER_H
#define AMBROSE_CONSOLEREADER_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

class ConsoleInput;

class ConsoleReader
{
public:
    using LineHandler = std::function<void(std::string line)>;
    using ClosedHandler = std::function<void()>;

    ConsoleReader(std::unique_ptr<ConsoleInput> input, LineHandler onLine, ClosedHandler onClosed);
    ~ConsoleReader();
    ConsoleReader(ConsoleReader const&) = delete;
    ConsoleReader& operator=(ConsoleReader const&) = delete;

    void Start();
    void Stop(std::chrono::milliseconds joinTimeout = std::chrono::seconds(2));

private:
    struct State;

    std::shared_ptr<State> _state;
    std::thread _thread;
};

#endif
