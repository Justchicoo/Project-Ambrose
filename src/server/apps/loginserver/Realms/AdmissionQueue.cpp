/*
 * Project Ambrose by Imjustchico
 * Queues authenticated selections in FIFO order per realm, reserves handoffs until a refresh sees the reserved character online or its key expires, updates queue positions live through MSG_USER_ADMIT_IND Status=2, and releases as many waiters as each refreshed player limit permits.
 */

#include "AdmissionQueue.h"

#include "LoginMgr.h"
#include "LoginSession.h"
#include "Settings.h"

#include <algorithm>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

namespace
{
    int64 NowEpochSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    AdmissionQueue::Clock::time_point ReservationExpiry()
    {
        return AdmissionQueue::Clock::now() + sLoginMgr.GetSettings()->KeyTtl + std::chrono::seconds(30);
    }
}

AdmissionQueue& AdmissionQueue::Instance()
{
    static AdmissionQueue instance;
    return instance;
}

uint32 AdmissionQueue::PositionAt(std::size_t index) noexcept
{
    return static_cast<uint32>(std::min<std::size_t>(index + 1, static_cast<std::size_t>(std::numeric_limits<int32>::max())));
}

void AdmissionQueue::RemoveSessionLocked(uint16 sessionId)
{
    for (auto queue = _queues.begin(); queue != _queues.end();)
    {
        std::deque<Entry>& entries = queue->second;
        std::erase_if(entries, [sessionId](Entry const& entry) { return entry.SessionId == sessionId; });
        if (entries.empty())
            queue = _queues.erase(queue);
        else
            ++queue;
    }
}

void AdmissionQueue::ExpireReservations(std::vector<Reservation>& reservations, Clock::time_point now)
{
    std::erase_if(reservations, [now](Reservation const& reservation) { return reservation.Expires <= now; });
}

AdmissionQueueRequest AdmissionQueue::Request(Realm const& realm, std::shared_ptr<LoginSession> const& session,
    LoginMessages::CharacterSelected const& reply, bool bypass)
{
    if (!session)
        return {};

    std::lock_guard const lock(_mutex);
    Clock::time_point const now = Clock::now();
    RemoveSessionLocked(session->GetSessionId());

    std::vector<Reservation>& reservations = _reservations[realm.Id];
    ExpireReservations(reservations, now);

    auto queue = _queues.find(realm.Id);
    if (queue != _queues.end())
    {
        std::erase_if(queue->second, [](Entry const& entry)
        {
            std::shared_ptr<LoginSession> session = entry.Session.lock();
            return !session || !session->IsOpen() || session->IsKicked();
        });
        if (queue->second.empty())
        {
            _queues.erase(queue);
            queue = _queues.end();
        }
    }

    uint64 const occupied = static_cast<uint64>(realm.OnlineCharacters) + reservations.size();
    bool const hasCapacity = realm.PlayerLimit == 0 || occupied < realm.PlayerLimit;
    if (bypass || (hasCapacity && queue == _queues.end()))
    {
        reservations.push_back({ reply.CharId, ReservationExpiry() });
        return { true, 0 };
    }

    std::deque<Entry>& entries = _queues[realm.Id];
    entries.push_back({ session->GetSessionId(), session, reply, realm.Name });
    return { false, PositionAt(entries.size() - 1) };
}

