/*
 * Project Ambrose by Imjustchico
 * Persists friends, requests and ignores, emits the client's buddy messages from its installed definitions, keeps live presence for this realm and filters radial chat through the owner's ignore list.
 */

#include "SocialMgr.h"

#include "CharacterDatabase.h"
#include "CharacterNameMgr.h"
#include "CharacterRepository.h"
#include "DatabaseEnv.h"
#include "GameSession.h"
#include "Log.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "PropertyFiller.h"
#include "QueryResult.h"
#include "Settings.h"
#include "TypeRegistry.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr uint8 PlayerStatusOffline = 1;
    constexpr uint8 PlayerStatusLinkDead = 2;
    constexpr uint8 PlayerStatusOnline = 4;
    constexpr uint32 ChatErrorGeneric = 1;

    struct FriendRow
    {
        uint64 CharacterId = 0;
        uint8 BestFriendSymbol = 0;
        uint64 Date = 0;
        std::string Name;
    };

    struct IgnoreRow
    {
        uint64 CharacterId = 0;
        int32 PlatformType = 0;
        std::string Name;
    };

    struct RequestRow
    {
        uint64 CharacterId = 0;
        std::string Name;
        int32 Level = 1;
    };

    using Statement = std::unique_ptr<PreparedStatement<CharacterDatabaseConnection>>;

    int64 NowEpochSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    uint32 Date32(uint64 date)
    {
        return static_cast<uint32>(std::min<uint64>(date, std::numeric_limits<uint32>::max()));
    }

    std::string CharacterName(Field const& customName, Field const& nameIndices, Field const& gender)
    {
        if (!customName.IsNull())
            return customName.Get<std::string>();
        return sCharacterNameMgr.FormatName(nameIndices.Get<uint32>(), gender.Get<uint32>()).value_or(std::string());
    }

    Statement Prepare(CharacterDatabaseStatements id)
    {
        return CharacterDatabase.GetPreparedStatement(id);
    }

    bool LoadFriends(uint64 ownerId, std::vector<FriendRow>& friends)
    {
        Statement statement = Prepare(CHAR_SEL_SOCIAL_FRIENDS);
        if (!statement || !CharacterDatabase.IsOpen())
            return false;
        statement->SetData(0, ownerId);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result))
            return false;
        if (!result)
            return true;
        if (result->GetRowCount() == 0)
            return true;
        PreparedResultSet const& row = *result;
        do
        {
            friends.push_back({ row[0].Get<uint64>(), row[1].Get<uint8>(), row[2].Get<uint64>(),
                CharacterName(row[3], row[4], row[5]) });
        } while (result->NextRow());
        return true;
    }

    bool LoadIgnores(uint64 ownerId, std::vector<IgnoreRow>& ignores)
    {
        Statement statement = Prepare(CHAR_SEL_SOCIAL_IGNORES);
        if (!statement || !CharacterDatabase.IsOpen())
            return false;
        statement->SetData(0, ownerId);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result))
            return false;
        if (!result)
            return true;
        if (result->GetRowCount() == 0)
            return true;
        PreparedResultSet const& row = *result;
        do
        {
            ignores.push_back({ row[0].Get<uint64>(), row[1].Get<int32>(), CharacterName(row[2], row[3], row[4]) });
        } while (result->NextRow());
        return true;
    }

    bool LoadRequests(uint64 targetId, std::vector<RequestRow>& requests)
    {
        Statement statement = Prepare(CHAR_SEL_SOCIAL_REQUESTS);
        if (!statement || !CharacterDatabase.IsOpen())
            return false;
        statement->SetData(0, targetId);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result))
            return false;
        if (!result)
            return true;
        if (result->GetRowCount() == 0)
            return true;
        PreparedResultSet const& row = *result;
        do
        {
            requests.push_back({ row[0].Get<uint64>(), CharacterName(row[1], row[2], row[3]), row[4].Get<int32>() });
        } while (result->NextRow());
        return true;
    }

    std::optional<uint32> FriendCount(uint64 ownerId)
    {
        Statement statement = Prepare(CHAR_SEL_SOCIAL_FRIEND_COUNT);
        if (!statement || !CharacterDatabase.IsOpen())
            return std::nullopt;
        statement->SetData(0, ownerId);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result) || !result)
            return std::nullopt;
        return (*result)[0].Get<uint32>();
    }

    std::optional<bool> HasRequest(uint64 requesterId, uint64 targetId)
    {
        Statement statement = Prepare(CHAR_SEL_SOCIAL_REQUEST_EXISTS);
        if (!statement || !CharacterDatabase.IsOpen())
            return std::nullopt;
        statement->SetData(0, requesterId);
        statement->SetData(1, targetId);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result))
            return std::nullopt;
        return result && result->GetRowCount() != 0;
    }

    std::optional<std::string> EncodeIgnoreList(std::vector<IgnoreRow> const& rows, std::optional<uint64> onlyId)
    {
        TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
        PropertyObjectPtr const list = catalog ? PropertyObject::Create(catalog, "class IgnoreEntryDataList") : nullptr;
        ObjectField const* const field = ObjectFields::Find("MSG_IGNORELIST", "ListData");
        if (!list || !field)
        {
            LOG_ERROR("server.social", "Cannot encode the ignore list: {}", !list ? "the loaded type dump has no class IgnoreEntryDataList" :
                "the loaded message definitions do not describe MSG_IGNORELIST.ListData");
            return std::nullopt;
        }

        std::string problem;
        PropertyValue::List entries;
        for (IgnoreRow const& row : rows)
        {
            if (onlyId && row.CharacterId != *onlyId)
                continue;
            PropertyObjectPtr entry = PropertyObject::Create(catalog, "class IgnoreEntryData");
            if (!entry)
            {
                LOG_ERROR("server.social", "Cannot encode the ignore list: the loaded type dump has no class IgnoreEntryData");
                return std::nullopt;
            }
            PropertyFiller(*entry, problem)
                .Set("m_ignoreName", row.Name)
                .Set("m_characterID", row.CharacterId)
                .Set("m_gameObjectID", uint64{ 0 })
                .Set("m_platformType", row.PlatformType);
            if (!problem.empty())
            {
                LOG_ERROR("server.social", "Cannot encode an ignore-list entry: {}", problem);
                return std::nullopt;
            }
            entries.emplace_back(std::move(entry));
        }

        PropertyFiller(*list, problem).Set("m_ignoreDataList", std::move(entries));
        if (!problem.empty())
        {
            LOG_ERROR("server.social", "Cannot encode the ignore list: {}", problem);
            return std::nullopt;
        }
        EncodeResult const encoded = ObjectSerializer::EncodeField(*field, list.get());
        if (!encoded.Ok())
        {
            LOG_ERROR("server.social", "Cannot encode the ignore list: {}", encoded.Detail);
            return std::nullopt;
        }
        return std::string(encoded.Bytes.begin(), encoded.Bytes.end());
    }

    CharacterLoad LoadCharacter(uint64 characterId)
    {
        return CharacterRepository::Load(characterId);
    }
}

