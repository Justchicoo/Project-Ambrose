/*
 * Project Ambrose by Imjustchico
 * Reads <Name>DatabaseInfo, <Name>Database.WorkerThreads, <Name>Database.SynchThreads, MaxPingTime and the Updates.* options, updates and opens or reconfigures each pool, and reports every failure by pool name; each database's state is kept under a lock the admin API reads through, the startup updater and a live data-only apply share one updater lock so they never overlap, and a live apply is refused unless the database is open and Updates.EnableDatabases includes it.
 */

#include "DatabaseLoader.h"
#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "DatabaseWorkerPool.h"
#include "Log.h"

#include <fmt/format.h>

#include <chrono>

DatabaseLoader::DatabaseLoader(ConfigMgr const& config) : _config(config)
{
}

DatabaseLoader::~DatabaseLoader()
{
    Close();
}

std::string_view DatabaseLoader::StateName(DatabaseState state) noexcept
{
    switch (state)
    {
        case DatabaseState::Waiting: return "waiting";
        case DatabaseState::Unconfigured: return "unconfigured";
        case DatabaseState::Updating: return "updating";
        case DatabaseState::Opening: return "opening";
        case DatabaseState::Open: return "open";
        case DatabaseState::Failed: return "failed";
        case DatabaseState::Closed: return "closed";
    }
    return "closed";
}

DatabaseLoader& DatabaseLoader::AddDatabase(DatabaseWorkerPoolBase& pool, std::string name, uint32 updateFlag, std::string updateFolder)
{
    Entry entry;
    entry.Pool = &pool;
    entry.UpdateFlag = updateFlag;
    entry.UpdateFolder = updateFolder.empty() ? pool.GetName() : std::move(updateFolder);
    entry.Name = std::move(name);
    std::lock_guard const lock(_mutex);
    _entries.push_back(std::move(entry));
    return *this;
}

void DatabaseLoader::ReadOptions(Entry const& entry, std::string& info, uint32& asyncThreads, uint32& syncThreads) const
{
    info = _config.GetOption<std::string>(fmt::format("{}DatabaseInfo", entry.Name), "", true);
    asyncThreads = _config.GetOption<uint32>(fmt::format("{}Database.WorkerThreads", entry.Name), 1, true);
    syncThreads = _config.GetOption<uint32>(fmt::format("{}Database.SynchThreads", entry.Name), 1, true);
}

void DatabaseLoader::ApplyPingInterval() const
{
    uint32 const minutes = _config.GetOption<uint32>("MaxPingTime", DefaultMaxPingMinutes, true);
    for (Entry const& entry : _entries)
        entry.Pool->SetKeepAliveInterval(std::chrono::minutes(minutes));
}

UpdaterSettings DatabaseLoader::ReadUpdaterSettings() const
{
    UpdaterSettings settings;
    settings.AutoSetup = _config.GetOption<bool>("Updates.AutoSetup", true, true);
    std::string const source = _config.GetOption<std::string>("Updates.SourcePath", "", true);
    if (!source.empty())
        settings.SourceDirectory = ConfigMgr::PathFromUtf8(source);
    settings.Redundancy = _config.GetOption<bool>("Updates.Redundancy", false, true);
    settings.AllowRehash = _config.GetOption<bool>("Updates.AllowRehash", false, true);
    settings.CleanDeadRefMaxCount = _config.GetOption<int32>("Updates.CleanDeadRefMaxCount", 3, true);
    settings.AllowPending = _config.GetOption<bool>("Updates.AllowPending", false, true);
    return settings;
}

bool DatabaseLoader::UpdatesEnabled(Entry const& entry) const
{
    return (entry.UpdateFlag & _config.GetOption<uint32>("Updates.EnableDatabases", DATABASE_NONE, true)) != 0;
}

bool DatabaseLoader::RunUpdater(Entry const& entry, std::string const& info) const
{
    std::string error;
    std::optional<MySQLConnectionInfo> const parsed = MySQLConnectionInfo::Parse(info, &error);
    if (!parsed)
    {
        LOG_ERROR("sql.driver", "{}DatabaseInfo is not a valid connection string: {}", entry.Name, error);
        return false;
    }
    if (DBUpdater::Run(*parsed, entry.UpdateFolder, ReadUpdaterSettings()))
        return true;
    LOG_ERROR("sql.updates", "Could not update the {} database; fix the error above or clear bit {} of Updates.EnableDatabases", entry.Pool->GetName(), entry.UpdateFlag);
    return false;
}

