/*
 * Project Ambrose by Imjustchico
 * Opens an app's database pools from its configuration in order, running the updater first at startup for databases Updates.EnableDatabases selects, unwinds on the first failure, re-applies changed connection options live, and closes them in reverse.
 */

#ifndef AMBROSE_DATABASELOADER_H
#define AMBROSE_DATABASELOADER_H

#include "Types.h"

#include <string>
#include <vector>

class ConfigMgr;
class DatabaseWorkerPoolBase;

class DatabaseLoader
{
public:
    static constexpr uint32 DefaultMaxPingMinutes = 30;

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
    };

    void ReadOptions(Entry& entry, std::string& info, uint32& asyncThreads, uint32& syncThreads) const;
    void ApplyPingInterval() const;
    bool RunUpdater(Entry const& entry, std::string const& info) const;

    ConfigMgr const& _config;
    std::vector<Entry> _entries;
};

#endif
