/*
 * Project Ambrose by Imjustchico
 * The realms the login server may send a player to (sRealmList), and the rule that decides which one. A realm is online when its last heartbeat is newer than the age a missed run of heartbeats makes it, so a gameserver that stopped saying anything falls out on its own rather than waiting to be marked down, and a clock that has not moved yet leaves every realm online rather than emptying the list. Choosing reads in one order: the realm the client named, else the one the operator named, else the least full of those online, so a named realm that is down is a refusal rather than a silent move to another. It holds no database of its own and is filled by whoever loaded the rows, because the login server reads them from its database and a test writes them by hand.
 */

#ifndef AMBROSE_REALMLIST_H
#define AMBROSE_REALMLIST_H

#include "Types.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

enum RealmFlags : uint32
{
    REALM_FLAG_NONE = 0x00,
    REALM_FLAG_OFFLINE = 0x01,
    REALM_FLAG_RECOMMENDED = 0x02,
    REALM_FLAG_FULL = 0x04,
    REALM_FLAG_TEST = 0x08
};

struct Realm
{
    uint32 Id = 0;
    std::string Name;
    std::string Address;
    std::string LocalAddress;
    uint16 Port = 0;
    uint32 Flags = REALM_FLAG_NONE;
    uint32 Population = 0;
    uint32 PlayerLimit = 0;
    int64 LastHeartbeatEpoch = 0;

    bool MarkedOffline() const noexcept { return (Flags & REALM_FLAG_OFFLINE) != 0; }
    bool Full() const noexcept { return PlayerLimit != 0 && Population >= PlayerLimit; }
};

struct RealmPolicy
{
    static constexpr uint32 DefaultHeartbeatSeconds = 30;
    static constexpr uint32 DefaultOfflineAfterIntervals = 3;

    uint32 HeartbeatSeconds = DefaultHeartbeatSeconds;
    uint32 OfflineAfterIntervals = DefaultOfflineAfterIntervals;
    std::string DefaultRealm;

    int64 OldestLiveHeartbeat(int64 nowEpoch) const noexcept;
};

class RealmList
{
public:
    static RealmList& Instance();

    RealmList(RealmList const&) = delete;
    RealmList& operator=(RealmList const&) = delete;

    void SetPolicy(RealmPolicy policy);
    RealmPolicy GetPolicy() const;

    void Replace(std::vector<Realm> realms);
    std::vector<Realm> All() const;
    std::vector<Realm> Online(int64 nowEpoch) const;
    std::optional<Realm> Find(std::string_view name) const;
    std::optional<Realm> Choose(std::string_view named, int64 nowEpoch) const;

    static bool IsOnline(Realm const& realm, RealmPolicy const& policy, int64 nowEpoch) noexcept;
    static std::optional<Realm> Choose(std::vector<Realm> const& realms, RealmPolicy const& policy, std::string_view named, int64 nowEpoch);

private:
    RealmList() = default;

    mutable std::shared_mutex _mutex;
    std::vector<Realm> _realms;
    RealmPolicy _policy;
};

#define sRealmList RealmList::Instance()

#endif
