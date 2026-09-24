/*
 * Project Ambrose by Imjustchico
 * Shares one LogFile per normalized path, remembering which paths were already opened in this process.
 */

#include "LogFileRegistry.h"

#include <algorithm>

LogFileRegistry& LogFileRegistry::Instance()
{
    static LogFileRegistry* const instance = new LogFileRegistry();
    return *instance;
}

std::filesystem::path LogFileRegistry::MakeKey(std::filesystem::path const& absolutePath)
{
#ifdef _WIN32
    std::wstring text = absolutePath.lexically_normal().native();
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : (c == L'/' ? L'\\' : c); });
    return std::filesystem::path(text);
#else
    return absolutePath.lexically_normal();
#endif
}

std::shared_ptr<LogFile> LogFileRegistry::Acquire(std::filesystem::path const& absolutePath, LogFileOptions const& options, std::string& error)
{
    std::filesystem::path const key = MakeKey(absolutePath);
    std::shared_ptr<LogFile> file;
    {
        std::lock_guard lock(_mutex);
        for (auto it = _open.begin(); it != _open.end();)
            it = it->second.expired() ? _open.erase(it) : std::next(it);
        auto const it = _open.find(key);
        if (it != _open.end())
            file = it->second.lock();
        if (!file)
        {
            file = std::make_shared<LogFile>(absolutePath.lexically_normal());
            bool const first = _openedThisProcess.insert(key).second;
            if (std::optional<std::string> const failure = file->Open(options, first))
            {
                if (first)
                    _openedThisProcess.erase(key);
                error = *failure;
                return nullptr;
            }
            _open[key] = file;
            return file;
        }
    }
    file->UpdateOptions(options);
    return file;
}

std::vector<std::shared_ptr<LogFile>> LogFileRegistry::LiveFilesLocked()
{
    std::vector<std::shared_ptr<LogFile>> files;
    for (auto const& [key, weak] : _open)
        if (std::shared_ptr<LogFile> file = weak.lock())
            files.push_back(std::move(file));
    return files;
}

void LogFileRegistry::FlushDue(std::chrono::steady_clock::time_point now)
{
    std::vector<std::shared_ptr<LogFile>> files;
    {
        std::lock_guard lock(_mutex);
        files = LiveFilesLocked();
    }
    for (std::shared_ptr<LogFile> const& file : files)
        file->FlushIfDue(now);
}

void LogFileRegistry::FlushAll()
{
    std::vector<std::shared_ptr<LogFile>> files;
    {
        std::lock_guard lock(_mutex);
        files = LiveFilesLocked();
    }
    for (std::shared_ptr<LogFile> const& file : files)
        file->Flush();
}
