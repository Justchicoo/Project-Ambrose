/*
 * Project Ambrose by Imjustchico
 * Loads friends, requests and ignores asynchronously, queues their writes away from the world thread, emits the client's buddy messages from its installed definitions, keeps live presence for this realm and filters radial chat through the owner's ignore list.
 */

#include "SocialMgr.h"

#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "DatabaseEnv.h"
#include "GameSession.h"
#include "Log.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "PackedName.h"
#include "PropertyFiller.h"
#include "QueryResult.h"
#include "Settings.h"
#include "TypeRegistry.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <map>
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

    struct IgnoreRow
    {
        uint64 CharacterId = 0;
        int32 PlatformType = 0;
        std::string PackedName;
    };

    struct RequestRow
    {
        uint64 CharacterId = 0;
        std::string PackedName;
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

    std::string PackedCharacterName(Field const& customName, Field const& nameIndices, Field const& gender)
    {
        std::optional<std::string> custom;
        if (!customName.IsNull())
            custom = customName.Get<std::string>();
        return PackedName::ForWizard(custom, nameIndices.Get<uint32>(), gender.Get<uint32>());
    }

    Statement Prepare(CharacterDatabaseStatements id)
    {
        return CharacterDatabase.GetPreparedStatement(id);
    }

    std::map<uint64, SocialFriend> ReadFriends(PreparedQueryResult result)
    {
        std::map<uint64, SocialFriend> friends;
        if (!result || result->GetRowCount() == 0)
            return friends;
        PreparedResultSet const& row = *result;
        do
        {
            uint64 const characterId = row[0].Get<uint64>();
            friends.emplace(characterId, SocialFriend{
                row[1].Get<uint8>(),
                row[2].Get<uint64>(),
                PackedCharacterName(row[3], row[4], row[5])
            });
        } while (result->NextRow());
        return friends;
    }

    std::map<uint64, SocialIgnore> ReadIgnores(PreparedQueryResult result)
    {
        std::map<uint64, SocialIgnore> ignores;
        if (!result || result->GetRowCount() == 0)
            return ignores;
        PreparedResultSet const& row = *result;
        do
        {
            uint64 const characterId = row[0].Get<uint64>();
            ignores.emplace(characterId, SocialIgnore{
                row[1].Get<int32>(),
                PackedCharacterName(row[2], row[3], row[4])
            });
        } while (result->NextRow());
        return ignores;
    }

    std::vector<RequestRow> ReadRequests(PreparedQueryResult result)
    {
        std::vector<RequestRow> requests;
        if (!result || result->GetRowCount() == 0)
            return requests;
        PreparedResultSet const& row = *result;
        do
        {
            requests.push_back({ row[0].Get<uint64>(), PackedCharacterName(row[1], row[2], row[3]), row[4].Get<int32>() });
        } while (result->NextRow());
        return requests;
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
                .Set("m_ignoreName", row.PackedName)
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

}

bool SocialLists::AddFriend(uint64 characterId, uint64 friendDate, uint8 bestFriendSymbol, std::string packedName)
{
    if (characterId == 0)
        return false;
    bool const inserted = !_friends.contains(characterId);
    _friends[characterId] = { bestFriendSymbol, friendDate, std::move(packedName) };
    return inserted;
}

bool SocialLists::RemoveFriend(uint64 characterId)
{
    return _friends.erase(characterId) != 0;
}

bool SocialLists::AddIgnore(uint64 characterId, int32 platformType, std::string packedName)
{
    if (characterId == 0)
        return false;
    _friends.erase(characterId);
    bool const inserted = !_ignores.contains(characterId);
    _ignores[characterId] = { platformType, std::move(packedName) };
    return inserted;
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
    auto const found = _friends.find(characterId);
    return found == _friends.end() ? 0 : found->second.Date;
}

uint8 SocialLists::GetBestFriendSymbol(uint64 characterId) const noexcept
{
    auto const found = _friends.find(characterId);
    return found == _friends.end() ? 0 : found->second.BestFriendSymbol;
}

void SocialLists::SetBestFriendSymbol(uint64 characterId, uint8 symbol) noexcept
{
    if (auto const found = _friends.find(characterId); found != _friends.end())
        found->second.BestFriendSymbol = symbol;
}

void SocialLists::Replace(std::map<uint64, SocialFriend> friends, std::map<uint64, SocialIgnore> ignores)
{
    for (auto const& ignore : ignores)
        friends.erase(ignore.first);
    _friends = std::move(friends);
    _ignores = std::move(ignores);
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

void SocialMgr::QueueQuery(GameSession& session, QueryCallback&& query)
{
    session._queryCallbacks.AddCallback(std::move(query));
    session._queryCallbacks.ProcessReadyCallbacks();
}

void SocialMgr::QueueWorldWork(GameSession& session, std::function<void(GameSession&)> work)
{
    std::shared_ptr<GameSession> owner = session.SharedSelf();
    if (!owner->IsOpen())
        return;
    if (!owner->QueueInbound([owner, work = std::move(work)]() mutable
    {
        if (owner->IsOpen())
            work(*owner);
    }))
    {
        if (owner->IsOpen())
            LOG_ERROR("server.social", "Could not queue database work for session {}", owner->GetSessionId());
    }
}

void SocialMgr::LoadLists(GameSession& session, uint64 characterId, std::function<void(bool)> completion)
{
    if (_loaded.contains(characterId))
    {
        completion(true);
        return;
    }
    Statement friends = Prepare(CHAR_SEL_SOCIAL_FRIENDS);
    if (!friends || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not prepare loading the friend list for wizard {}", characterId);
        completion(false);
        return;
    }
    friends->SetData(0, characterId);
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(friends), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, characterId, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        std::map<uint64, SocialFriend> friends = ReadFriends(std::move(result));
        QueueWorldWork(*owner, [this, characterId, friends = std::move(friends), completion = std::move(completion)](GameSession& session) mutable
        {
            Statement ignores = Prepare(CHAR_SEL_SOCIAL_IGNORES);
            if (!ignores || !CharacterDatabase.IsOpen())
            {
                LOG_ERROR("server.social", "Could not prepare loading the ignore list for wizard {}", characterId);
                completion(false);
                return;
            }
            ignores->SetData(0, characterId);
            GameSession* const owner = &session;
            QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(ignores), session.MakeCompletionHandler())
                .WithPreparedCallback([this, owner, characterId, friends = std::move(friends), completion = std::move(completion)](PreparedQueryResult ignoreResult) mutable
            {
                std::map<uint64, SocialIgnore> ignores = ReadIgnores(std::move(ignoreResult));
                QueueWorldWork(*owner, [this, characterId, friends = std::move(friends), ignores = std::move(ignores),
                    completion = std::move(completion)](GameSession&) mutable
                {
                    if (!_loaded.contains(characterId))
                    {
                        Lists(characterId).Replace(std::move(friends), std::move(ignores));
                        _loaded.insert(characterId);
                    }
                    completion(true);
                });
            }));
        });
    }));
}

