/*
 * Project Ambrose by Imjustchico
 * Every error an app has raised, gathered into one group per place in the code: the category, the source location and the template are the key, so one line raised a thousand times is one group with a count rather than a thousand records to read past, and two lines that read alike but come from different places stay apart. A group keeps when it was first and last seen, the build it came from and the last message it rendered, and the store is bounded with the group seen longest ago dropped first, so an app that raises errors forever does not grow forever. The message is kept as it was written and masked where it leaves, because masking belongs to the boundary that knows what a secret is.
 */

#ifndef AMBROSE_LOGERRORS_H
#define AMBROSE_LOGERRORS_H

#include "LogMessage.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct LogErrorGroup
{
    std::string Category;
    std::string File;
    uint32 Line = 0;
    std::string Function;
    std::string Template;
    std::string LastMessage;
    std::string Revision;
    LogLevel Level = LogLevel::Error;
    uint64 Count = 0;
    std::chrono::system_clock::time_point FirstSeen;
    std::chrono::system_clock::time_point LastSeen;
};

class LogErrorStore
{
public:
    static constexpr std::size_t DefaultCapacity = 200;
    static constexpr std::size_t MaxCapacity = 10000;

    LogErrorStore() = default;

    LogErrorStore(LogErrorStore const&) = delete;
    LogErrorStore& operator=(LogErrorStore const&) = delete;

    void Note(LogMessage const& message) noexcept;
    std::vector<LogErrorGroup> Groups() const;
    std::size_t Size() const;
    void Clear();
    void SetCapacity(std::size_t capacity);
    std::size_t GetCapacity() const;
    uint64 GetDropped() const;

private:
    struct Entry
    {
        LogErrorGroup Group;
        uint64 Order = 0;
    };

    static std::string KeyOf(LogMessage const& message);
    void EvictOldest();

    mutable std::mutex _mutex;
    std::unordered_map<std::string, Entry> _groups;
    std::size_t _capacity = DefaultCapacity;
    uint64 _order = 0;
    uint64 _dropped = 0;
};

#endif
