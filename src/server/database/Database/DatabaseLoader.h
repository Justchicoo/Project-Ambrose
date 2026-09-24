/*
 * Project Ambrose by Imjustchico
 * Opens an app's database pools from its configuration in order, running the updater first at startup for databases Updates.EnableDatabases selects, unwinds on the first failure, re-applies changed connection options live, and closes them in reverse; while the app runs it reports each database's state and pool use, lists its applied and pending updates, and applies the pending data-only ones live, with one updater run at a time across the app.
 */

#ifndef AMBROSE_DATABASELOADER_H
#define AMBROSE_DATABASELOADER_H

#include "ConfigMgr.h"
#include "DatabaseConnectionSet.h"
#include "Types.h"
#include "UpdateFetcher.h"

#include <array>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class DatabaseWorkerPoolBase;
struct UpdaterSettings;

enum class DatabaseState : uint8
{
    Waiting,
    Unconfigured,
    Updating,
    Opening,
    Open,
    Failed,
    Closed
};

enum class DatabaseApplyRefusal : uint8
{
    None,
    Unknown,
    Busy,
    NotOpen,
    UpdatesDisabled,
    BadConnection
};

struct DatabaseStatus
{
    std::string Name;
    std::string Key;
    DatabaseState State = DatabaseState::Waiting;
    bool Applying = false;
    bool UpdatesEnabled = false;
    uint32 UpdateFlag = 0;
    std::string Address;
    DatabasePoolUse Use;
};

struct DatabaseUpdates
{
    std::string Name;
    bool Listed = false;
    std::string Error;
    UpdateReport Report;
};

struct DatabaseApplyResult
{
    DatabaseApplyRefusal Refusal = DatabaseApplyRefusal::None;
    std::string Message;
    UpdateSummary Summary;
};

class DatabaseLoader
{
public:
    static constexpr uint32 DefaultMaxPingMinutes = 30;
    static constexpr std::array<RestartRequiredOption, 1> RestartRequiredOptions{ {
        { "Updates.AutoSetup", "A missing database is created only while the server starts, so a change takes effect at the next start" }
    } };

    enum DatabaseTypeFlags : uint32
    {
        DATABASE_NONE = 0,
        DATABASE_LOGIN = 1,
        DATABASE_CHARACTER = 2,
        DATABASE_WORLD = 4
    };

    explicit DatabaseLoader(ConfigMgr const& config);
    ~DatabaseLoader();

    DatabaseLoader(DatabaseLoader const&) = delete;
    DatabaseLoader& operator=(DatabaseLoader const&) = delete;

    DatabaseLoader& AddDatabase(DatabaseWorkerPoolBase& pool, std::string name, uint32 updateFlag = DATABASE_NONE, std::string updateFolder = {});
    bool Load();
    bool ApplyConfig();
    void Close();

    std::vector<std::string> GetNames() const;
    std::vector<DatabaseStatus> GetStatus() const;
    DatabaseUpdates InspectUpdates(std::string_view name) const;
    DatabaseApplyResult ApplyDataUpdates(std::string_view name);

    static std::string_view StateName(DatabaseState state) noexcept;

private:
    struct Entry
    {
        DatabaseWorkerPoolBase* Pool = nullptr;
        std::string Name;
        std::string Info;
        uint32 AsyncThreads = 1;
        uint32 SyncThreads = 1;
        uint32 UpdateFlag = DATABASE_NONE;
        std::string UpdateFolder;
        bool Opened = false;
        bool Applying = false;
        DatabaseState State = DatabaseState::Waiting;
    };

    void ReadOptions(Entry const& entry, std::string& info, uint32& asyncThreads, uint32& syncThreads) const;
    void ApplyPingInterval() const;
    UpdaterSettings ReadUpdaterSettings() const;
    bool UpdatesEnabled(Entry const& entry) const;
    bool RunUpdater(Entry const& entry, std::string const& info) const;
    void SetState(Entry& entry, DatabaseState state);
    Entry const* Find(std::string_view name) const;
    Entry* Find(std::string_view name);

    ConfigMgr const& _config;
    mutable std::mutex _mutex;
    std::mutex _updateMutex;
    std::vector<Entry> _entries;
};

#endif