void SocialMgr::LoadCharacter(GameSession& session, uint64 characterId, std::function<void(std::optional<CharacterSummary>)> completion)
{
    CharacterRepository::Statement statement = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareLoad(characterId) : nullptr;
    if (!statement)
    {
        LOG_ERROR("server.social", "Could not prepare loading wizard {}", characterId);
        completion(std::nullopt);
        return;
    }
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(statement), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, characterId, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        std::vector<CharacterSummary> characters = result ? CharacterRepository::ReadCharacters(*result) : std::vector<CharacterSummary>();
        std::optional<CharacterSummary> character;
        if (!characters.empty())
            character = std::move(characters.front());
        QueueWorldWork(*owner, [completion = std::move(completion), character = std::move(character)](GameSession&) mutable
        {
            completion(std::move(character));
        });
    }));
}

void SocialMgr::CheckRequestExists(GameSession& session, uint64 requesterId, uint64 targetId, std::function<void(bool)> completion)
{
    Statement statement = Prepare(CHAR_SEL_SOCIAL_REQUEST_EXISTS);
    if (!statement || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not prepare checking friend request {} -> {}", requesterId, targetId);
        completion(false);
        return;
    }
    statement->SetData(0, requesterId);
    statement->SetData(1, targetId);
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(statement), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, requesterId, targetId, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        bool const exists = result && result->GetRowCount() != 0;
        QueueWorldWork(*owner, [completion = std::move(completion), exists](GameSession&) mutable
        {
            completion(exists);
        });
    }));
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
    _pendingRequests.clear();
}

