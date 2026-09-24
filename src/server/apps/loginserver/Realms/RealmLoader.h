/*
 * Project Ambrose by Imjustchico
 * Filling the realm list from the login database. The rows are read again every Realm.RefreshInterval seconds, so a realm added or edited while the server runs is picked up without a restart and a heartbeat written by a gameserver becomes something the login server acts on. A read that fails leaves the realms already loaded in place, because a list one refresh out of date routes players to servers that are still there, while an empty one routes them nowhere.
 */

#ifndef AMBROSE_REALMLOADER_H
#define AMBROSE_REALMLOADER_H

#include "RealmList.h"
#include "Types.h"

#include <chrono>
#include <string>

class ConfigMgr;

struct RealmLoaderSettings
{
    static constexpr uint32 DefaultRefreshSeconds = 10;

    uint32 RefreshSeconds = DefaultRefreshSeconds;
    RealmPolicy Policy;

    static RealmLoaderSettings Load(ConfigMgr const& config);
};

class RealmLoader
{
public:
    RealmLoader() = default;

    RealmLoader(RealmLoader const&) = delete;
    RealmLoader& operator=(RealmLoader const&) = delete;

    void Configure(RealmLoaderSettings settings);
    void LoadNow();
    void Update(std::chrono::milliseconds diff);

    uint64 Loads() const noexcept { return _loads; }

private:
    RealmLoaderSettings _settings;
    std::chrono::milliseconds _since{ 0 };
    uint64 _loads = 0;
};

#endif