bool SocialLists::AddFriend(uint64 characterId, uint64 friendDate)
{
    if (characterId == 0)
        return false;
    bool const inserted = _friends.insert(characterId).second;
    _friendDates[characterId] = friendDate;
    return inserted;
}

bool SocialLists::RemoveFriend(uint64 characterId)
{
    _friendDates.erase(characterId);
    return _friends.erase(characterId) != 0;
}

bool SocialLists::AddIgnore(uint64 characterId)
{
    if (characterId == 0)
        return false;
    _friends.erase(characterId);
    _friendDates.erase(characterId);
    return _ignores.insert(characterId).second;
}

bool SocialLists::RemoveIgnore(uint64 characterId)
{
    return _ignores.erase(characterId) != 0;
}

bool SocialLists::IsFriend(uint64 characterId) const
{
    return _friends.contains(characterId);
}

bool SocialLists::IsIgnored(uint64 characterId) const
{
    return _ignores.contains(characterId);
}

uint64 SocialLists::GetFriendDate(uint64 characterId) const noexcept
{
    auto const found = _friendDates.find(characterId);
    return found == _friendDates.end() ? 0 : found->second;
}

void SocialLists::Replace(std::set<uint64> friends, std::set<uint64> ignores, std::map<uint64, uint64> friendDates)
{
    for (uint64 const ignoredId : ignores)
    {
        friends.erase(ignoredId);
        friendDates.erase(ignoredId);
    }
    _friends = std::move(friends);
    _ignores = std::move(ignores);
    _friendDates = std::move(friendDates);
}

SocialMgr& SocialMgr::Instance()
{
    static SocialMgr manager;
    return manager;
}

SocialLists& SocialMgr::Lists(uint64 characterId)
{
    return _lists[characterId];
}

