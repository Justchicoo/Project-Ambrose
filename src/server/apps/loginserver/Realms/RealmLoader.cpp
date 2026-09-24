/*
 * Project Ambrose by Imjustchico
 * Reading realmlist and handing it to sRealmList. The read is synchronous because it runs on the server's own timer rather than on a player's request, and a player who arrives mid-refresh is answered from the list already in place rather than waiting on a database. What comes back is swapped in whole, so a realm is never half-updated: a reader sees the list as it was or as it is.
 */

#include "RealmLoader.h"

#include "ConfigMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "StringUtil.h"

#include <utility>
#include <vector>

RealmLoaderSettings RealmLoaderSettings::Load(ConfigMgr const& config)
{
    RealmLoaderSettings settings;
    uint32 const refresh = config.GetOption<uint32>("Realm.RefreshInterval", DefaultRefreshSeconds, true);
    settings.RefreshSeconds = refresh != 0 ? refresh : DefaultRefreshSeconds;

    uint32 const heartbeat = config.GetOption<uint32>("Realm.HeartbeatInterval", RealmPolicy::DefaultHeartbeatSeconds, true);
    settings.Policy.HeartbeatSeconds = heartbeat != 0 ? heartbeat : RealmPolicy::DefaultHeartbeatSeconds;
    uint32 const intervals = config.GetOption<uint32>("Realm.OfflineAfterIntervals", RealmPolicy::DefaultOfflineAfterIntervals, true);
    settings.Policy.OfflineAfterIntervals = intervals != 0 ? intervals : RealmPolicy::DefaultOfflineAfterIntervals;
    settings.Policy.DefaultRealm = std::string(Ambrose::Trim(config.GetOption<std::string>("Realm.DefaultRealm", "", true)));
    return settings;
}

void RealmLoader::Configure(RealmLoaderSettings settings)
{
    _settings = std::move(settings);
    _since = std::chrono::milliseconds::zero();
    sRealmList.SetPolicy(_settings.Policy);
    LoadNow();
}

void RealmLoader::LoadNow()
{
    _since = std::chrono::milliseconds::zero();
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.IsOpen() ? LoginDatabase.GetPreparedStatement(LOGIN_SEL_REALMLIST) : nullptr;
    if (!statement)
    {
        if (_loads == 0)
        {
            sRealmList.Replace({});
            LOG_WARN("server.loginserver", "there is no login database to read the realm list from, so no player can be sent to a gameserver");
            ++_loads;
        }
        return;
    }

    PreparedQueryResult result = LoginDatabase.Query(*statement);

    if (!result)
    {
        if (_loads == 0)
        {
            sRealmList.Replace({});
            LOG_WARN("server.loginserver", "realmlist holds no realm, so no player can be sent to a gameserver until one is added");
            ++_loads;
        }
        return;
    }

    std::vector<Realm> realms;
    do
    {
        Field const* fields = result->Fetch();
        Realm realm;
        realm.Id = fields[0].Get<uint32>();
        realm.Name = fields[1].Get<std::string>();
        realm.Address = fields[2].Get<std::string>();
        realm.LocalAddress = fields[3].Get<std::string>();
        realm.Port = fields[4].Get<uint16>();
        realm.Flags = fields[5].Get<uint32>();
        realm.Population = fields[6].Get<uint32>();
        realm.PlayerLimit = fields[7].Get<uint32>();
        realm.LastHeartbeatEpoch = static_cast<int64>(fields[8].Get<uint64>());
        realms.push_back(std::move(realm));
    } while (result->NextRow());

    std::size_t const count = realms.size();
    sRealmList.Replace(std::move(realms));
    if (_loads == 0)
        LOG_INFO("server.loginserver", "The realm list holds {} realm(s), read again every {} second(s)", count, _settings.RefreshSeconds);
    ++_loads;
}

void RealmLoader::Update(std::chrono::milliseconds diff)
{
    _since += diff;
    if (_since < std::chrono::seconds(_settings.RefreshSeconds))
        return;
    LoadNow();
}
