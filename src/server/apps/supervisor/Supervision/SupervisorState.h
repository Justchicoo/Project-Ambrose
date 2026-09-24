/*
 * Project Ambrose by Imjustchico
 * The supervisor's saved state: each app's desired state and the identity and start time of the process it started, written whenever either changes to a file only this user can read, through a temporary file renamed over the old one so a crash never leaves half a file, and read back at start so running apps are adopted and apps meant to run are started.
 */

#ifndef AMBROSE_SUPERVISORSTATE_H
#define AMBROSE_SUPERVISORSTATE_H

#include "ChildProcess.h"
#include "Types.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

struct SavedApp
{
    bool WantRunning = false;
    std::optional<ChildProcessIdentity> Process;
    int64 StartedEpochMs = 0;
};

class SupervisorState
{
public:
    static constexpr int SchemaVersion = 1;

    explicit SupervisorState(std::filesystem::path file);

    bool Load(std::string& error);
    bool Put(std::string const& name, SavedApp const& app, std::string& error);
    std::optional<SavedApp> Get(std::string const& name) const;
    std::filesystem::path const& GetFile() const noexcept { return _file; }

    static std::string ToJson(std::map<std::string, SavedApp, std::less<>> const& apps);
    static std::optional<std::map<std::string, SavedApp, std::less<>>> FromJson(std::string_view text, std::string& error);

private:
    bool Write(std::string& error) const;

    std::filesystem::path _file;
    mutable std::mutex _mutex;
    std::map<std::string, SavedApp, std::less<>> _apps;
};

#endif