void SocialMgr::SendChatError(GameSession& session, uint64 characterId)
{
    GameMessages::ChatError error;
    error.ListOwnerGid = session.GetCharacterId();
    error.CharacterId = characterId;
    error.Error = ChatErrorGeneric;
    if (!session.SendDmlMessage(error))
        LOG_ERROR("server.social", "Could not send CHATERROR to wizard {}", session.GetCharacterId());
}

void SocialMgr::SendFriendEntry(GameSession& session, uint64 friendId, std::string const& friendPackedName, uint64 friendDate, uint64 friendStatusDate)
{
    GameMessages::BuddyEntry entry;
    entry.ListOwnerGid = session.GetCharacterId();
    entry.EntryGid = friendId;
    entry.Name = friendPackedName;
    entry.FriendInfo = 0;
    entry.Permissions = sSettings.Get<uint32>("LoginComplete.Permissions");
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
    if (!_loaded.contains(session.GetCharacterId()))
    {
        LOG_ERROR("server.social", "Cannot send the ignore list before it is loaded for wizard {}", session.GetCharacterId());
        return;
    }
    for (auto const& [ignoredId, ignore] : Lists(session.GetCharacterId()).GetIgnores())
        rows.push_back({ ignoredId, ignore.PlatformType, ignore.PackedName });
    std::optional<std::string> data = EncodeIgnoreList(rows, addOne ? std::optional<uint64>(characterId) : std::nullopt);
    if (!data)
        return;
    GameMessages::IgnoreList reply;
    reply.ListOwnerGid = session.GetCharacterId();
    reply.ListData = std::move(*data);
    reply.Add = addOne ? 1 : 0;
    session.SendDmlMessage(reply);
}

void SocialMgr::SendPendingRequests(GameSession& session, std::function<void()> completion)
{
    uint64 const ownerId = session.GetCharacterId();
    Statement statement = Prepare(CHAR_SEL_SOCIAL_REQUESTS);
    if (!statement || !CharacterDatabase.IsOpen())
    {
        LOG_ERROR("server.social", "Could not prepare incoming friend requests for wizard {}", ownerId);
        completion();
        return;
    }
    statement->SetData(0, ownerId);
    GameSession* const owner = &session;
    QueueQuery(session, CharacterDatabase.AsyncQuery(std::move(statement), session.MakeCompletionHandler())
        .WithPreparedCallback([this, owner, ownerId, completion = std::move(completion)](PreparedQueryResult result) mutable
    {
        std::vector<RequestRow> requests = ReadRequests(std::move(result));
        QueueWorldWork(*owner, [this, ownerId, requests = std::move(requests), completion = std::move(completion)](GameSession& session) mutable
        {
            for (RequestRow const& request : requests)
            {
                _pendingRequests.emplace(request.CharacterId, ownerId);
                GameMessages::BuddyRequestAdd message;
                message.ListOwnerGid = request.CharacterId;
                message.EntryGid = ownerId;
                message.OwnerName = request.PackedName;
                message.OwnerLevel = static_cast<uint8>(std::clamp(request.Level, 0, static_cast<int32>(std::numeric_limits<uint8>::max())));
                session.SendDmlMessage(message);
            }
            completion();
        });
    }));
}

