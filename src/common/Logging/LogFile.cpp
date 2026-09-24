/*
 * Project Ambrose by Imjustchico
 * Buffered log file writes on raw handles with truncate-once, backups, size rotation, pruning, and reopen after failure.
 */

#include "LogFile.h"
#include "ConfigMgr.h"
#include "LogCommon.h"
#include "LogTimestamp.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <regex>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
    std::string LastErrorMessage()
    {
#ifdef _WIN32
        return std::system_category().message(static_cast<int>(::GetLastError()));
#else
        return std::generic_category().message(errno);
#endif
    }

    std::size_t CountLines(std::string_view bytes)
    {
        return static_cast<std::size_t>(std::count(bytes.begin(), bytes.end(), '\n'));
    }

    std::string Utf8(std::filesystem::path const& path)
    {
        return ConfigMgr::PathToUtf8(path);
    }
}

class LogFile::Handle
{
public:
#ifdef _WIN32
    explicit Handle(HANDLE handle) : _handle(handle) { }

    ~Handle()
    {
        ::CloseHandle(_handle);
    }

    static std::unique_ptr<Handle> Open(std::filesystem::path const& path, bool truncate, std::string& error)
    {
        DWORD const access = truncate ? GENERIC_WRITE : FILE_APPEND_DATA;
        DWORD const disposition = truncate ? CREATE_ALWAYS : OPEN_ALWAYS;
        HANDLE const handle = ::CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            error = LastErrorMessage();
            return nullptr;
        }
        return std::make_unique<Handle>(handle);
    }

    bool Write(char const* data, std::size_t size)
    {
        while (size > 0)
        {
            DWORD const chunk = static_cast<DWORD>(size > 1048576 ? 1048576 : size);
            DWORD written = 0;
            if (!::WriteFile(_handle, data, chunk, &written, nullptr) || written == 0)
                return false;
            data += written;
            size -= written;
        }
        return true;
    }

    uint64 Size() const
    {
        LARGE_INTEGER size{};
        return ::GetFileSizeEx(_handle, &size) ? static_cast<uint64>(size.QuadPart) : 0;
    }

private:
    HANDLE _handle;
#else
    explicit Handle(int descriptor) : _descriptor(descriptor) { }

    ~Handle()
    {
        ::close(_descriptor);
    }

    static std::unique_ptr<Handle> Open(std::filesystem::path const& path, bool truncate, std::string& error)
    {
        int const flags = O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | (truncate ? O_TRUNC : 0);
        int descriptor = -1;
        do
            descriptor = ::open(path.c_str(), flags, 0644);
        while (descriptor < 0 && errno == EINTR);
        if (descriptor < 0)
        {
            error = LastErrorMessage();
            return nullptr;
        }
        return std::make_unique<Handle>(descriptor);
    }

    bool Write(char const* data, std::size_t size)
    {
        while (size > 0)
        {
            ssize_t const written = ::write(_descriptor, data, size);
            if (written < 0)
            {
                if (errno == EINTR)
                    continue;
                return false;
            }
            data += written;
            size -= static_cast<std::size_t>(written);
        }
        return true;
    }

    uint64 Size() const
    {
        struct stat info{};
        return ::fstat(_descriptor, &info) == 0 ? static_cast<uint64>(info.st_size) : 0;
    }

private:
    int _descriptor;
#endif
};

LogFile::LogFile(std::filesystem::path configuredPath) : _configuredPath(std::move(configuredPath)), _activePath(_configuredPath)
{
}

LogFile::~LogFile()
{
    std::lock_guard lock(_mutex);
    FlushLocked();
    _handle.reset();
}

std::optional<std::string> LogFile::Open(LogFileOptions const& options, bool firstOpenInProcess)
{
    std::lock_guard lock(_mutex);
    _options = options;
    std::error_code error;
    std::filesystem::create_directories(_configuredPath.parent_path(), error);
    if (error)
        return fmt::format("cannot create {}: {}", Utf8(_configuredPath.parent_path()), error.message());

    bool truncate = false;
    if (options.TimestampName)
        _activePath = MakeFreeTimestampedPath(_configuredPath, options.LoadTime, options.Utc);
    else
    {
        _activePath = _configuredPath;
        truncate = options.Mode == LogFileMode::Truncate && firstOpenInProcess;
        if (truncate && options.BackupExisting)
        {
            std::error_code sizeError;
            uintmax_t const existing = std::filesystem::file_size(_activePath, sizeError);
            if (!sizeError && existing > 0)
            {
                std::filesystem::path const backup = MakeFreeTimestampedPath(_configuredPath, options.LoadTime, options.Utc);
                std::error_code renameError;
                std::filesystem::rename(_activePath, backup, renameError);
                if (renameError)
                    return fmt::format("cannot back up {} to {}: {}", Utf8(_activePath), Utf8(backup), renameError.message());
            }
        }
    }
    std::optional<std::string> const failure = OpenLocked(truncate);
    if (failure)
        return fmt::format("cannot open {}: {}", Utf8(_activePath), *failure);
    _nextRotationCheck = 0;
    _lastFlush = std::chrono::steady_clock::now();
    if (_options.MaxBackups > 0)
        PruneLocked();
    return std::nullopt;
}

