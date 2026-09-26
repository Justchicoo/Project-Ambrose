/*
 * Project Ambrose by Imjustchico
 * Draining is what this adds to a session, and dispatching what it can answer: the world calls DrainQueue on its own thread and the queued work runs there, bounded so one talkative client cannot hold the tick, and every handler is written knowing it runs on that thread and nowhere else. MSG_ATTACH is taken as soon as a client connects, because a client that has not attached has nothing else to say, and the key it carries is spent before the client is let in: the spend is one conditional update, so two clients holding the same key cannot both win it, and a spend that changed no row is read back only to say why, because the reason a client was turned away is worth knowing while the reason it was let in is not. A refused attach is told once and the socket closed behind it, and a session that was let in gives its wizard back when it goes. Entering the world is refused with MSG_ATTACHFAILED, the reason logged, when the wizard is missing, deleted or another account's, when its stats cannot be read or its school has no level table, when its zone is one this server cannot load, when the instance has no mobile id left, or when its object cannot be built; the stats are read through the wizard's own row, so a failed read is never mistaken for a wizard with no stats yet, whose save would then write over what it has; the object is encoded with the transmit mask the owner's own object is read with, and CriticalObjects is sent empty, which the client reads as no critical objects rather than a list to deserialize.
 */

#include "GameSession.h"
#include "AccountMgr.h"
#include "CharacterNameMgr.h"
#include "CharacterRepository.h"
#include "CoreObjectSerializer.h"
#include "Frame.h"
#include "GameMessageTable.h"
#include "Log.h"
#include "MapMgr.h"
#include "MessageRegistry.h"
#include "ObjectFields.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "PlayerLevelMgr.h"
#include "PlayerObjectBuilder.h"
#include "Settings.h"
#include "StringHash.h"
#include "StringUtil.h"
#include "ZoneMgr.h"

#include <fmt/format.h>

#include <chrono>
#include <utility>
#include <vector>

namespace
{

    std::atomic<uint32> RealmId{ 0 };

    int64 NowEpochSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

GameSession::GameSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context)
    : SessionBase(std::move(socket), limits, std::move(context))
{
}

void GameSession::SetRealmId(uint32 realmId) noexcept
{
    RealmId.store(realmId, std::memory_order_relaxed);
}

uint32 GameSession::GetRealmId() noexcept
{
    return RealmId.load(std::memory_order_relaxed);
}

std::shared_ptr<GameSession> GameSession::SharedSelf()
{
    return std::static_pointer_cast<GameSession>(shared_from_this());
}

std::size_t GameSession::DrainQueue(std::size_t limit)
{
    return ProcessQueuedMessages(limit);
}

