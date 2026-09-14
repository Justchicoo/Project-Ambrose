/*
 * Project Ambrose by Imjustchico
 * Reads <Name>DatabaseInfo, <Name>Database.WorkerThreads, <Name>Database.SynchThreads, MaxPingTime and the Updates.* options, updates and opens or reconfigures each pool, and reports every failure by pool name.
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

DatabaseLoader& DatabaseLoader::AddDatabase(DatabaseWorkerPoolBase& pool, std::string name, uint32 updateFlag, std::string updateFolder)
{
    Entry entry;
    entry.Pool = &pool;
    entry.UpdateFlag = updateFlag;
    entry.UpdateFolder = updateFolder.empty() ? pool.GetName() : std::move(updateFolder);
    entry.Name = std::move(name);
    _entries.push_back(std::move(entry));
    return *this;
}

void DatabaseLoader::ReadOptions(Entry& entry, std::string& info, uint32& asyncThreads, uint32& syncThreads) const
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

bool DatabaseLoader::RunUpdater(Entry const& entry, std::string const& info) const
{
    uint32 const enabled = _config.GetOption<uint32>("Updates.EnableDatabases", DATABASE_NONE, true);
    if ((entry.UpdateFlag & enabled) == 0)
        return true;
    std::string error;
    std::optional<MySQLConnectionInfo> const parsed = MySQLConnectionInfo::Parse(info, &error);
    if (!parsed)
    {
        LOG_ERROR("sql.driver", "{}DatabaseInfo is not a valid connection string: {}", entry.Name, error);
        return false;
    }
    UpdaterSettings settings;
    settings.AutoSetup = _config.GetOption<bool>("Updates.AutoSetup", true, true);
    std::string const source = _config.GetOption<std::string>("Updates.SourcePath", "", true);
    if (!source.empty())
        settings.SourceDirectory = ConfigMgr::PathFromUtf8(source);
    if (DBUpdater::Run(*parsed, entry.UpdateFolder, settings))
        return true;
    LOG_ERROR("sql.updates", "Could not update the {} database; fix the error above or clear bit {} of Updates.EnableDatabases", entry.Pool->GetName(), entry.UpdateFlag);
    return false;
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
            continue;
        }
        if (!entry.Pool->SetConnectionInfo(info, asyncThreads, syncThreads))
        {
            LOG_ERROR("sql.driver", "{}DatabaseInfo is not a valid connection string", entry.Name);
            Close();
            return false;
        }
        if (!RunUpdater(entry, info))
        {
            Close();
            return false;
        }
        if (uint32 const error = entry.Pool->Open())
        {
            LOG_ERROR("sql.driver", "Could not open the {} database (error {}); see the errors above, which name the connection or the statement and table that failed (tables come from updates when Updates.EnableDatabases includes this database)", entry.Pool->GetName(), error);
            Close();
            return false;
        }
        entry.Info = info;
        entry.AsyncThreads = asyncThreads;
        entry.SyncThreads = syncThreads;
        entry.Opened = true;
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
        if (info == entry.Info && asyncThreads == entry.AsyncThreads && syncThreads == entry.SyncThreads)
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
        entry.Info = info;
        entry.AsyncThreads = asyncThreads;
        entry.SyncThreads = syncThreads;
        entry.Opened = true;
    }
    return succeeded;
}

void DatabaseLoader::Close()
{
    for (auto entry = _entries.rbegin(); entry != _entries.rend(); ++entry)
    {
        if (entry->Opened)
            entry->Pool->Close();
        entry->Opened = false;
    }
}