void DatabaseLoader::SetState(Entry& entry, DatabaseState state)
{
    std::lock_guard const lock(_mutex);
    entry.State = state;
}

DatabaseLoader::Entry const* DatabaseLoader::Find(std::string_view name) const
{
    for (Entry const& entry : _entries)
        if (entry.Pool->GetName() == name)
            return &entry;
    return nullptr;
}

DatabaseLoader::Entry* DatabaseLoader::Find(std::string_view name)
{
    for (Entry& entry : _entries)
        if (entry.Pool->GetName() == name)
            return &entry;
    return nullptr;
}

bool DatabaseLoader::Load()
{
    ApplyPingInterval();
    for (Entry& entry : _entries)
    {
        uint32 asyncThreads = 1;
        uint32 syncThreads = 1;
        std::string info;
        ReadOptions(entry, info, asyncThreads, syncThreads);
        if (info.empty())
        {
            LOG_WARN("sql.driver", "{}DatabaseInfo is empty, so the {} database stays closed", entry.Name, entry.Pool->GetName());
            SetState(entry, DatabaseState::Unconfigured);
            continue;
        }
        if (!entry.Pool->SetConnectionInfo(info, asyncThreads, syncThreads))
        {
            LOG_ERROR("sql.driver", "{}DatabaseInfo is not a valid connection string", entry.Name);
            SetState(entry, DatabaseState::Failed);
            Close();
            return false;
        }
        if (UpdatesEnabled(entry))
        {
            std::lock_guard const updating(_updateMutex);
            SetState(entry, DatabaseState::Updating);
            if (!RunUpdater(entry, info))
            {
                SetState(entry, DatabaseState::Failed);
                Close();
                return false;
            }
        }
        SetState(entry, DatabaseState::Opening);
        if (uint32 const error = entry.Pool->Open())
        {
            LOG_ERROR("sql.driver", "Could not open the {} database (error {}); see the errors above, which name the connection or the statement and table that failed (tables come from updates when Updates.EnableDatabases includes this database)", entry.Pool->GetName(), error);
            SetState(entry, DatabaseState::Failed);
            Close();
            return false;
        }
        std::lock_guard const lock(_mutex);
        entry.Info = info;
        entry.AsyncThreads = asyncThreads;
        entry.SyncThreads = syncThreads;
        entry.Opened = true;
        entry.State = DatabaseState::Open;
    }
    return true;
}

bool DatabaseLoader::ApplyConfig()
{
    ApplyPingInterval();
    bool succeeded = true;
    for (Entry& entry : _entries)
    {
        uint32 asyncThreads = 1;
        uint32 syncThreads = 1;
        std::string info;
        ReadOptions(entry, info, asyncThreads, syncThreads);
        std::string current;
        uint32 currentAsync = 1;
        uint32 currentSync = 1;
        {
            std::lock_guard const lock(_mutex);
            current = entry.Info;
            currentAsync = entry.AsyncThreads;
            currentSync = entry.SyncThreads;
        }
        if (info == current && asyncThreads == currentAsync && syncThreads == currentSync)
            continue;
        if (info.empty())
        {
            LOG_WARN("sql.driver", "{}DatabaseInfo became empty; the {} database keeps its current connections", entry.Name, entry.Pool->GetName());
            continue;
        }
        if (!entry.Pool->IsOpen())
        {
            if (!entry.Pool->SetConnectionInfo(info, asyncThreads, syncThreads) || entry.Pool->Open() != 0)
            {
                LOG_ERROR("sql.driver", "The {} database could not be opened with the new {}DatabaseInfo", entry.Pool->GetName(), entry.Name);
                succeeded = false;
                continue;
            }
        }
        else if (uint32 const error = entry.Pool->Reconfigure(info, asyncThreads, syncThreads))
        {
            LOG_ERROR("sql.driver", "The changed {} database settings did not apply (error {}); the current connections stay in use", entry.Pool->GetName(), error);
            succeeded = false;
            continue;
        }
        std::lock_guard const lock(_mutex);
        entry.Info = info;
        entry.AsyncThreads = asyncThreads;
        entry.SyncThreads = syncThreads;
        entry.Opened = true;
        entry.State = DatabaseState::Open;
    }
    return succeeded;
}

void DatabaseLoader::Close()
{
    for (auto entry = _entries.rbegin(); entry != _entries.rend(); ++entry)
    {
        bool opened = false;
        {
            std::lock_guard const lock(_mutex);
            opened = entry->Opened;
        }
        if (opened)
            entry->Pool->Close();
        std::lock_guard const lock(_mutex);
        entry->Opened = false;
        if (entry->State != DatabaseState::Failed && entry->State != DatabaseState::Unconfigured)
            entry->State = DatabaseState::Closed;
    }
}