void GameSession::WorldUpdate(std::chrono::steady_clock::time_point now)
{
    if (!IsOpen() || IsAttached() || _attaching.load(std::memory_order_relaxed))
        return;
    std::chrono::milliseconds const timeout = GetContext().GetSettings().AttachTimeout;
    if (now - _connectedAt < timeout)
        return;
    LOG_INFO("server.gamesession", "Session {} from {} sent no MSG_ATTACH within Attach.Timeout of {} s; closing it",
        GetSessionId(), GetRemoteAddress().to_string(), std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
    CloseSocket();
}

void GameSession::ProcessCallbacks()
{
    _countedCallbacks.ProcessReadyCallbacks();
    _queryCallbacks.ProcessReadyCallbacks();
}

SQLOperation::CompletionHandler GameSession::MakeCompletionHandler()
{
    return [weak = std::weak_ptr<GameSession>(SharedSelf()), executor = GetExecutor()]
    {
        asio::post(executor, [weak]
        {
            if (std::shared_ptr<GameSession> const session = weak.lock())
                session->ProcessCallbacks();
        });
    };
}

void GameSession::OnMessage(DmlMessageData& message)
{
    DispatchResult const result = GameMessageTable::Get().Dispatch(*this, sMessageRegistry.GetCatalog(), message);
    if (result != DispatchResult::NotHandled && result != DispatchResult::UnknownMessage)
        return;
    _unhandled.fetch_add(1, std::memory_order_relaxed);
    if (result == DispatchResult::UnknownMessage)
        LOG_DEBUG("server.gamesession", "Session {} sent service {} order {}, which no loaded message definition names",
            GetSessionId(), message.ServiceId, message.Order);
}

void GameSession::OnSessionClosed()
{
    if (_attached.exchange(false, std::memory_order_relaxed))
    {
        LoginKeyClaim claim;
        claim.AccountId = GetAccountId();
        claim.CharacterId = GetCharacterId();
        claim.RealmId = GetRealmId();
        LoginKeyValidator::MarkOffline(claim);
    }
    _countedCallbacks.Clear();
    _queryCallbacks.Clear();
    SessionBase::OnSessionClosed();
}

void GameSession::HandleAttach(GameMessages::Attach& message)
{
    LoginKeyClaim claim;
    claim.Key = message.LoginKey;
    claim.AccountId = message.UserId;
    claim.CharacterId = message.CharId;
    claim.RealmId = GetRealmId();

    LOG_INFO("server.gamesession", "Session {} from {} is attaching as account {} with wizard {} for zone {} at {}, on a key of {} character(s)",
        GetSessionId(), GetRemoteAddress().to_string(), message.UserId, message.CharId, Ambrose::ForLog(message.ZoneName, 128),
        Ambrose::ForLog(message.Location, 64), message.LoginKey.size());

    if (_attaching.exchange(true, std::memory_order_relaxed))
    {
        RefuseAttach(claim, LoginKeyVerdict::AlreadyUsed);
        return;
    }

    int64 const now = NowEpochSeconds();
    std::optional<CountedCallback> consume = LoginKeyValidator::BeginConsume(claim, now, MakeCompletionHandler());
    if (!consume)
    {
        RefuseAttach(claim, LoginKeyVerdict::Unavailable);
        return;
    }

    _countedCallbacks.AddCallback(std::move(*consume).AfterComplete([this, claim, now](std::optional<uint64> affected)
    {
        if (!IsOpen() || IsKicked())
            return;
        if (!affected)
        {
            RefuseAttach(claim, LoginKeyVerdict::Unavailable);
            return;
        }
        if (*affected == 1)
        {
            AcceptAttach(claim);
            return;
        }
        Diagnose(claim, now);
    }));
}

void GameSession::Diagnose(LoginKeyClaim claim, int64 now)
{
    std::optional<QueryCallback> diagnose = LoginKeyValidator::BeginDiagnose(claim.Key, MakeCompletionHandler());
    if (!diagnose)
    {
        RefuseAttach(claim, LoginKeyVerdict::Unavailable);
        return;
    }
    _queryCallbacks.AddCallback(std::move(*diagnose).WithPreparedCallback([this, claim, now](PreparedQueryResult result)
    {
        if (!IsOpen() || IsKicked())
            return;
        RefuseAttach(claim, LoginKeyValidator::Classify(LoginKeyValidator::ReadRecord(result), claim, now));
    }));
}

void GameSession::AcceptAttach(LoginKeyClaim const& claim)
{
    SetAccountId(claim.AccountId);
    SetCharacterId(claim.CharacterId);
    _attached.store(true, std::memory_order_relaxed);
    SetStatus(SessionStatus::Authenticated);
    LoginKeyValidator::MarkOnline(claim);
    LOG_INFO("server.gamesession", "Session {} attached: account {} with wizard {} on realm {}, its key accepted and spent",
        GetSessionId(), claim.AccountId, claim.CharacterId, claim.RealmId);
    LoadAccount(claim);
}

void GameSession::LoadAccount(LoginKeyClaim const& claim)
{
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.IsOpen() ? AccountMgr::PrepareGetAccountById(claim.AccountId) : nullptr;
    if (!statement)
    {
        RefuseEntry(claim, "the login database is not open");
        return;
    }
    _queryCallbacks.AddCallback(LoginDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler()).WithPreparedCallback([this, claim](PreparedQueryResult result)
    {
        if (!IsOpen() || IsKicked())
            return;
        if (!result)
        {
            RefuseEntry(claim, fmt::format("account {} is not in the login database", claim.AccountId));
            return;
        }
        AccountInfo const account = AccountMgr::ReadAccountRow(*result);
        _securityLevel.store(account.SecurityLevel, std::memory_order_relaxed);
        LoadCharacter(claim);
    }));
}