bool SocialMgr::LoadLists(uint64 characterId)
{
    if (_loaded.contains(characterId))
        return true;
    std::vector<FriendRow> friends;
    std::vector<IgnoreRow> ignores;
    if (!LoadFriends(characterId, friends) || !LoadIgnores(characterId, ignores))
    {
        LOG_ERROR("server.social", "Could not load the friend and ignore lists for wizard {}", characterId);
        return false;
    }
    std::set<uint64> friendIds;
    std::set<uint64> ignoreIds;
    std::map<uint64, uint64> friendDates;
    for (FriendRow const& friendRow : friends)
    {
        friendIds.insert(friendRow.CharacterId);
        friendDates.emplace(friendRow.CharacterId, friendRow.Date);
    }
    for (IgnoreRow const& ignoreRow : ignores)
        ignoreIds.insert(ignoreRow.CharacterId);
    Lists(characterId).Replace(std::move(friendIds), std::move(ignoreIds), std::move(friendDates));
    _loaded.insert(characterId);
    return true;
}

bool SocialMgr::IsIgnored(uint64 ownerId, uint64 speakerId) const noexcept
{
    auto const owner = _lists.find(ownerId);
    return owner != _lists.end() && owner->second.IsIgnored(speakerId);
}

bool SocialMgr::ShouldRelayChat(uint64 ownerId, uint64 speakerId) const noexcept
{
    auto const owner = _lists.find(ownerId);
    return owner == _lists.end() || owner->second.ShouldRelayChatFrom(speakerId);
}

bool SocialMgr::IsFriend(uint64 ownerId, uint64 friendId) const noexcept
{
    auto const owner = _lists.find(ownerId);
    return owner != _lists.end() && owner->second.IsFriend(friendId);
}

void SocialMgr::Clear()
{
    _lists.clear();
    _online.clear();
    _loaded.clear();
}

void SocialMgr::SendChatError(GameSession& session, uint64 characterId)
{
    GameMessages::ChatError error;
    error.ListOwnerGid = session.GetCharacterId();
    error.CharacterId = characterId;
    error.Error = ChatErrorGeneric;
    session.SendDmlMessage(error);
}

void SocialMgr::SendFriendEntry(GameSession& session, uint64 friendId, std::string const& friendName, uint64 friendDate, uint64 friendStatusDate)
{
    GameMessages::BuddyEntry entry;
    entry.ListOwnerGid = session.GetCharacterId();
    entry.EntryGid = friendId;
    entry.Name = friendName;
    entry.FriendInfo = 0;
    entry.Permissions = 0;
    entry.RealmName = sSettings.Get<std::string>("Realm.Name");
    entry.FriendDate = Date32(friendDate);
    entry.FriendStatusDate = Date32(friendStatusDate);
    entry.Status = PlayerStatusOffline;
    if (auto const online = _online.find(friendId); online != _online.end())
    {
        if (std::shared_ptr<GameSession> const other = online->second.Session.lock())
        {
            entry.GameObjectId = other->GetWorldGuid();
            entry.Status = online->second.Status;
            entry.ZoneName = online->second.ZoneName;
        }
    }
    session.SendDmlMessage(entry);
}

void SocialMgr::SendIgnoreList(GameSession& session, bool addOne, uint64 characterId)
{
    std::vector<IgnoreRow> rows;
    if (!LoadIgnores(session.GetCharacterId(), rows))
    {
        LOG_ERROR("server.social", "Could not load the ignore list for wizard {}", session.GetCharacterId());
        return;
    }
    std::optional<std::string> data = EncodeIgnoreList(rows, addOne ? std::optional<uint64>(characterId) : std::nullopt);
    if (!data)
        return;
    GameMessages::IgnoreList reply;
    reply.ListOwnerGid = session.GetCharacterId();
    reply.ListData = std::move(*data);
    reply.Add = addOne ? 1 : 0;
    session.SendDmlMessage(reply);
}

void SocialMgr::SendPendingRequests(GameSession& session)
{
    std::vector<RequestRow> requests;
    if (!LoadRequests(session.GetCharacterId(), requests))
    {
        LOG_ERROR("server.social", "Could not load incoming friend requests for wizard {}", session.GetCharacterId());
        return;
    }
    for (RequestRow const& request : requests)
    {
        GameMessages::BuddyRequestAdd message;
        message.ListOwnerGid = request.CharacterId;
        message.EntryGid = session.GetCharacterId();
        message.OwnerName = request.Name;
        message.OwnerLevel = static_cast<uint8>(std::clamp(request.Level, 0, static_cast<int32>(std::numeric_limits<uint8>::max())));
        session.SendDmlMessage(message);
    }
}

