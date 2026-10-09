/*
 * Project Ambrose by Imjustchico
 * Keeps a FIFO queue for each full realm, reserves slots while a successful handoff becomes an online character, releases waiters from refreshed realm capacity, sends live queue positions, and removes disconnected sessions.
 */

#ifndef AMBROSE_ADMISSIONQUEUE_H
#define AMBROSE_ADMISSIONQUEUE_H

#include "LoginMessages.h"
#include "RealmList.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class LoginSession;

struct AdmissionQueueRequest
{
    bool Admitted = false;
    uint32 Position = 0;
};

class AdmissionQueue
{
public:
    using Clock = std::chrono::steady_clock;

    static AdmissionQueue& Instance();

    AdmissionQueueRequest Request(Realm const& realm, std::shared_ptr<LoginSession> const& session, LoginMessages::CharacterSelected const& reply, bool bypass);
    void OnRealmRefresh(std::vector<Realm> const& realms, std::unordered_set<uint64> const& onlineCharacterGuids);
    void Update(std::chrono::milliseconds diff);
    uint64 QueuedCount() const;
    void RemoveSession(LoginSession const* session);
    void CancelReservation(uint32 realmId, uint64 characterGuid);
    void Reset();

private:
    struct Entry
    {
        uint16 SessionId = 0;
        std::weak_ptr<LoginSession> Session;
        LoginMessages::CharacterSelected Reply;
        std::string RealmName;
    };

    struct Reservation
    {
        uint64 CharacterGuid = 0;
        Clock::time_point Expires;
    };

    static uint32 PositionAt(std::size_t index) noexcept;
    void RemoveSessionLocked(uint16 sessionId);
    static void ExpireReservations(std::vector<Reservation>& reservations, Clock::time_point now);

    mutable std::mutex _mutex;
    std::unordered_map<uint32, std::deque<Entry>> _queues;
    std::unordered_map<uint32, std::vector<Reservation>> _reservations;
    std::chrono::milliseconds _positionUpdatesElapsed{ 0 };
};

#define sAdmissionQueue AdmissionQueue::Instance()

#endif