void GameSession::LoadCharacter(LoginKeyClaim const& claim)
{
    CharacterRepository::Statement statement = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareLoad(claim.CharacterId) : nullptr;
    if (!statement)
    {
        RefuseEntry(claim, "the characters database is not open");
        return;
    }
    _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler()).WithPreparedCallback([this, claim](PreparedQueryResult result)
    {
        if (!IsOpen() || IsKicked())
            return;
        std::vector<CharacterSummary> found = result ? CharacterRepository::ReadCharacters(*result) : std::vector<CharacterSummary>();
        if (found.empty())
        {
            RefuseEntry(claim, fmt::format("wizard {} is not in the characters database", claim.CharacterId));
            return;
        }
        CharacterSummary character = std::move(found.front());
        if (character.Account != claim.AccountId || character.IsDeleted())
        {
            RefuseEntry(claim, fmt::format("wizard {} is {}", claim.CharacterId, character.IsDeleted() ? "deleted" : fmt::format("account {}'s, not {}'s", character.Account, claim.AccountId)));
            return;
        }
        LoadStats(claim, std::move(character));
    }));
}

void GameSession::LoadStats(LoginKeyClaim const& claim, CharacterSummary character)
{
    CharacterRepository::Statement statement = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareLoadStats(character.Guid) : nullptr;
    if (!statement)
    {
        RefuseEntry(claim, "the characters database is not open");
        return;
    }
    _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler()).WithPreparedCallback([this, claim, character = std::move(character)](PreparedQueryResult result)
    {
        if (!IsOpen() || IsKicked())
            return;
        if (!result)
        {
            RefuseEntry(claim, fmt::format("wizard {}'s stats cannot be read from the characters database", character.Guid));
            return;
        }
        std::optional<CharacterStats> stored = CharacterRepository::ReadStats(*result);
        std::shared_ptr<GameSession> const self = SharedSelf();
        if (!QueueInbound([self, claim, character, stored = std::move(stored)] { self->EnterWorld(claim, character, stored); }))
            RefuseEntry(claim, "its queue of work is full");
    }));
}