void SocialMgr::SendLists(GameSession& session)
{
    uint64 const ownerId = session.GetCharacterId();
    if (ownerId == 0 || !LoadLists(ownerId))
        return;
    std::vector<FriendRow> friends;
    if (!LoadFriends(ownerId, friends))
    {
        LOG_ERROR("server.social", "Could not load the friend list for wizard {}", ownerId);
        return;
    }
    for (FriendRow const& friendRow : friends)
    {
        SendFriendEntry(session, friendRow.CharacterId, friendRow.Name, friendRow.Date, friendRow.Date);
        if (friendRow.BestFriendSymbol != 0)
        {
            GameMessages::BestFriend bestFriend;
            bestFriend.ListOwnerGid = ownerId;
            bestFriend.BuddyId = friendRow.CharacterId;
            bestFriend.Forwarded = 1;
            bestFriend.FriendSymbol = friendRow.BestFriendSymbol;
            session.SendDmlMessage(bestFriend);
        }
    }
    SendIgnoreList(session);
    SendPendingRequests(session);
    GameMessages::BuddyListComplete complete;
    complete.ListOwnerGid = ownerId;
    session.SendDmlMessage(complete);
}

void SocialMgr::AddFriendRequest(GameSession& session, GameMessages::BuddyRequestAdd const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const targetId = message.EntryGid;
    if (message.ListOwnerGid != ownerId || targetId == 0 || targetId == ownerId)
    {
        LOG_WARN("server.social", "Session {} sent an invalid friend request from wizard {} to {}", session.GetSessionId(), message.ListOwnerGid, targetId);
        SendChatError(session, targetId);
        return;
    }
    if (message.Remove != 0)
    {
        Statement statement = Prepare(CHAR_DEL_SOCIAL_REQUEST);
        if (!statement || !CharacterDatabase.IsOpen())
        {
            LOG_ERROR("server.social", "Could not cancel friend request {} -> {}", ownerId, targetId);
            return;
        }
        statement->SetData(0, ownerId);
        statement->SetData(1, targetId);
        if (!CharacterDatabase.DirectExecute(*statement))
        {
            LOG_ERROR("server.social", "Could not cancel friend request {} -> {}", ownerId, targetId);
            return;
        }
        if (auto const target = _online.find(targetId); target != _online.end())
            if (std::shared_ptr<GameSession> recipient = target->second.Session.lock())
            {
                GameMessages::BuddyRequestDrop drop;
                drop.ListOwnerGid = targetId;
                drop.EntryGid = ownerId;
                recipient->SendDmlMessage(drop);
            }
        return;
    }

    if (!LoadLists(ownerId) || !LoadLists(targetId))
    {
        SendChatError(session, targetId);
        return;
    }
    if (Lists(ownerId).IsFriend(targetId) || Lists(targetId).IsFriend(ownerId) || Lists(targetId).IsIgnored(ownerId))
    {
        SendChatError(session, targetId);
        return;
    }

    CharacterLoad const target = LoadCharacter(targetId);
    CharacterLoad const owner = LoadCharacter(ownerId);
    if (target.Result != CharacterOpResult::Ok || !target.Character || target.Character->IsDeleted() ||
        owner.Result != CharacterOpResult::Ok || !owner.Character || owner.Character->IsDeleted())
    {
        LOG_DEBUG("server.social", "Session {} requested friendship with a missing, deleted or unavailable wizard", session.GetSessionId());
        SendChatError(session, targetId);
        return;
    }
    std::optional<uint32> const count = FriendCount(ownerId);
    if (!count)
    {
        LOG_ERROR("server.social", "Could not count friends for wizard {}", ownerId);
        SendChatError(session, targetId);
        return;
    }
    if (!CanRequestFriend(*count, sSettings.Get<uint32>("Social.MaxFriends")))
    {
        SendChatError(session, targetId);
        return;
    }

    Statement statement = Prepare(CHAR_INS_SOCIAL_REQUEST);
    if (!statement || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not persist friend request {} -> {}", ownerId, targetId);
        SendChatError(session, targetId);
        return;
    }
    statement->SetData(0, ownerId);
    statement->SetData(1, targetId);
    statement->SetData(2, static_cast<uint64>(NowEpochSeconds()));
    if (!CharacterDatabase.DirectExecute(*statement))
    {
        LOG_ERROR("server.social", "Could not persist friend request {} -> {}", ownerId, targetId);
        SendChatError(session, targetId);
        return;
    }
    if (auto const found = _online.find(targetId); found != _online.end())
        if (std::shared_ptr<GameSession> recipient = found->second.Session.lock())
        {
            GameMessages::BuddyRequestAdd request;
            request.ListOwnerGid = ownerId;
            request.EntryGid = targetId;
            request.OwnerName = owner.Character->CustomName.value_or(
                sCharacterNameMgr.FormatName(owner.Character->NameIndices, owner.Character->Appearance.Gender).value_or(std::string()));
            request.OwnerLevel = static_cast<uint8>(std::clamp(owner.Character->Level, 0, static_cast<int32>(std::numeric_limits<uint8>::max())));
            recipient->SendDmlMessage(request);
        }
}

