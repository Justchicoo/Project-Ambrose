/*
 * Project Ambrose by Imjustchico
 * Line sources for the console: the interface a reader thread polls, and the process's standard input as UTF-8 lines that a shutdown can interrupt, with over-long lines discarded instead of buffered.
 */

#ifndef AMBROSE_CONSOLEINPUT_H
#define AMBROSE_CONSOLEINPUT_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

class ConsoleInput
{
public:
    enum class ReadResult
    {
        Line,
        Timeout,
        Closed
    };

    virtual ~ConsoleInput() = default;

    virtual ReadResult ReadLine(std::string& line, std::chrono::milliseconds timeout) = 0;
    virtual void Interrupt() = 0;
};

class StandardConsoleInput : public ConsoleInput
{
public:
    static constexpr std::size_t MaxPendingLine = std::size_t{ 64 } << 10;

    StandardConsoleInput();
    ~StandardConsoleInput() override;

    ReadResult ReadLine(std::string& line, std::chrono::milliseconds timeout) override;
    void Interrupt() override;

private:
    bool TakeBufferedLine(std::string& line);
    void Append(char const* data, std::size_t size);

    std::string _buffer;
    bool _skipToNewline = false;
    std::atomic<bool> _interrupted{ false };
    std::atomic<bool> _closed{ false };
#ifdef _WIN32
    ReadResult ReadConsoleLine(std::string& line);
    ReadResult ReadPipe(std::string& line, std::chrono::milliseconds timeout);

    void* _handle = nullptr;
    unsigned long _fileType = 0;
    std::mutex _threadMutex;
    void* _readerThread = nullptr;
#endif
};

#endif