void GameSession::EnterWorld(LoginKeyClaim const& claim, CharacterSummary const& character, std::optional<CharacterStats> const& stored)
{
    std::string problem;
    std::optional<PlayerStats> stats = PlayerStats::Create(character, stored, *sPlayerLevelMgr.GetLevels(), *sPlayerLevelMgr.GetStats(), problem);
    if (!stats)
    {
        RefuseEntry(claim, problem);
        return;
    }

    bool const placed = character.PositionX != 0.0f || character.PositionY != 0.0f || character.PositionZ != 0.0f;
    ZonePlace const start = sZoneMgr.FindPlace(character.Zone, ZoneLocations::StartName);
    if (!start.Found())
    {
        RefuseEntry(claim, fmt::format("wizard {} is in {}, and {}", character.Guid, Ambrose::ForLog(character.Zone, 128), ZoneMgr::GetLookupName(start.Result)));
        return;
    }
    PlayerPlacement placement;
    placement.X = placed ? character.PositionX : start.Location.X;
    placement.Y = placed ? character.PositionY : start.Location.Y;
    placement.Z = placed ? character.PositionZ : start.Location.Z;
    placement.Yaw = placed ? character.Orientation : start.Location.Yaw.value_or(0.0f);

    Map& map = sMapMgr.FindOrCreatePublic(character.Zone);
    std::optional<uint16> const mobileId = sMapMgr.AddPlayer(map, character.Guid);
    if (!mobileId)
    {
        RefuseEntry(claim, fmt::format("instance {} of {} has no mobile id left", map.GetDynamicZoneId(), character.Zone));
        return;
    }
    _mapId = map.GetDynamicZoneId();
    _zonePath = character.Zone;
    _worldGuid = character.Guid;
    placement.MobileId = *mobileId;

    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    CoreObjectTypeTablePtr const types = sObjectSchemaMgr.GetCoreObjectTypes();
    std::shared_ptr<BehaviorClientClasses const> const behaviors = sObjectSchemaMgr.GetBehaviorClientClasses();
    std::shared_ptr<ObjectTemplate const> const playerTemplate = sObjectTemplateMgr.GetPlayer();
    PropertyObjectPtr const player = PlayerObjectBuilder::Build(catalog, *types, *behaviors, *playerTemplate, character, *stats, placement, problem);
    ObjectField const* const field = ObjectFields::Find("MSG_LOGINCOMPLETE", "Data");
    EncodeResult const data = player && field ? CoreObjectSerializer::EncodeField(*field, *player, *types) : EncodeResult{};
    if (!player || !field || !data.Ok())
    {
        LeaveWorld();
        RefuseEntry(claim, player ? fmt::format("its object does not encode: {}", data.Detail) : fmt::format("its object cannot be built: {}", problem));
        return;
    }

    GameMessages::LoginComplete complete;
    complete.ZoneName = character.Zone;
    complete.Data.assign(data.Bytes.begin(), data.Bytes.end());
    complete.ServerTime = static_cast<uint32>(NowEpochSeconds());
    complete.ZoneId = StringHash::KiStringHash(character.Zone);
    complete.DynamicZoneId = map.GetDynamicZoneId();
    complete.DynamicServerProcId = map.GetDynamicZoneId();
    complete.Permissions = sSettings.Get<uint32>("LoginComplete.Permissions");
    complete.IsCsr = _securityLevel.load(std::memory_order_relaxed) >= sSettings.Get<uint32>("LoginComplete.CSRSecurityLevel") ? 1 : 0;
    complete.TestServer = sSettings.Get<bool>("LoginComplete.TestServer") ? 1 : 0;
    complete.RealmName = sSettings.Get<std::string>("Realm.Name");
    SetCharacterName(sCharacterNameMgr.FormatName(character.NameIndices, character.Appearance.Gender).value_or(std::string()));
    _stats = std::move(stats);
    _statsRevision = stored ? stored->Revision : 0;
    _movement.Reset({ placement.X, placement.Y, placement.Z, placement.Yaw }, 0);
    _characterRevision = character.StateRevision;
    SendDmlMessage(complete);
    SetStatus(SessionStatus::LoggedIn);
    LOG_DEBUG("server.gamesession", "Session {} sent MSG_LOGINCOMPLETE: zone {}, id {}, dynamic zone {} in process {}, server time {}, realm {}, permissions {:#x}, CSR {}, test server {}, critical objects {}",
        GetSessionId(), complete.ZoneName, complete.ZoneId, complete.DynamicZoneId, complete.DynamicServerProcId, complete.ServerTime, complete.RealmName, complete.Permissions,
        complete.IsCsr, complete.TestServer, complete.CriticalObjects.empty() ? "none" : "a list");
    LOG_INFO("server.gamesession", "Session {} put wizard {} in {} instance {} at ({}, {}, {}) with mobile id {}, level {} with {} of {} health and {} of {} mana, and sent its {}-byte object",
        GetSessionId(), character.Guid, character.Zone, map.GetDynamicZoneId(), placement.X, placement.Y, placement.Z, placement.MobileId, _stats->GetLevel(), _stats->GetHitpoints(),
        _stats->GetMaxHitpoints(), _stats->GetMana(), _stats->GetMaxMana(), data.Bytes.size());
}