void SocialMgr::AcceptFriendRequest(GameSession& session, GameMessages::BuddyRequestAccept const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const requesterId = message.EntryGid;
    if (message.ListOwnerGid != ownerId || requesterId == 0 || requesterId == ownerId)
    {
        LOG_WARN("server.social", "Session {} sent an invalid friend acceptance from wizard {} for {}", session.GetSessionId(), message.ListOwnerGid, requesterId);
        SendChatError(session, requesterId);
        return;
    }
    std::optional<bool> const requestExists = HasRequest(requesterId, ownerId);
    if (!requestExists)
    {
        LOG_ERROR("server.social", "Could not check incoming friend request {} -> {}", requesterId, ownerId);
        SendChatError(session, requesterId);
        return;
    }
    if (!CanAcceptFriendRequest(*requestExists))
    {
        SendChatError(session, requesterId);
        return;
    }
    std::optional<uint32> const ownerCount = FriendCount(ownerId);
    std::optional<uint32> const requesterCount = FriendCount(requesterId);
    if (!ownerCount || !requesterCount)
    {
        LOG_ERROR("server.social", "Could not count friends while accepting request {} -> {}", requesterId, ownerId);
        SendChatError(session, requesterId);
        return;
    }
    uint32 const maximum = sSettings.Get<uint32>("Social.MaxFriends");
    if (*ownerCount >= maximum || *requesterCount >= maximum)
    {
        SendChatError(session, requesterId);
        return;
    }
    if (!LoadLists(ownerId) || !LoadLists(requesterId))
    {
        SendChatError(session, requesterId);
        return;
    }

    uint64 const now = static_cast<uint64>(NowEpochSeconds());
    Statement ownerFriend = Prepare(CHAR_INS_SOCIAL_FRIEND);
    Statement requesterFriend = Prepare(CHAR_INS_SOCIAL_FRIEND);
    Statement deleteIncoming = Prepare(CHAR_DEL_SOCIAL_REQUEST);
    Statement deleteOutgoing = Prepare(CHAR_DEL_SOCIAL_REQUEST);
    if (!ownerFriend || !requesterFriend || !deleteIncoming || !deleteOutgoing || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not prepare the transaction accepting friend request {} -> {}", requesterId, ownerId);
        SendChatError(session, requesterId);
        return;
    }
    ownerFriend->SetData(0, ownerId);
    ownerFriend->SetData(1, requesterId);
    ownerFriend->SetData(2, uint8{ 0 });
    ownerFriend->SetData(3, now);
    requesterFriend->SetData(0, requesterId);
    requesterFriend->SetData(1, ownerId);
    requesterFriend->SetData(2, uint8{ 0 });
    requesterFriend->SetData(3, now);
    deleteIncoming->SetData(0, requesterId);
    deleteIncoming->SetData(1, ownerId);
    deleteOutgoing->SetData(0, ownerId);
    deleteOutgoing->SetData(1, requesterId);

    std::shared_ptr<Transaction<CharacterDatabaseConnection>> transaction = CharacterDatabase.BeginTransaction();
    transaction->Append(std::move(ownerFriend));
    transaction->Append(std::move(requesterFriend));
    transaction->Append(std::move(deleteIncoming));
    transaction->Append(std::move(deleteOutgoing));
    if (!CharacterDatabase.DirectCommitTransaction(transaction))
    {
        LOG_ERROR("server.social", "Could not commit accepted friend request {} -> {}", requesterId, ownerId);
        SendChatError(session, requesterId);
        return;
    }
    Lists(ownerId).AddFriend(requesterId, now);
    Lists(requesterId).AddFriend(ownerId, now);

    CharacterLoad const requester = LoadCharacter(requesterId);
    if (requester.Result == CharacterOpResult::Ok && requester.Character)
    {
        std::string const name = requester.Character->CustomName.value_or(
            sCharacterNameMgr.FormatName(requester.Character->NameIndices, requester.Character->Appearance.Gender).value_or(std::string()));
        SendFriendEntry(session, requesterId, name, now, now);
    }
    else
        LOG_ERROR("server.social", "Accepted friend {} -> {} but could not read the requester's name", requesterId, ownerId);

    if (auto const remote = _online.find(requesterId); remote != _online.end())
        if (std::shared_ptr<GameSession> recipient = remote->second.Session.lock())
        {
            std::string const name = session.GetCharacterName();
            SendFriendEntry(*recipient, ownerId, name, now, now);
        }
    SendPresenceToFriends(ownerId, PlayerStatusOnline, session.GetZoneDisplay());
    if (auto const remote = _online.find(requesterId); remote != _online.end())
        SendPresenceToFriends(requesterId, remote->second.Status, remote->second.ZoneName);
}

