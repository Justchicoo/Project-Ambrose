/*
 * Project Ambrose by Imjustchico
 * One open log file on raw OS handles with its own buffer, open modes, backups, size rotation, pruning and flush policy.
 */

#ifndef AMBROSE_LOGFILE_H
#define AMBROSE_LOGFILE_H

#include "Types.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class LogFileMode : uint8
{
    Append,
    Truncate
};

struct LogFileOptions
{
    LogFileMode Mode = LogFileMode::Append;
    bool BackupExisting = false;
    bool TimestampName = false;
    uint64 MaxFileSize = 0;
    uint32 MaxBackups = 0;
    std::chrono::milliseconds FlushInterval{ 0 };
    bool Utc = false;
    std::chrono::system_clock::time_point LoadTime;
};

class LogFile
{
public:
    static constexpr std::size_t BufferSize = 65536;
    static constexpr std::chrono::seconds ReopenDelay{ 5 };

    explicit LogFile(std::filesystem::path configuredPath);
    ~LogFile();

    LogFile(LogFile const&) = delete;
    LogFile& operator=(LogFile const&) = delete;

    std::optional<std::string> Open(LogFileOptions const& options, bool firstOpenInProcess);
    void UpdateOptions(LogFileOptions const& options);
    void WriteLines(std::string_view lines, bool flushNow);
    void Flush();
    void FlushIfDue(std::chrono::steady_clock::time_point now);

    std::filesystem::path GetActivePath() const;
    uint64 GetSize() const;
    uint64 GetRotationCount() const;
    uint64 GetLostLineCount() const;
    std::size_t GetBufferedBytes() const;
    std::chrono::milliseconds GetFlushInterval() const;

    static std::filesystem::path MakeTimestampedPath(std::filesystem::path const& path, std::chrono::system_clock::time_point time, bool utc, uint32 collision);
    static std::filesystem::path MakeFreeTimestampedPath(std::filesystem::path const& path, std::chrono::system_clock::time_point time, bool utc);

private:
    class Handle;

    std::optional<std::string> OpenLocked(bool truncate);
    void WriteLocked(std::string_view bytes);
    void FlushLocked();
    void RotateLocked();
    void PruneLocked();
    void WriteNoticeLocked(std::string_view level, std::string const& text);

    mutable std::mutex _mutex;
    std::filesystem::path _configuredPath;
    std::filesystem::path _activePath;
    LogFileOptions _options;
    std::unique_ptr<Handle> _handle;
    std::vector<char> _buffer;
    uint64 _size = 0;
    uint64 _nextRotationCheck = 0;
    uint64 _rotations = 0;
    uint64 _lostLines = 0;
    uint64 _unreportedLostLines = 0;
    std::chrono::steady_clock::time_point _lastFlush;
    std::chrono::steady_clock::time_point _nextReopen;
};

#endif