std::vector<std::string> DatabaseLoader::GetNames() const
{
    std::vector<std::string> names;
    std::lock_guard const lock(_mutex);
    for (Entry const& entry : _entries)
        names.push_back(entry.Pool->GetName());
    return names;
}

std::vector<DatabaseStatus> DatabaseLoader::GetStatus() const
{
    std::vector<DatabaseStatus> statuses;
    std::lock_guard const lock(_mutex);
    for (Entry const& entry : _entries)
    {
        DatabaseStatus status;
        status.Name = entry.Pool->GetName();
        status.Key = entry.Name;
        status.State = entry.State;
        status.Applying = entry.Applying;
        status.UpdateFlag = entry.UpdateFlag;
        status.UpdatesEnabled = UpdatesEnabled(entry);
        std::string const info = entry.Info.empty() ? _config.GetOption<std::string>(fmt::format("{}DatabaseInfo", entry.Name), "", true) : entry.Info;
        if (std::optional<MySQLConnectionInfo> const parsed = MySQLConnectionInfo::Parse(info))
            status.Address = parsed->ToLogString();
        status.Use = entry.Pool->GetUse();
        statuses.push_back(std::move(status));
    }
    return statuses;
}

DatabaseUpdates DatabaseLoader::InspectUpdates(std::string_view name) const
{
    DatabaseUpdates result;
    result.Name = std::string(name);
    std::string info;
    std::string key;
    {
        std::lock_guard const lock(_mutex);
        Entry const* const entry = Find(name);
        if (!entry)
        {
            result.Error = fmt::format("this app has no {} database", name);
            return result;
        }
        key = entry->Name;
        info = entry->Info.empty() ? _config.GetOption<std::string>(fmt::format("{}DatabaseInfo", entry->Name), "", true) : entry->Info;
    }
    if (info.empty())
    {
        result.Error = fmt::format("{}DatabaseInfo is empty, so the {} database stays closed", key, name);
        return result;
    }
    std::string error;
    std::optional<MySQLConnectionInfo> const parsed = MySQLConnectionInfo::Parse(info, &error);
    if (!parsed)
    {
        result.Error = fmt::format("{}DatabaseInfo is not a valid connection string: {}", key, error);
        return result;
    }
    result.Listed = DBUpdater::Inspect(*parsed, ReadUpdaterSettings(), result.Report, result.Error);
    return result;
}

DatabaseApplyResult DatabaseLoader::ApplyDataUpdates(std::string_view name)
{
    DatabaseApplyResult result;
    std::unique_lock const updating(_updateMutex, std::try_to_lock);
    if (!updating.owns_lock())
    {
        result.Refusal = DatabaseApplyRefusal::Busy;
        result.Message = "The updater is already running in this app; try again when it finishes";
        return result;
    }
    std::string info;
    std::string folder;
    {
        std::lock_guard const lock(_mutex);
        Entry* const entry = Find(name);
        if (!entry)
        {
            result.Refusal = DatabaseApplyRefusal::Unknown;
            result.Message = fmt::format("This app has no {} database", name);
            return result;
        }
        if (entry->State != DatabaseState::Open)
        {
            result.Refusal = DatabaseApplyRefusal::NotOpen;
            result.Message = fmt::format("The {} database is {}, so no update can be applied to it", name, StateName(entry->State));
            return result;
        }
        if (!UpdatesEnabled(*entry))
        {
            result.Refusal = DatabaseApplyRefusal::UpdatesDisabled;
            result.Message = fmt::format("Updates.EnableDatabases leaves out the {} database (bit {}), so its updates are applied by hand", name, entry->UpdateFlag);
            return result;
        }
        info = entry->Info;
        folder = entry->UpdateFolder;
        entry->Applying = true;
    }
    std::string error;
    if (std::optional<MySQLConnectionInfo> const parsed = MySQLConnectionInfo::Parse(info, &error))
        result.Summary = DBUpdater::ApplyDataOnly(*parsed, folder, ReadUpdaterSettings());
    else
    {
        result.Refusal = DatabaseApplyRefusal::BadConnection;
        result.Message = fmt::format("The {} connection string is not valid: {}", name, error);
    }
    std::lock_guard const lock(_mutex);
    if (Entry* const entry = Find(name))
        entry->Applying = false;
    return result;
}