void SocialMgr::DenyFriendRequest(GameSession& session, GameMessages::BuddyRequestDeny const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const requesterId = message.EntryGid;
    if (message.ListOwnerGid != ownerId || requesterId == 0 || requesterId == ownerId)
    {
        LOG_WARN("server.social", "Session {} sent an invalid friend denial from wizard {} for {}", session.GetSessionId(), message.ListOwnerGid, requesterId);
        return;
    }
    Statement statement = Prepare(CHAR_DEL_SOCIAL_REQUEST);
    if (!statement || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not deny friend request {} -> {}", requesterId, ownerId);
        return;
    }
    statement->SetData(0, requesterId);
    statement->SetData(1, ownerId);
    if (!CharacterDatabase.DirectExecute(*statement))
    {
        LOG_ERROR("server.social", "Could not deny friend request {} -> {}", requesterId, ownerId);
        return;
    }
    if (auto const requester = _online.find(requesterId); requester != _online.end())
        if (std::shared_ptr<GameSession> recipient = requester->second.Session.lock())
        {
            GameMessages::BuddyRequestDeny denied;
            denied.ListOwnerGid = requesterId;
            denied.EntryGid = ownerId;
            recipient->SendDmlMessage(denied);
        }
}

void SocialMgr::DropFriendRequest(GameSession& session, GameMessages::BuddyRequestDrop const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const friendId = message.EntryGid;
    if (message.ListOwnerGid != ownerId || friendId == 0 || friendId == ownerId || !LoadLists(ownerId) || !LoadLists(friendId))
    {
        LOG_WARN("server.social", "Session {} sent an invalid friend removal for wizard {}", session.GetSessionId(), friendId);
        return;
    }
    if (!Lists(ownerId).IsFriend(friendId) && !Lists(friendId).IsFriend(ownerId))
        return;

    Statement ownerFriend = Prepare(CHAR_DEL_SOCIAL_FRIEND);
    Statement friendFriend = Prepare(CHAR_DEL_SOCIAL_FRIEND);
    if (!ownerFriend || !friendFriend || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not prepare removal of friendship {} <-> {}", ownerId, friendId);
        return;
    }
    ownerFriend->SetData(0, ownerId);
    ownerFriend->SetData(1, friendId);
    friendFriend->SetData(0, friendId);
    friendFriend->SetData(1, ownerId);
    std::shared_ptr<Transaction<CharacterDatabaseConnection>> transaction = CharacterDatabase.BeginTransaction();
    transaction->Append(std::move(ownerFriend));
    transaction->Append(std::move(friendFriend));
    if (!CharacterDatabase.DirectCommitTransaction(transaction))
    {
        LOG_ERROR("server.social", "Could not remove friendship {} <-> {}", ownerId, friendId);
        return;
    }
    Lists(ownerId).RemoveFriend(friendId);
    Lists(friendId).RemoveFriend(ownerId);

    GameMessages::BuddyDrop ownerDrop;
    ownerDrop.ListOwnerGid = ownerId;
    ownerDrop.EntryGid = friendId;
    session.SendDmlMessage(ownerDrop);
    if (auto const remote = _online.find(friendId); remote != _online.end())
        if (std::shared_ptr<GameSession> recipient = remote->second.Session.lock())
        {
            GameMessages::BuddyDrop remoteDrop;
            remoteDrop.ListOwnerGid = friendId;
            remoteDrop.EntryGid = ownerId;
            recipient->SendDmlMessage(remoteDrop);
        }
}

