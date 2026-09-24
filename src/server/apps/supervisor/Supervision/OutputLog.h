/*
 * Project Ambrose by Imjustchico
 * The supervisor's own capture of one app's output: the files the app writes its standard output and error into for this run and the one before, read as they grow into rings of the last lines of each run with the stream and time of each, the supervisor's own notes in the same rings, a file cut back to nothing once it passes its limit because the app keeps appending at the new end, and the tails of the files an adopted app is still writing read back in, so the page has the output whether or not the app's admin API is on.
 */

#ifndef AMBROSE_OUTPUTLOG_H
#define AMBROSE_OUTPUTLOG_H

#include "Types.h"

#include <array>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

struct OutputLine
{
    uint64 Sequence = 0;
    std::string Stream;
    std::string Text;
    int64 EpochMs = 0;
};

enum class OutputRun : uint8
{
    Current,
    Previous
};

class OutputLog
{
public:
    static constexpr std::size_t RingLines = 1000;
    static constexpr std::size_t MaxLineBytes = 64 * 1024;
    static constexpr uint64 DefaultMaxFileBytes = 16 * 1024 * 1024;
    static constexpr uint64 MinMaxFileBytes = 64 * 1024;
    static constexpr uint64 MaxReadBytes = 1024 * 1024;
    static constexpr uint64 MaxTailBytes = 4 * 1024 * 1024;

    OutputLog(std::filesystem::path folder, uint64 maxFileBytes);

    std::filesystem::path OutputFile() const;
    std::filesystem::path ErrorFile() const;
    std::filesystem::path const& GetFolder() const noexcept { return _folder; }

    bool BeginRun(std::string& error);
    void Attach();
    std::vector<OutputLine> Poll();
    void Note(std::string const& text);
    std::vector<OutputLine> Lines(OutputRun run, uint64 after) const;

    static std::string Clean(std::string_view text);

private:
    struct Stream
    {
        std::string Name;
        std::filesystem::path Current;
        std::filesystem::path Previous;
        uint64 Offset = 0;
        std::string Pending;
    };

    void Push(std::deque<OutputLine>& ring, std::string const& stream, std::string_view text, int64 epochMs, std::vector<OutputLine>* collected);
    std::vector<OutputLine> Read(Stream& stream, bool flush);
    static std::vector<std::string> Tail(std::filesystem::path const& file, std::size_t count);

    std::filesystem::path _folder;
    uint64 _maxFileBytes;
    mutable std::mutex _mutex;
    std::array<Stream, 2> _streams;
    std::deque<OutputLine> _current;
    std::deque<OutputLine> _previous;
    uint64 _sequence = 0;
};

#endif