std::optional<std::string> LogFile::OpenLocked(bool truncate)
{
    std::string error;
    _handle = Handle::Open(_activePath, truncate, error);
    if (!_handle)
        return error;
    _size = _handle->Size();
    return std::nullopt;
}

void LogFile::UpdateOptions(LogFileOptions const& options)
{
    std::lock_guard lock(_mutex);
    FlushLocked();
    _options.MaxFileSize = options.MaxFileSize;
    _options.MaxBackups = options.MaxBackups;
    _options.FlushInterval = options.FlushInterval;
    _options.Utc = options.Utc;
    _nextRotationCheck = 0;
}

void LogFile::WriteLines(std::string_view lines, bool flushNow)
{
    std::lock_guard lock(_mutex);
    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    if (!_handle)
    {
        if (now < _nextReopen || OpenLocked(false))
        {
            _nextReopen = std::max(_nextReopen, now + ReopenDelay);
            std::size_t const lost = CountLines(lines);
            _lostLines += lost;
            _unreportedLostLines += lost;
            return;
        }
        if (_unreportedLostLines > 0)
        {
            WriteNoticeLocked("WARN ", fmt::format("{} lines lost while {} was unavailable", _unreportedLostLines, Utf8(_activePath)));
            _unreportedLostLines = 0;
        }
    }
    if (_options.MaxFileSize > 0 && _size > 0 && _size + lines.size() > _options.MaxFileSize && _size >= _nextRotationCheck)
        RotateLocked();
    WriteLocked(lines);
    if (flushNow || _options.FlushInterval.count() == 0 || now - _lastFlush >= _options.FlushInterval)
        FlushLocked();
}

void LogFile::WriteLocked(std::string_view bytes)
{
    if (_buffer.size() + bytes.size() > BufferSize && !_buffer.empty())
        FlushLocked();
    _buffer.insert(_buffer.end(), bytes.begin(), bytes.end());
    _size += bytes.size();
    if (_buffer.size() >= BufferSize)
        FlushLocked();
}

void LogFile::Flush()
{
    std::lock_guard lock(_mutex);
    FlushLocked();
}

void LogFile::FlushIfDue(std::chrono::steady_clock::time_point now)
{
    std::lock_guard lock(_mutex);
    if (!_buffer.empty() && now - _lastFlush >= _options.FlushInterval)
        FlushLocked();
}

void LogFile::FlushLocked()
{
    _lastFlush = std::chrono::steady_clock::now();
    if (_buffer.empty())
        return;
    if (_handle && _handle->Write(_buffer.data(), _buffer.size()))
    {
        _buffer.clear();
        return;
    }
    std::size_t const lost = CountLines(std::string_view(_buffer.data(), _buffer.size()));
    _lostLines += lost;
    _unreportedLostLines += lost;
    _buffer.clear();
    _handle.reset();
    _nextReopen = _lastFlush + ReopenDelay;
}

void LogFile::RotateLocked()
{
    FlushLocked();
    if (!_handle)
        return;
    std::chrono::system_clock::time_point const now = std::chrono::system_clock::now();
    _handle.reset();
    if (_options.TimestampName)
    {
        _activePath = MakeFreeTimestampedPath(_configuredPath, now, _options.Utc);
        if (OpenLocked(true))
        {
            _nextReopen = std::chrono::steady_clock::now() + ReopenDelay;
            return;
        }
    }
    else
    {
        std::filesystem::path const backup = MakeFreeTimestampedPath(_configuredPath, now, _options.Utc);
        std::error_code renameError;
        std::filesystem::rename(_activePath, backup, renameError);
        if (renameError)
        {
            if (OpenLocked(false))
            {
                _nextReopen = std::chrono::steady_clock::now() + ReopenDelay;
                return;
            }
            WriteNoticeLocked("ERROR", fmt::format("cannot rotate {}: {}; continuing in this file", Utf8(_activePath), renameError.message()));
            _nextRotationCheck = _size + _options.MaxFileSize;
            return;
        }
        if (OpenLocked(true))
        {
            _nextReopen = std::chrono::steady_clock::now() + ReopenDelay;
            return;
        }
    }
    _size = 0;
    _nextRotationCheck = 0;
    ++_rotations;
    PruneLocked();
}