void SocialMgr::SetBestFriend(GameSession& session, GameMessages::BestFriend const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const friendId = message.BuddyId;
    if (message.ListOwnerGid != ownerId || friendId == 0 || friendId == ownerId || !LoadLists(ownerId) || !Lists(ownerId).IsFriend(friendId))
    {
        LOG_WARN("server.social", "Session {} tried to set a best-friend symbol for a wizard outside its friend list", session.GetSessionId());
        return;
    }
    Statement statement = Prepare(CHAR_UPD_SOCIAL_BEST_FRIEND);
    if (!statement || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not set best-friend symbol for friendship {} -> {}", ownerId, friendId);
        return;
    }
    statement->SetData(0, message.FriendSymbol);
    statement->SetData(1, ownerId);
    statement->SetData(2, friendId);
    std::optional<uint64> const changed = CharacterDatabase.DirectExecuteCounted(*statement);
    if (!changed)
    {
        LOG_ERROR("server.social", "Could not set best-friend symbol for friendship {} -> {}", ownerId, friendId);
        return;
    }
    if (*changed == 0)
        return;

    GameMessages::BestFriend reply;
    reply.ListOwnerGid = ownerId;
    reply.BuddyId = friendId;
    reply.Forwarded = 1;
    reply.FriendSymbol = message.FriendSymbol;
    session.SendDmlMessage(reply);
}

void SocialMgr::SendMaximumFriends(GameSession& session, GameMessages::RequestMaxFriends const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    if (message.RequestingPlayerGid != ownerId)
    {
        LOG_WARN("server.social", "Session {} requested max-friends data for wizard {}", session.GetSessionId(), message.RequestingPlayerGid);
        SendChatError(session, message.RequestingPlayerGid);
        return;
    }
    uint32 const maximum = sSettings.Get<uint32>("Social.MaxFriends");
    GameMessages::RequestMaxFriends reply;
    reply.RequestingPlayerGid = ownerId;
    reply.MaximumFriends = static_cast<int32>(std::min<uint32>(maximum, static_cast<uint32>(std::numeric_limits<int32>::max())));
    reply.MaximumSubscriberFriends = reply.MaximumFriends;
    session.SendDmlMessage(reply);
}

void SocialMgr::AddIgnore(GameSession& session, GameMessages::IgnoreAdd const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const ignoredId = message.CharacterGid;
    if (message.ListOwnerGid != ownerId || ignoredId == 0 || ignoredId == ownerId || !LoadLists(ownerId) || !LoadLists(ignoredId))
    {
        LOG_WARN("server.social", "Session {} sent an invalid ignore request for wizard {}", session.GetSessionId(), ignoredId);
        SendChatError(session, ignoredId);
        return;
    }
    CharacterLoad const target = LoadCharacter(ignoredId);
    if (target.Result != CharacterOpResult::Ok || !target.Character || target.Character->IsDeleted())
    {
        SendChatError(session, ignoredId);
        return;
    }

    bool const wasFriend = Lists(ownerId).IsFriend(ignoredId) || Lists(ignoredId).IsFriend(ownerId);
    Statement addIgnore = Prepare(CHAR_INS_SOCIAL_IGNORE);
    Statement ownerFriend = Prepare(CHAR_DEL_SOCIAL_FRIEND);
    Statement targetFriend = Prepare(CHAR_DEL_SOCIAL_FRIEND);
    Statement outgoing = Prepare(CHAR_DEL_SOCIAL_REQUEST);
    Statement incoming = Prepare(CHAR_DEL_SOCIAL_REQUEST);
    if (!addIgnore || !ownerFriend || !targetFriend || !outgoing || !incoming || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not prepare ignore operation {} -> {}", ownerId, ignoredId);
        SendChatError(session, ignoredId);
        return;
    }
    addIgnore->SetData(0, ownerId);
    addIgnore->SetData(1, ignoredId);
    addIgnore->SetData(2, int32{ 0 });
    addIgnore->SetData(3, static_cast<uint64>(NowEpochSeconds()));
    ownerFriend->SetData(0, ownerId);
    ownerFriend->SetData(1, ignoredId);
    targetFriend->SetData(0, ignoredId);
    targetFriend->SetData(1, ownerId);
    outgoing->SetData(0, ownerId);
    outgoing->SetData(1, ignoredId);
    incoming->SetData(0, ignoredId);
    incoming->SetData(1, ownerId);
    std::shared_ptr<Transaction<CharacterDatabaseConnection>> transaction = CharacterDatabase.BeginTransaction();
    transaction->Append(std::move(addIgnore));
    transaction->Append(std::move(ownerFriend));
    transaction->Append(std::move(targetFriend));
    transaction->Append(std::move(outgoing));
    transaction->Append(std::move(incoming));
    if (!CharacterDatabase.DirectCommitTransaction(transaction))
    {
        LOG_ERROR("server.social", "Could not commit ignore operation {} -> {}", ownerId, ignoredId);
        SendChatError(session, ignoredId);
        return;
    }

    Lists(ownerId).AddIgnore(ignoredId);
    Lists(ignoredId).RemoveFriend(ownerId);
    SendIgnoreList(session, true, ignoredId);
    if (wasFriend)
    {
        GameMessages::BuddyDrop ownerDrop;
        ownerDrop.ListOwnerGid = ownerId;
        ownerDrop.EntryGid = ignoredId;
        session.SendDmlMessage(ownerDrop);
        if (auto const remote = _online.find(ignoredId); remote != _online.end())
            if (std::shared_ptr<GameSession> recipient = remote->second.Session.lock())
            {
                GameMessages::BuddyDrop remoteDrop;
                remoteDrop.ListOwnerGid = ignoredId;
                remoteDrop.EntryGid = ownerId;
                recipient->SendDmlMessage(remoteDrop);
            }
    }
}