void AdmissionQueue::OnRealmRefresh(std::vector<Realm> const& realms, std::unordered_set<uint64> const& onlineCharacterGuids)
{
    std::vector<std::pair<std::shared_ptr<LoginSession>, uint32>> positions;
    std::vector<std::tuple<std::shared_ptr<LoginSession>, uint64, std::string>> failures;
    std::vector<std::tuple<std::shared_ptr<LoginSession>, LoginMessages::CharacterSelected, std::string, uint32>> admissions;
    std::unordered_map<uint32, Realm const*> byId;
    byId.reserve(realms.size());
    for (Realm const& realm : realms)
        byId.emplace(realm.Id, &realm);

    RealmPolicy const policy = sRealmList.GetPolicy();
    int64 const nowEpoch = NowEpochSeconds();
    Clock::time_point const now = Clock::now();

    {
        std::lock_guard const lock(_mutex);

        for (auto reservations = _reservations.begin(); reservations != _reservations.end();)
        {
            std::erase_if(reservations->second, [&onlineCharacterGuids](Reservation const& reservation)
            {
                return onlineCharacterGuids.contains(reservation.CharacterGuid);
            });
            ExpireReservations(reservations->second, now);
            if (!byId.contains(reservations->first) || reservations->second.empty())
                reservations = _reservations.erase(reservations);
            else
                ++reservations;
        }

        for (auto queue = _queues.begin(); queue != _queues.end();)
        {
            auto realm = byId.find(queue->first);
            if (realm == byId.end() || !RealmList::IsOnline(*realm->second, policy, nowEpoch))
            {
                for (Entry const& entry : queue->second)
                    if (std::shared_ptr<LoginSession> session = entry.Session.lock(); session && session->IsOpen() && !session->IsKicked())
                        failures.emplace_back(std::move(session), entry.Reply.CharId, "the chosen realm is no longer online");
                queue = _queues.erase(queue);
                continue;
            }

            Realm const& currentRealm = *realm->second;
            std::erase_if(queue->second, [](Entry const& entry)
            {
                std::shared_ptr<LoginSession> session = entry.Session.lock();
                return !session || !session->IsOpen() || session->IsKicked();
            });
            if (queue->second.empty())
            {
                queue = _queues.erase(queue);
                continue;
            }

            std::vector<Reservation>& reservations = _reservations[currentRealm.Id];
            uint64 occupied = static_cast<uint64>(currentRealm.OnlineCharacters) + reservations.size();

            while (!queue->second.empty() && (currentRealm.PlayerLimit == 0 || occupied < currentRealm.PlayerLimit))
            {
                Entry entry = std::move(queue->second.front());
                queue->second.pop_front();
                std::shared_ptr<LoginSession> session = entry.Session.lock();
                if (!session || !session->IsOpen() || session->IsKicked())
                    continue;

                reservations.push_back({ entry.Reply.CharId, ReservationExpiry() });
                ++occupied;
                admissions.emplace_back(std::move(session), std::move(entry.Reply), std::move(entry.RealmName), currentRealm.Id);
            }

            if (queue->second.empty())
            {
                queue = _queues.erase(queue);
                continue;
            }

            std::size_t index = 0;
            for (Entry const& entry : queue->second)
            {
                if (std::shared_ptr<LoginSession> session = entry.Session.lock(); session && session->IsOpen() && !session->IsKicked())
                    positions.emplace_back(std::move(session), PositionAt(index));
                ++index;
            }
            ++queue;
        }

    }

    for (auto& [session, character, realmName, realmId] : admissions)
        session->AdmitQueuedCharacter(std::move(character), std::move(realmName), realmId);
    for (auto& [session, position] : positions)
        session->NotifyAdmissionQueuePosition(position);
    for (auto& [session, characterGuid, detail] : failures)
        session->FailQueuedCharacter(characterGuid, std::move(detail));
}

void AdmissionQueue::Update(std::chrono::milliseconds diff)
{
    uint32 const intervalSeconds = sSettings.Get<uint32>("Queue.PositionUpdateInterval");
    std::vector<std::pair<std::shared_ptr<LoginSession>, uint32>> updates;

    {
        std::lock_guard const lock(_mutex);
        _positionUpdatesElapsed += diff;
        if (_positionUpdatesElapsed < std::chrono::seconds(intervalSeconds))
            return;
        _positionUpdatesElapsed = std::chrono::milliseconds::zero();

        for (auto queue = _queues.begin(); queue != _queues.end();)
        {
            std::deque<Entry>& entries = queue->second;
            std::erase_if(entries, [](Entry const& entry)
            {
                std::shared_ptr<LoginSession> session = entry.Session.lock();
                return !session || !session->IsOpen() || session->IsKicked();
            });
            if (entries.empty())
            {
                queue = _queues.erase(queue);
                continue;
            }

            std::size_t index = 0;
            for (Entry const& entry : entries)
            {
                if (std::shared_ptr<LoginSession> session = entry.Session.lock(); session && session->IsOpen() && !session->IsKicked())
                    updates.emplace_back(std::move(session), PositionAt(index));
                ++index;
            }
            ++queue;
        }
    }

    for (auto& [session, position] : updates)
        session->NotifyAdmissionQueuePosition(position);
}

uint64 AdmissionQueue::QueuedCount() const
{
    std::lock_guard const lock(_mutex);
    uint64 count = 0;
    for (auto const& [realmId, entries] : _queues)
    {
        static_cast<void>(realmId);
        count += entries.size();
    }
    return count;
}

void AdmissionQueue::RemoveSession(LoginSession const* session)
{
    if (!session)
        return;
    std::lock_guard const lock(_mutex);
    RemoveSessionLocked(session->GetSessionId());
}

void AdmissionQueue::CancelReservation(uint32 realmId, uint64 characterGuid)
{
    std::lock_guard const lock(_mutex);
    auto reservations = _reservations.find(realmId);
    if (reservations == _reservations.end())
        return;
    std::erase_if(reservations->second, [characterGuid](Reservation const& reservation) { return reservation.CharacterGuid == characterGuid; });
    if (reservations->second.empty())
        _reservations.erase(reservations);
}

void AdmissionQueue::Reset()
{
    std::lock_guard const lock(_mutex);
    _queues.clear();
    _reservations.clear();
    _positionUpdatesElapsed = std::chrono::milliseconds::zero();
}
