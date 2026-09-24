/*
 * Project Ambrose by Imjustchico
 * Answers with the list as the login server holds it, judged by the same rule that decides where a player is sent: a realm counts as online while its last beat is inside the policy's window, so what an operator reads here and what a client is offered can never disagree. The age of a beat is given as well as the moment of it, because a page that sits open should be able to say how stale a figure is without asking the time again.
 */

#include "AdminRealmsView.h"
#include "AdminRouter.h"
#include "RealmList.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <utility>
#include <vector>

namespace
{
    int64 NowSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

std::string AdminRealmsView::RealmsJson(int64 nowEpochSeconds)
{
    RealmPolicy const policy = sRealmList.GetPolicy();
    std::vector<Realm> const realms = sRealmList.All();

    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["heartbeat_seconds"] = policy.HeartbeatSeconds;
    body["offline_after_intervals"] = policy.OfflineAfterIntervals;

    nlohmann::json rows = nlohmann::json::array();
    uint32 players = 0;
    uint32 online = 0;
    for (Realm const& realm : realms)
    {
        bool const alive = RealmList::IsOnline(realm, policy, nowEpochSeconds);
        players += realm.Population;
        online += alive ? 1 : 0;

        nlohmann::json row;
        row["id"] = realm.Id;
        row["name"] = realm.Name;
        row["address"] = realm.Address;
        row["local_address"] = realm.LocalAddress;
        row["port"] = realm.Port;
        row["flags"] = realm.Flags;
        row["population"] = realm.Population;
        row["player_limit"] = realm.PlayerLimit;
        row["full"] = realm.Full();
        row["marked_offline"] = realm.MarkedOffline();
        row["online"] = alive;
        row["last_heartbeat_epoch_seconds"] = realm.LastHeartbeatEpoch;
        row["heartbeat_age_seconds"] = realm.LastHeartbeatEpoch > 0 && nowEpochSeconds > realm.LastHeartbeatEpoch
            ? nowEpochSeconds - realm.LastHeartbeatEpoch
            : 0;
        rows.push_back(std::move(row));
    }
    body["realms"] = std::move(rows);
    body["counted"] = realms.size();
    body["online"] = online;
    body["players"] = players;
    return body.dump();
}

void AdminRealmsView::Register(AdminRouter& router)
{
    router.AddGuarded("GET", "/api/realms", "realms.read", [](AdminRequest const&)
    {
        return AdminResponse::Json(200, RealmsJson(NowSeconds()));
    });
}