void SocialMgr::DropIgnore(GameSession& session, GameMessages::IgnoreDrop const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const ignoredId = message.CharacterGid;
    if (message.ListOwnerGid != ownerId || ignoredId == 0 || !LoadLists(ownerId))
    {
        LOG_WARN("server.social", "Session {} sent an invalid ignore removal for wizard {}", session.GetSessionId(), ignoredId);
        return;
    }
    Statement statement = Prepare(CHAR_DEL_SOCIAL_IGNORE);
    if (!statement || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not remove ignored wizard {} from list {}", ignoredId, ownerId);
        return;
    }
    statement->SetData(0, ownerId);
    statement->SetData(1, ignoredId);
    if (!CharacterDatabase.DirectExecute(*statement))
    {
        LOG_ERROR("server.social", "Could not remove ignored wizard {} from list {}", ignoredId, ownerId);
        return;
    }
    Lists(ownerId).RemoveIgnore(ignoredId);
    SendIgnoreList(session);
}

void SocialMgr::SendPresenceToFriends(uint64 characterId, uint8 status, std::string const& zoneName)
{
    uint64 const statusDate = static_cast<uint64>(NowEpochSeconds());
    for (auto const& [viewerId, viewer] : _online)
    {
        if (viewerId == characterId || !IsFriend(viewerId, characterId))
            continue;
        if (std::shared_ptr<GameSession> session = viewer.Session.lock())
        {
            GameMessages::BuddyStatusUpdate update;
            update.ListOwnerGid = viewerId;
            update.EntryGid = characterId;
            update.Status = status;
            update.Permissions = 0;
            update.ZoneName = status == PlayerStatusOffline ? std::string() : zoneName;
            update.RealmName = sSettings.Get<std::string>("Realm.Name");
            update.FriendInfo = 0;
            update.FriendDate = Date32(_lists.at(viewerId).GetFriendDate(characterId));
            update.FriendStatusDate = Date32(statusDate);
            session->SendDmlMessage(update);
        }
    }
}

void SocialMgr::UpdatePresence(std::vector<std::shared_ptr<GameSession>> const& sessions)
{
    std::set<uint64> present;
    for (std::shared_ptr<GameSession> const& session : sessions)
    {
        bool const connected = session->IsOpen() || session->IsLinkDead();
        uint64 const characterId = session->GetCharacterId();
        if (!connected || !session->IsAttached() || characterId == 0)
            continue;
        present.insert(characterId);
        uint8 const status = session->IsLinkDead() ? PlayerStatusLinkDead : PlayerStatusOnline;
        std::string zoneName = session->GetZoneDisplay();
        if (zoneName.empty())
            zoneName = session->GetZonePath();
        auto [position, inserted] = _online.try_emplace(characterId);
        bool const changed = inserted || position->second.Status != status || position->second.ZoneName != zoneName;
        std::shared_ptr<GameSession> const previousSession = position->second.Session.lock();
        position->second.Session = session;
        position->second.Status = status;
        position->second.ZoneName = zoneName;
        if (changed)
            SendPresenceToFriends(characterId, status, zoneName);
    }

    for (auto at = _online.begin(); at != _online.end();)
    {
        if (present.contains(at->first))
        {
            ++at;
            continue;
        }
        SendPresenceToFriends(at->first, PlayerStatusOffline, {});
        at = _online.erase(at);
    }
}