void LogFile::PruneLocked()
{
    if (_options.MaxBackups == 0)
        return;
    std::u8string const stem = _configuredPath.stem().u8string();
    std::u8string const extension = _configuredPath.extension().u8string();
    struct Backup
    {
        std::string Stamp;
        uint32 Collision;
        std::filesystem::path Path;
    };
    std::vector<Backup> backups;
    std::error_code error;
    std::filesystem::path const directory = _configuredPath.parent_path();
    static std::regex const Pattern(R"(^_(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})(?:_(\d{1,9}))?$)");
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
    {
        std::error_code typeError;
        if (!it->is_regular_file(typeError) || it->path() == _activePath)
            continue;
        std::u8string const name = it->path().filename().u8string();
        if (name.size() <= stem.size() + extension.size() || name.compare(0, stem.size(), stem) != 0 || name.compare(name.size() - extension.size(), extension.size(), extension) != 0)
            continue;
        std::string const middle(name.begin() + static_cast<std::ptrdiff_t>(stem.size()), name.end() - static_cast<std::ptrdiff_t>(extension.size()));
        std::smatch match;
        if (!std::regex_match(middle, match, Pattern))
            continue;
        uint32 collision = 0;
        if (match[2].matched)
            collision = static_cast<uint32>(std::stoul(match[2].str()));
        backups.push_back({ match[1].str(), collision, it->path() });
    }
    if (backups.size() <= _options.MaxBackups)
        return;
    std::sort(backups.begin(), backups.end(), [](Backup const& left, Backup const& right)
    {
        return left.Stamp != right.Stamp ? left.Stamp < right.Stamp : left.Collision < right.Collision;
    });
    std::size_t const excess = backups.size() - _options.MaxBackups;
    for (std::size_t i = 0; i < excess; ++i)
    {
        std::error_code removeError;
        std::filesystem::remove(backups[i].Path, removeError);
    }
}

void LogFile::WriteNoticeLocked(std::string_view level, std::string const& text)
{
    std::string const line = fmt::format("{} {} [server.logging] {}\n", LogTimestamp::FormatPrefix(std::chrono::system_clock::now(), _options.Utc), level, text);
    WriteLocked(line);
    FlushLocked();
}

std::filesystem::path LogFile::GetActivePath() const
{
    std::lock_guard lock(_mutex);
    return _activePath;
}

uint64 LogFile::GetSize() const
{
    std::lock_guard lock(_mutex);
    return _size;
}

uint64 LogFile::GetRotationCount() const
{
    std::lock_guard lock(_mutex);
    return _rotations;
}

uint64 LogFile::GetLostLineCount() const
{
    std::lock_guard lock(_mutex);
    return _lostLines;
}

std::size_t LogFile::GetBufferedBytes() const
{
    std::lock_guard lock(_mutex);
    return _buffer.size();
}

std::chrono::milliseconds LogFile::GetFlushInterval() const
{
    std::lock_guard lock(_mutex);
    return _options.FlushInterval;
}

std::filesystem::path LogFile::MakeTimestampedPath(std::filesystem::path const& path, std::chrono::system_clock::time_point time, bool utc, uint32 collision)
{
    std::array<char, LogTimestamp::FileNameLength> const stamp = LogTimestamp::FormatFileName(time, utc);
    std::u8string name = path.stem().u8string();
    name.push_back(u8'_');
    name.append(stamp.begin(), stamp.end());
    if (collision > 0)
    {
        std::string const suffix = fmt::format("_{}", collision);
        name.append(suffix.begin(), suffix.end());
    }
    name.append(path.extension().u8string());
    return path.parent_path() / std::filesystem::path(name);
}

std::filesystem::path LogFile::MakeFreeTimestampedPath(std::filesystem::path const& path, std::chrono::system_clock::time_point time, bool utc)
{
    std::filesystem::path const base = MakeTimestampedPath(path, time, utc, 0);
    std::u8string const baseStem = base.stem().u8string();
    std::u8string const extension = base.extension().u8string();
    std::optional<uint32> highest;
    std::error_code error;
    for (std::filesystem::directory_iterator it(path.parent_path(), error), end; !error && it != end; it.increment(error))
    {
        std::u8string const name = it->path().filename().u8string();
        if (name.size() < baseStem.size() + extension.size() || name.compare(0, baseStem.size(), baseStem) != 0 || name.compare(name.size() - extension.size(), extension.size(), extension) != 0)
            continue;
        std::u8string const middle = name.substr(baseStem.size(), name.size() - baseStem.size() - extension.size());
        if (middle.empty())
        {
            highest = std::max<uint32>(highest.value_or(0), 0);
            continue;
        }
        if (middle.size() < 2 || middle.size() > 10 || middle[0] != u8'_')
            continue;
        std::string const digits(middle.begin() + 1, middle.end());
        if (std::optional<uint32> const collision = Ambrose::StringTo<uint32>(digits))
            highest = std::max(highest.value_or(0), *collision);
    }
    if (!highest)
        return base;
    return MakeTimestampedPath(path, time, utc, *highest + 1);
}