void SocialMgr::SendLists(GameSession& session)
{
    uint64 const ownerId = session.GetCharacterId();
    if (ownerId == 0)
        return;
    LoadLists(session, ownerId, [this, owner = &session, ownerId](bool loaded)
    {
        if (!loaded || !owner->IsOpen())
            return;
        for (auto const& [friendId, friendInfo] : Lists(ownerId).GetFriends())
        {
            SendFriendEntry(*owner, friendId, friendInfo.PackedName, friendInfo.Date, friendInfo.Date);
            if (friendInfo.BestFriendSymbol != 0)
            {
                GameMessages::BestFriend bestFriend;
                bestFriend.ListOwnerGid = ownerId;
                bestFriend.BuddyId = friendId;
                bestFriend.Forwarded = 1;
                bestFriend.FriendSymbol = friendInfo.BestFriendSymbol;
                owner->SendDmlMessage(bestFriend);
            }
        }
        SendIgnoreList(*owner);
        SendPendingRequests(*owner, [owner, ownerId]
        {
            if (!owner->IsOpen())
                return;
            GameMessages::BuddyListComplete complete;
            complete.ListOwnerGid = ownerId;
            owner->SendDmlMessage(complete);
        });
    });
}

void SocialMgr::AddFriendRequest(GameSession& session, GameMessages::BuddyRequestAdd const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const targetId = message.EntryGid;
    if (!IsRequestOwnerForCharacter(message.ListOwnerGid, ownerId) || targetId == 0 || targetId == ownerId)
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
        _pendingRequests.erase({ ownerId, targetId });
        CharacterDatabase.Execute(std::move(statement));
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

    GameSession* const requester = &session;
    LoadLists(session, ownerId, [this, requester, ownerId, targetId](bool ownerLoaded)
    {
        if (!ownerLoaded || !requester->IsOpen())
        {
            SendChatError(*requester, targetId);
            return;
        }
        LoadLists(*requester, targetId, [this, requester, ownerId, targetId](bool targetLoaded)
        {
            if (!targetLoaded || !requester->IsOpen() || Lists(ownerId).IsFriend(targetId) ||
                Lists(targetId).IsFriend(ownerId) || Lists(targetId).IsIgnored(ownerId))
            {
                SendChatError(*requester, targetId);
                return;
            }
            LoadCharacter(*requester, targetId, [this, requester, ownerId, targetId](std::optional<CharacterSummary> target) mutable
            {
                if (!target || target->IsDeleted())
                {
                    LOG_DEBUG("server.social", "Session {} requested friendship with a missing, deleted or unavailable wizard", requester->GetSessionId());
                    SendChatError(*requester, targetId);
                    return;
                }
                LoadCharacter(*requester, ownerId, [this, requester, ownerId, targetId](std::optional<CharacterSummary> owner) mutable
                {
                    if (!owner || owner->IsDeleted())
                    {
                        LOG_DEBUG("server.social", "Session {} requested friendship with a missing, deleted or unavailable wizard", requester->GetSessionId());
                        SendChatError(*requester, targetId);
                        return;
                    }
                    if (!CanRequestFriend(static_cast<uint32>(Lists(ownerId).FriendCount()), sSettings.Get<uint32>("Social.MaxFriends")))
                    {
                        SendChatError(*requester, targetId);
                        return;
                    }

                    Statement statement = Prepare(CHAR_INS_SOCIAL_REQUEST);
                    if (!statement || !CharacterDatabase.IsOpen())
                    {
                        LOG_ERROR("server.social", "Could not prepare friend request {} -> {}", ownerId, targetId);
                        SendChatError(*requester, targetId);
                        return;
                    }
                    statement->SetData(0, ownerId);
                    statement->SetData(1, targetId);
                    statement->SetData(2, static_cast<uint64>(NowEpochSeconds()));
                    _pendingRequests.emplace(ownerId, targetId);
                    CharacterDatabase.Execute(std::move(statement));
                    if (auto const found = _online.find(targetId); found != _online.end())
                        if (std::shared_ptr<GameSession> recipient = found->second.Session.lock())
                        {
                            GameMessages::BuddyRequestAdd request;
                            request.ListOwnerGid = ownerId;
                            request.EntryGid = targetId;
                            request.OwnerName = PackedName::ForWizard(owner->CustomName, owner->NameIndices, owner->Appearance.Gender);
                            request.OwnerLevel = static_cast<uint8>(std::clamp(owner->Level, 0, static_cast<int32>(std::numeric_limits<uint8>::max())));
                            recipient->SendDmlMessage(request);
                        }
                });
            });
        });
    });
}

