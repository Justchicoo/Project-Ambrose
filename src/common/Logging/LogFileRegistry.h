/*
 * Project Ambrose by Imjustchico
 * Process-wide map from absolute log paths to shared LogFile objects so reloads never reopen or truncate a live file.
 */

#ifndef AMBROSE_LOGFILEREGISTRY_H
#define AMBROSE_LOGFILEREGISTRY_H

#include "LogFile.h"

#include <map>
#include <set>

class LogFileRegistry
{
public:
    static LogFileRegistry& Instance();

    std::shared_ptr<LogFile> Acquire(std::filesystem::path const& absolutePath, LogFileOptions const& options, std::string& error);
    void FlushDue(std::chrono::steady_clock::time_point now);
    void FlushAll();

    static std::filesystem::path MakeKey(std::filesystem::path const& absolutePath);

private:
    std::vector<std::shared_ptr<LogFile>> LiveFilesLocked();

    std::mutex _mutex;
    std::map<std::filesystem::path, std::weak_ptr<LogFile>> _open;
    std::set<std::filesystem::path> _openedThisProcess;
};

#endif