void GameSession::HandleClientZoned(GameMessages::ClientZoned& message)
{
    uint32 const expected = StringHash::KiStringHash(_zonePath);
    if (message.ZoneNameId != expected)
    {
        LOG_WARN("server.gamesession", "Session {} says it loaded zone name id {}, but its wizard was sent to {} ({})", GetSessionId(), message.ZoneNameId,
            Ambrose::ForLog(_zonePath, 128), expected);
        return;
    }
    SetStatus(SessionStatus::InWorld);
    LOG_INFO("server.gamesession", "Session {} loaded {}, and wizard {} stands in the world", GetSessionId(), _zonePath, _worldGuid);
}

std::string GameSession::GetCharacterName() const
{
    std::lock_guard const lock(_nameMutex);
    return _characterName;
}

void GameSession::SetCharacterName(std::string name)
{
    std::lock_guard const lock(_nameMutex);
    _characterName = std::move(name);
}

void GameSession::SaveStats()
{
    if (!_stats || !CharacterDatabase.IsOpen())
        return;
    CharacterStats stored = _stats->ToStored();
    stored.Revision = ++_statsRevision;
    if (!CharacterRepository::IsValidStats(stored))
    {
        LOG_ERROR("server.gamesession", "Session {} did not save wizard {}'s stats, which hold a negative amount", GetSessionId(), _worldGuid);
        return;
    }
    if (CharacterRepository::Statement statement = CharacterRepository::PrepareSaveStats(_worldGuid, stored))
        CharacterDatabase.Execute(std::move(statement));
}

void GameSession::SavePosition(PlayerPosition const& position)
{
    if (!CharacterDatabase.IsOpen())
        return;
    if (CharacterRepository::Statement statement = CharacterRepository::PrepareSavePosition(_worldGuid, position.X, position.Y, position.Z, position.Yaw, ++_characterRevision))
        CharacterDatabase.Execute(std::move(statement));
    LOG_INFO("server.gamesession", "Session {} saved wizard {} at ({}, {}, {}) facing {} in {} after {} move(s)", GetSessionId(), _worldGuid, position.X, position.Y, position.Z, position.Yaw,
        Ambrose::ForLog(_zonePath, 128), _movement.GetMoves());
}

void GameSession::LeaveWorld()
{
    SetCharacterName(std::string());
    if (_stats)
    {
        SaveStats();
        _stats.reset();
    }
    if (std::optional<PlayerPosition> const moved = _movement.TakeWrite())
        SavePosition(*moved);
    if (!_mapId)
        return;
    if (Map* const map = sMapMgr.Find(*_mapId))
        sMapMgr.RemovePlayer(*map, _worldGuid);
    _mapId.reset();
}

void GameSession::RefuseEntry(LoginKeyClaim const& claim, std::string const& reason)
{
    LOG_WARN("server.gamesession", "Session {} cannot enter the world as account {} with wizard {}: {}", GetSessionId(), claim.AccountId, claim.CharacterId, reason);
    GameMessages::AttachFailed failed;
    failed.Error = 1;
    failed.Rejected = 1;
    failed.NoDisconnect = 0;
    SendDmlMessageDelayedClose(failed);
}

void GameSession::RefuseAttach(LoginKeyClaim const& claim, LoginKeyVerdict verdict)
{
    LOG_WARN("server.gamesession", "Session {} from {} was refused as account {} with wizard {}: {}",
        GetSessionId(), GetRemoteAddress().to_string(), claim.AccountId, claim.CharacterId, LoginKeyValidator::Describe(verdict));
    GameMessages::AttachFailed failed;
    failed.Error = 1;
    failed.Rejected = 1;
    failed.NoDisconnect = 0;
    SendDmlMessageDelayedClose(failed);
}
