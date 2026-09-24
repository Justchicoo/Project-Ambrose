/*
 * Project Ambrose by Imjustchico
 * Deciding which realm a player is sent to. A realm marked offline by hand is offline whatever its heartbeat says, and a heartbeat older than the missed runs the policy allows is the same answer reached the other way. A named realm that is offline is not quietly replaced by another, because a player who asked for one world and was put in a second would find none of what they left there.
 */

#include "RealmList.h"

#include "StringUtil.h"

#include <algorithm>

int64 RealmPolicy::OldestLiveHeartbeat(int64 nowEpoch) const noexcept
{
    uint32 const seconds = HeartbeatSeconds != 0 ? HeartbeatSeconds : DefaultHeartbeatSeconds;
    uint32 const intervals = OfflineAfterIntervals != 0 ? OfflineAfterIntervals : DefaultOfflineAfterIntervals;
    return nowEpoch - static_cast<int64>(seconds) * static_cast<int64>(intervals);
}

RealmList& RealmList::Instance()
{
    static RealmList instance;
    return instance;
}

void RealmList::SetPolicy(RealmPolicy policy)
{
    std::unique_lock const lock(_mutex);
    _policy = std::move(policy);
}

RealmPolicy RealmList::GetPolicy() const
{
    std::shared_lock const lock(_mutex);
    return _policy;
}

void RealmList::Replace(std::vector<Realm> realms)
{
    std::unique_lock const lock(_mutex);
    _realms = std::move(realms);
}

std::vector<Realm> RealmList::All() const
{
    std::shared_lock const lock(_mutex);
    return _realms;
}

bool RealmList::IsOnline(Realm const& realm, RealmPolicy const& policy, int64 nowEpoch) noexcept
{
    if (realm.MarkedOffline())
        return false;
    if (realm.LastHeartbeatEpoch <= 0)
        return false;
    return realm.LastHeartbeatEpoch >= policy.OldestLiveHeartbeat(nowEpoch);
}

std::vector<Realm> RealmList::Online(int64 nowEpoch) const
{
    std::shared_lock const lock(_mutex);
    std::vector<Realm> live;
    live.reserve(_realms.size());
    for (Realm const& realm : _realms)
        if (IsOnline(realm, _policy, nowEpoch))
            live.push_back(realm);
    return live;
}

std::optional<Realm> RealmList::Find(std::string_view name) const
{
    std::shared_lock const lock(_mutex);
    for (Realm const& realm : _realms)
        if (Ambrose::EqualsIgnoreCase(realm.Name, name))
            return realm;
    return std::nullopt;
}

std::optional<Realm> RealmList::Choose(std::vector<Realm> const& realms, RealmPolicy const& policy, std::string_view named, int64 nowEpoch)
{
    auto pickNamed = [&](std::string_view wanted) -> std::optional<Realm>
    {
        for (Realm const& realm : realms)
            if (Ambrose::EqualsIgnoreCase(realm.Name, wanted) && IsOnline(realm, policy, nowEpoch))
                return realm;
        return std::nullopt;
    };

    if (!Ambrose::Trim(named).empty())
        return pickNamed(Ambrose::Trim(named));
    if (!Ambrose::Trim(policy.DefaultRealm).empty())
    {
        if (std::optional<Realm> chosen = pickNamed(Ambrose::Trim(policy.DefaultRealm)))
            return chosen;
    }

    Realm const* least = nullptr;
    for (Realm const& realm : realms)
    {
        if (!IsOnline(realm, policy, nowEpoch) || realm.Full())
            continue;
        if (least == nullptr || realm.Population < least->Population)
            least = &realm;
    }
    if (least != nullptr)
        return *least;
    return std::nullopt;
}

std::optional<Realm> RealmList::Choose(std::string_view named, int64 nowEpoch) const
{
    std::shared_lock const lock(_mutex);
    return Choose(_realms, _policy, named, nowEpoch);
}