void SocialMgr::AcceptFriendRequest(GameSession& session, GameMessages::BuddyRequestAccept const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const requesterId = message.ListOwnerGid;
    if (!IsIncomingRequestForCharacter(requesterId, message.EntryGid, ownerId))
    {
        LOG_WARN("server.social", "Session {} sent an invalid friend acceptance from wizard {} for {}", session.GetSessionId(), message.ListOwnerGid, requesterId);
        SendChatError(session, requesterId);
        return;
    }
    GameSession* const owner = &session;
    LoadLists(session, ownerId, [this, owner, ownerId, requesterId](bool ownerLoaded)
    {
        if (!ownerLoaded || !owner->IsOpen())
        {
            SendChatError(*owner, requesterId);
            return;
        }
        LoadLists(*owner, requesterId, [this, owner, ownerId, requesterId](bool requesterLoaded)
        {
            if (!requesterLoaded || !owner->IsOpen())
            {
                SendChatError(*owner, requesterId);
                return;
            }
            if (_pendingRequests.contains({ requesterId, ownerId }))
            {
                FinishAcceptFriendRequest(*owner, ownerId, requesterId);
                return;
            }
            CheckRequestExists(*owner, requesterId, ownerId, [this, owner, ownerId, requesterId](bool exists)
            {
                if (!exists)
                {
                    SendChatError(*owner, requesterId);
                    return;
                }
                _pendingRequests.emplace(requesterId, ownerId);
                FinishAcceptFriendRequest(*owner, ownerId, requesterId);
            });
        });
    });
}

