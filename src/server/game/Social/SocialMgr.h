/*
 * Project Ambrose by Imjustchico
 * Owns realm-wide online friend presence, naming a friend's zone by the locale key of its display name as the client looks it up, cached friend and ignore records and pending requests, using asynchronous character-database reads, queued writes and a live friend cap.
 */

#ifndef AMBROSE_SOCIALMGR_H
#define AMBROSE_SOCIALMGR_H

#include "CharacterSummary.h"
#include "GameMessages.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <string_view>
#include <utility>
#include <vector>

class GameSession;
class ZoneTemplates;
class QueryCallback;

struct SocialFriend
{
    uint8 BestFriendSymbol = 0;
    uint64 Date = 0;
    std::string PackedName;
};

struct SocialIgnore
{
    int32 PlatformType = 0;
    std::string PackedName;
};

class SocialLists
{
public:
    bool AddFriend(uint64 characterId, uint64 friendDate = 0, uint8 bestFriendSymbol = 0, std::string packedName = {});
    bool RemoveFriend(uint64 characterId);
    bool AddIgnore(uint64 characterId, int32 platformType = 0, std::string packedName = {});
    bool RemoveIgnore(uint64 characterId);
    bool IsFriend(uint64 characterId) const;
    bool IsIgnored(uint64 characterId) const;
    bool ShouldRelayChatFrom(uint64 characterId) const noexcept { return !IsIgnored(characterId); }
    uint64 GetFriendDate(uint64 characterId) const noexcept;
    uint8 GetBestFriendSymbol(uint64 characterId) const noexcept;
    std::size_t FriendCount() const noexcept { return _friends.size(); }
    bool CanAddFriend(uint32 maximum) const noexcept { return _friends.size() < maximum; }
    std::map<uint64, SocialFriend> const& GetFriends() const noexcept { return _friends; }
    std::map<uint64, SocialIgnore> const& GetIgnores() const noexcept { return _ignores; }
    void SetBestFriendSymbol(uint64 characterId, uint8 symbol) noexcept;
    void Replace(std::map<uint64, SocialFriend> friends, std::map<uint64, SocialIgnore> ignores);

private:
    std::map<uint64, SocialFriend> _friends;
    std::map<uint64, SocialIgnore> _ignores;
};

class SocialMgr
{
public:
    static SocialMgr& Instance();

    SocialMgr(SocialMgr const&) = delete;
    SocialMgr& operator=(SocialMgr const&) = delete;

    static std::string FriendZoneName(std::string_view zonePath, ZoneTemplates const* templates);
    static bool CanAcceptFriendRequest(bool requestExists) noexcept { return requestExists; }
    static bool CanRequestFriend(uint32 friendCount, uint32 maximum) noexcept { return friendCount < maximum; }
    static bool IsRequestOwnerForCharacter(uint64 requestedOwnerId, uint64 characterId) noexcept
    {
        return characterId != 0 && (requestedOwnerId == 0 || requestedOwnerId == characterId);
    }
    static bool IsIncomingRequestForCharacter(uint64 requesterId, uint64 requestedCharacterId, uint64 characterId) noexcept
    {
        return characterId != 0 && requesterId != 0 && requesterId != characterId &&
               (requestedCharacterId == 0 || requestedCharacterId == characterId);
    }

    void SendLists(GameSession& session);
    void AddFriendRequest(GameSession& session, GameMessages::BuddyRequestAdd const& message);
    void AcceptFriendRequest(GameSession& session, GameMessages::BuddyRequestAccept const& message);
    void DenyFriendRequest(GameSession& session, GameMessages::BuddyRequestDeny const& message);
    void DropFriendRequest(GameSession& session, GameMessages::BuddyRequestDrop const& message);
    void SetBestFriend(GameSession& session, GameMessages::BestFriend const& message);
    void SendMaximumFriends(GameSession& session, GameMessages::RequestMaxFriends const& message);
    void AddIgnore(GameSession& session, GameMessages::IgnoreAdd const& message);
    void DropIgnore(GameSession& session, GameMessages::IgnoreDrop const& message);
    void UpdatePresence(std::vector<std::shared_ptr<GameSession>> const& sessions);

    bool IsIgnored(uint64 ownerId, uint64 speakerId) const noexcept;
    bool IsFriend(uint64 ownerId, uint64 friendId) const noexcept;
    bool ShouldRelayChat(uint64 ownerId, uint64 speakerId) const noexcept;
    void Clear();

private:
    struct OnlinePlayer
    {
        std::weak_ptr<GameSession> Session;
        uint8 Status = 0;
        std::string ZoneName;
    };

    SocialMgr() = default;

    std::map<uint64, SocialLists> _lists;
    std::map<uint64, OnlinePlayer> _online;
    std::unordered_set<uint64> _loaded;
    std::set<std::pair<uint64, uint64>> _pendingRequests;

    SocialLists& Lists(uint64 characterId);
    void LoadLists(GameSession& session, uint64 characterId, std::function<void(bool)> completion);
    void LoadCharacter(GameSession& session, uint64 characterId, std::function<void(std::optional<CharacterSummary>)> completion);
    void CheckRequestExists(GameSession& session, uint64 requesterId, uint64 targetId, std::function<void(bool)> completion);
    void QueueQuery(GameSession& session, QueryCallback&& query);
    void QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work);
    void FinishAcceptFriendRequest(GameSession& session, uint64 ownerId, uint64 requesterId);
    void SendFriendEntry(GameSession& session, uint64 friendId, std::string const& friendPackedName, uint64 friendDate, uint64 friendStatusDate);
    void SendIgnoreList(GameSession& session, bool addOne = false, uint64 characterId = 0);
    void SendPendingRequests(GameSession& session, std::function<void()> completion);
    void SendChatError(GameSession& session, uint64 characterId);
    void SendPresenceToFriends(uint64 characterId, uint8 status, std::string const& zoneName);
};

#define sSocialMgr SocialMgr::Instance()

#endif
