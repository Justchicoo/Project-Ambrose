/*
 * Project Ambrose by Imjustchico
 * The realms the login server may send a player to (sRealmList), and the rules that decide which one. A realm is online when its last heartbeat is newer than the age a missed run of heartbeats makes it, so a gameserver that stopped saying anything falls out on its own rather than waiting to be marked down. Normal choosing follows the named realm, operator default and heartbeat population; admission choosing uses online-character counts and retains a full realm so its queue can handle the selection. It holds no database of its own and is filled by whoever loaded the rows.
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
    uint32 OnlineCharacters = 0;
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

    void Replace(std::vector<Realm> realms, int64 readAtEpoch = 0);
    std::string Describe(int64 nowEpoch) const;
    std::vector<Realm> All() const;
    std::vector<Realm> Online(int64 nowEpoch) const;
    std::optional<Realm> Find(std::string_view name) const;
    std::optional<Realm> Choose(std::string_view named, int64 nowEpoch) const;
    std::optional<Realm> ChooseForAdmission(std::string_view named, int64 nowEpoch) const;

    static bool IsOnline(Realm const& realm, RealmPolicy const& policy, int64 nowEpoch) noexcept;
    static std::optional<Realm> Choose(std::vector<Realm> const& realms, RealmPolicy const& policy, std::string_view named, int64 nowEpoch);
    static std::optional<Realm> ChooseForAdmission(std::vector<Realm> const& realms, RealmPolicy const& policy, std::string_view named, int64 nowEpoch);

private:
    RealmList() = default;

    mutable std::shared_mutex _mutex;
    std::vector<Realm> _realms;
    RealmPolicy _policy;
    int64 _readAtEpoch = 0;
};

#define sRealmList RealmList::Instance()

#endif