void SocialMgr::FinishAcceptFriendRequest(GameSession& session, uint64 ownerId, uint64 requesterId)
{
    uint32 const maximum = sSettings.Get<uint32>("Social.MaxFriends");
    if (!CanAcceptFriendRequest(_pendingRequests.contains({ requesterId, ownerId })) ||
        !Lists(ownerId).CanAddFriend(maximum) || !Lists(requesterId).CanAddFriend(maximum))
    {
        SendChatError(session, requesterId);
        return;
    }

    GameSession* const owner = &session;
    LoadCharacter(session, requesterId, [this, owner, ownerId, requesterId](std::optional<CharacterSummary> requester)
    {
        if (!requester || requester->IsDeleted())
        {
            LOG_ERROR("server.social", "Could not load the requester's name while accepting {} -> {}", requesterId, ownerId);
            SendChatError(*owner, requesterId);
            return;
        }
        uint64 const now = static_cast<uint64>(NowEpochSeconds());
        std::string const requesterName = PackedName::ForWizard(requester->CustomName, requester->NameIndices, requester->Appearance.Gender);
        Statement ownerFriend = Prepare(CHAR_INS_SOCIAL_FRIEND);
        Statement requesterFriend = Prepare(CHAR_INS_SOCIAL_FRIEND);
        Statement deleteIncoming = Prepare(CHAR_DEL_SOCIAL_REQUEST);
        Statement deleteOutgoing = Prepare(CHAR_DEL_SOCIAL_REQUEST);
        if (!ownerFriend || !requesterFriend || !deleteIncoming || !deleteOutgoing || !CharacterDatabase.IsOpen())
        {
            LOG_ERROR("server.social", "Could not prepare the transaction accepting friend request {} -> {}", requesterId, ownerId);
            SendChatError(*owner, requesterId);
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
        Lists(ownerId).AddFriend(requesterId, now, 0, requesterName);
        Lists(requesterId).AddFriend(ownerId, now, 0, owner->GetChatName());
        _pendingRequests.erase({ requesterId, ownerId });
        _pendingRequests.erase({ ownerId, requesterId });
        CharacterDatabase.CommitTransaction(std::move(transaction));

        SendFriendEntry(*owner, requesterId, requesterName, now, now);
        if (auto const remote = _online.find(requesterId); remote != _online.end())
            if (std::shared_ptr<GameSession> recipient = remote->second.Session.lock())
                SendFriendEntry(*recipient, ownerId, owner->GetChatName(), now, now);
        SendPresenceToFriends(ownerId, PlayerStatusOnline, owner->GetZoneDisplay());
        if (auto const remote = _online.find(requesterId); remote != _online.end())
            SendPresenceToFriends(requesterId, remote->second.Status, remote->second.ZoneName);
    });
}

void SocialMgr::DenyFriendRequest(GameSession& session, GameMessages::BuddyRequestDeny const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const requesterId = message.ListOwnerGid;
    if (!IsIncomingRequestForCharacter(requesterId, message.EntryGid, ownerId))
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
    _pendingRequests.erase({ requesterId, ownerId });
    CharacterDatabase.Execute(std::move(statement));
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
    if (message.ListOwnerGid != ownerId || friendId == 0 || friendId == ownerId)
    {
        LOG_WARN("server.social", "Session {} sent an invalid friend removal for wizard {}", session.GetSessionId(), friendId);
        return;
    }
    LoadLists(session, ownerId, [this, owner = &session, ownerId, friendId](bool ownerLoaded)
    {
        if (!ownerLoaded || !owner->IsOpen())
            return;
        LoadLists(*owner, friendId, [this, owner, ownerId, friendId](bool friendLoaded)
        {
            if (!friendLoaded || (!Lists(ownerId).IsFriend(friendId) && !Lists(friendId).IsFriend(ownerId)))
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
            Lists(ownerId).RemoveFriend(friendId);
            Lists(friendId).RemoveFriend(ownerId);
            CharacterDatabase.CommitTransaction(std::move(transaction));

            GameMessages::BuddyDrop ownerDrop;
            ownerDrop.ListOwnerGid = ownerId;
            ownerDrop.EntryGid = friendId;
            owner->SendDmlMessage(ownerDrop);
            if (auto const remote = _online.find(friendId); remote != _online.end())
                if (std::shared_ptr<GameSession> recipient = remote->second.Session.lock())
                {
                    GameMessages::BuddyDrop remoteDrop;
                    remoteDrop.ListOwnerGid = friendId;
                    remoteDrop.EntryGid = ownerId;
                    recipient->SendDmlMessage(remoteDrop);
                }
        });
    });
}

void SocialMgr::SetBestFriend(GameSession& session, GameMessages::BestFriend const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const friendId = message.BuddyId;
    if (message.ListOwnerGid != ownerId || friendId == 0 || friendId == ownerId)
    {
        LOG_WARN("server.social", "Session {} tried to set a best-friend symbol for a wizard outside its friend list", session.GetSessionId());
        return;
    }
    LoadLists(session, ownerId, [this, owner = &session, ownerId, friendId, message](bool loaded)
    {
        if (!loaded || !owner->IsOpen() || !Lists(ownerId).IsFriend(friendId))
            return;
        Statement statement = Prepare(CHAR_UPD_SOCIAL_BEST_FRIEND);
        if (!statement || !CharacterDatabase.IsOpen())
        {
            LOG_ERROR("server.social", "Could not prepare best-friend symbol for friendship {} -> {}", ownerId, friendId);
            return;
        }
        statement->SetData(0, message.FriendSymbol);
        statement->SetData(1, ownerId);
        statement->SetData(2, friendId);
        Lists(ownerId).SetBestFriendSymbol(friendId, message.FriendSymbol);
        CharacterDatabase.Execute(std::move(statement));

        GameMessages::BestFriend reply;
        reply.ListOwnerGid = ownerId;
        reply.BuddyId = friendId;
        reply.Forwarded = 1;
        reply.FriendSymbol = message.FriendSymbol;
        owner->SendDmlMessage(reply);
    });
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
    if (!IsRequestOwnerForCharacter(message.ListOwnerGid, ownerId) || ignoredId == 0 || ignoredId == ownerId)
    {
        LOG_WARN("server.social", "Session {} sent an invalid ignore request for wizard {}", session.GetSessionId(), ignoredId);
        SendChatError(session, ignoredId);
        return;
    }
    GameSession* const owner = &session;
    LoadLists(session, ownerId, [this, owner, ownerId, ignoredId](bool ownerLoaded)
    {
        if (!ownerLoaded || !owner->IsOpen())
        {
            SendChatError(*owner, ignoredId);
            return;
        }
        LoadLists(*owner, ignoredId, [this, owner, ownerId, ignoredId](bool targetLoaded)
        {
            if (!targetLoaded || !owner->IsOpen())
            {
                SendChatError(*owner, ignoredId);
                return;
            }
            LoadCharacter(*owner, ignoredId, [this, owner, ownerId, ignoredId](std::optional<CharacterSummary> target)
            {
                if (!target || target->IsDeleted())
                {
                    SendChatError(*owner, ignoredId);
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
                    SendChatError(*owner, ignoredId);
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

                std::string const ignoredName = PackedName::ForWizard(target->CustomName, target->NameIndices, target->Appearance.Gender);
                Lists(ownerId).AddIgnore(ignoredId, 0, ignoredName);
                Lists(ownerId).RemoveFriend(ignoredId);
                Lists(ignoredId).RemoveFriend(ownerId);
                _pendingRequests.erase({ ownerId, ignoredId });
                _pendingRequests.erase({ ignoredId, ownerId });
                CharacterDatabase.CommitTransaction(std::move(transaction));
                SendIgnoreList(*owner, true, ignoredId);
                if (wasFriend)
                {
                    GameMessages::BuddyDrop ownerDrop;
                    ownerDrop.ListOwnerGid = ownerId;
                    ownerDrop.EntryGid = ignoredId;
                    owner->SendDmlMessage(ownerDrop);
                    if (auto const remote = _online.find(ignoredId); remote != _online.end())
                        if (std::shared_ptr<GameSession> recipient = remote->second.Session.lock())
                        {
                            GameMessages::BuddyDrop remoteDrop;
                            remoteDrop.ListOwnerGid = ignoredId;
                            remoteDrop.EntryGid = ownerId;
                            recipient->SendDmlMessage(remoteDrop);
                        }
                }
            });
        });
    });
}

void SocialMgr::DropIgnore(GameSession& session, GameMessages::IgnoreDrop const& message)
{
    uint64 const ownerId = session.GetCharacterId();
    uint64 const ignoredId = message.CharacterGid;
    if (!IsRequestOwnerForCharacter(message.ListOwnerGid, ownerId) || ignoredId == 0)
    {
        LOG_WARN("server.social", "Session {} sent an invalid ignore removal for wizard {}", session.GetSessionId(), ignoredId);
        return;
    }
    LoadLists(session, ownerId, [this, owner = &session, ownerId, ignoredId](bool loaded)
    {
        if (!loaded || !owner->IsOpen())
            return;
        Statement statement = Prepare(CHAR_DEL_SOCIAL_IGNORE);
        if (!statement || !CharacterDatabase.IsOpen())
        {
            LOG_ERROR("server.social", "Could not prepare removal of ignored wizard {} from list {}", ignoredId, ownerId);
            return;
        }
        statement->SetData(0, ownerId);
        statement->SetData(1, ignoredId);
        Lists(ownerId).RemoveIgnore(ignoredId);
        CharacterDatabase.Execute(std::move(statement));
        SendIgnoreList(*owner);
    });
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
            update.Permissions = sSettings.Get<uint32>("LoginComplete.Permissions");
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
