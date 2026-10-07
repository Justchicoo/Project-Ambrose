/*
 * Project Ambrose by Imjustchico
 * Implements game-session attachment, queued world-thread message handling, wizard persistence, chat, outbound instance updates, and the custom emotes a wizard owns.
 */

#include "GameSession.h"
#include "AccountMgr.h"
#include "CharacterNameMgr.h"
#include "CharacterRepository.h"
#include "CoreObjectSerializer.h"
#include "Frame.h"
#include "GameMessageTable.h"
#include "BlobEnvelope.h"
#include "ConfigMgr.h"
#include "Log.h"
#include "MapMgr.h"
#include "MessageRegistry.h"
#include "MovementPacking.h"
#include "ObjectFields.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "PlayerLevelMgr.h"
#include "PackedName.h"
#include "PlayerObjectBuilder.h"
#include "Settings.h"
#include "SpellMgr.h"
#include "StringHash.h"
#include "StringUtil.h"
#include "TypeRegistry.h"
#include "ZoneMgr.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
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
    if (IsLinkDead())
    {
        uint32 const linkDeadTime = sSettings.Get<uint32>("Player.LinkDeadTime");
        int64 const lostAt = _socketLostAtNanoseconds.load(std::memory_order_relaxed);
        auto const lost = std::chrono::steady_clock::time_point(std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::nanoseconds(lostAt)));
        if (linkDeadTime == 0 || now - lost >= std::chrono::seconds(linkDeadTime))
        {
            _linkDead.store(false, std::memory_order_relaxed);
            _inWorld.store(false, std::memory_order_relaxed);
            MarkOffline();
            return;
        }
        if (!_linkDeadNotified)
        {
            _linkDeadNotified = true;
            _relay.Stop();
            bool const wasPendingArrival = std::exchange(_arrived, false);
            _linkDeadStartPending.store(!wasPendingArrival, std::memory_order_relaxed);
        }
        return;
    }

    if (!IsOpen())
        return;
    if (IsAttached() && _inWorld.load(std::memory_order_relaxed) && GetStatus() == SessionStatus::InWorld)
    {
        if (!_afkTimerStarted)
        {
            _afkStarted = now;
            _afkTimerStarted = true;
        }
        uint32 const afkTime = sSettings.Get<uint32>("Player.AfkTime");
        if (afkTime != 0)
        {
            auto const idle = now - _afkStarted;
            uint32 const warningAt = std::min(sSettings.Get<uint32>("Player.AfkWarnTime"), afkTime);
            if (!_afkWarned && idle >= std::chrono::seconds(warningAt))
            {
                GameMessages::DisconnectAfk warning;
                SendDmlMessage(warning);
                _afkWarned = true;
            }
            if (idle >= std::chrono::seconds(afkTime))
            {
                _intentionalDisconnect.store(true, std::memory_order_relaxed);
                GameMessages::DisconnectAfk disconnect;
                disconnect.Warning = 0;
                SendDmlMessageDelayedClose(disconnect);
                LOG_INFO("server.gamesession", "Session {} disconnected wizard {} after {} s idle", GetSessionId(), GetCharacterId(), afkTime);
                return;
            }
        }
        return;
    }
    if (IsAttached() || _attaching.load(std::memory_order_relaxed))
        return;
    std::chrono::milliseconds const timeout = GetContext().GetSettings().AttachTimeout;
    if (now - _connectedAt < timeout)
        return;
    LOG_INFO("server.gamesession", "Session {} from {} sent no MSG_ATTACH within Attach.Timeout of {} s; closing it",
        GetSessionId(), GetRemoteAddress().to_string(), std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
    CloseSocket();
}

std::chrono::duration<float> GameSession::GetLinkDeadRemaining(std::chrono::steady_clock::time_point now) const
{
    if (!IsLinkDead())
        return std::chrono::duration<float>::zero();
    int64 const lostAt = _socketLostAtNanoseconds.load(std::memory_order_relaxed);
    auto const lost = std::chrono::steady_clock::time_point(std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::nanoseconds(lostAt)));
    std::chrono::duration<float> const remaining = std::chrono::seconds(sSettings.Get<uint32>("Player.LinkDeadTime")) - (now - lost);
    return std::max(remaining, std::chrono::duration<float>::zero());
}

bool GameSession::CanResume(std::chrono::steady_clock::time_point now) const
{
    return IsLinkDead() && GetLinkDeadRemaining(now).count() > 0.0f;
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
    if (!IsKicked() && !_intentionalDisconnect.load(std::memory_order_relaxed) && !_superseded.load(std::memory_order_relaxed) &&
        _attached.load(std::memory_order_relaxed) && _inWorld.load(std::memory_order_relaxed) &&
        sSettings.Get<uint32>("Player.LinkDeadTime") != 0)
    {
        _socketLostAtNanoseconds.store(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_relaxed);
        _linkDead.store(true, std::memory_order_relaxed);
    }
    else
        MarkOffline();
    _countedCallbacks.Clear();
    _queryCallbacks.Clear();
    SessionBase::OnSessionClosed();
}

void GameSession::MarkOffline()
{
    if (_attached.exchange(false, std::memory_order_relaxed))
    {
        LoginKeyClaim claim;
        claim.AccountId = GetAccountId();
        claim.CharacterId = GetCharacterId();
        claim.RealmId = GetRealmId();
        LoginKeyValidator::MarkOffline(claim);
    }
}

void GameSession::HandleAttach(GameMessages::Attach& message)
{
    LoginKeyClaim claim;
    claim.Key = message.LoginKey;
    claim.AccountId = message.UserId;
    claim.CharacterId = message.CharId;
    claim.RealmId = GetRealmId();
    _reattach = message.Reattach;

    LOG_INFO("server.gamesession", "Session {} from {} is attaching as account {} with wizard {} for zone {} at {}, Reattach={}, on a key of {} character(s)",
        GetSessionId(), GetRemoteAddress().to_string(), message.UserId, message.CharId, Ambrose::ForLog(message.ZoneName, 128),
        Ambrose::ForLog(message.Location, 64), message.Reattach, message.LoginKey.size());

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

void GameSession::HandleQueryLogout(GameMessages::QueryLogout& message)
{
    if (message.IsInstance == 0)
    {
        _intentionalDisconnect.store(true, std::memory_order_relaxed);
        SendDmlMessage(GameMessages::ClientDisconnect{});
    }
    SendDmlMessage(message);
    LOG_INFO("server.gamesession", "Session {} replied to MSG_QUERY_LOGOUT with IsInstance={}{}", GetSessionId(), message.IsInstance,
        message.IsInstance == 0 ? ", after MSG_CLIENT_DISCONNECT, which sends its client back to the login server" : "");
}

void GameSession::HandleClientDisconnect(GameMessages::ClientDisconnect&)
{
    _intentionalDisconnect.store(true, std::memory_order_relaxed);
    CloseSocket();
}

void GameSession::HandleNotAfk(GameMessages::NotAfk&)
{
    _afkStarted = std::chrono::steady_clock::now();
    _afkTimerStarted = true;
    _afkWarned = false;
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
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.IsOpen() ? AccountMgr::PrepareGetAccountByIdWithMute(claim.AccountId, AccountMgr::Now()) : nullptr;
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
        SetSecurityLevel(account.SecurityLevel);
        SetChatMode(account.ChatMode);
        if (_chatMode > 2)
            LOG_WARN("server.gamesession", "Session {}'s account {} has chat_mode {}; chat permissions are disabled until it is corrected", GetSessionId(), account.Id, _chatMode);
        if (std::optional<AccountMute> const mute = AccountMgr::ReadAccountMuteRow(*result))
            _muteUntil = mute->Until;
        _accountPermissions = account.Permissions;
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
        LoadSpells(claim, character, CharacterRepository::ReadStats(*result));
    }));
}

void GameSession::LoadSpells(LoginKeyClaim const& claim, CharacterSummary character, std::optional<CharacterStats> stored)
{
    CharacterRepository::Statement statement = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareLoadSpells(character.Guid) : nullptr;
    if (!statement)
    {
        RefuseEntry(claim, "the characters database is not open");
        return;
    }
    _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler()).WithPreparedCallback(
        [this, claim, character = std::move(character), stored = std::move(stored)](PreparedQueryResult result)
    {
        if (!IsOpen() || IsKicked())
            return;
        if (!result)
        {
            RefuseEntry(claim, fmt::format("wizard {}'s spellbook cannot be read from the characters database", character.Guid));
            return;
        }
        std::vector<CharacterSpell> spells = CharacterRepository::ReadSpells(*result);
        std::shared_ptr<GameSession> const self = SharedSelf();
        if (!QueueInbound([self, claim, character, stored, spells = std::move(spells)] { self->EnterWorld(claim, character, stored, spells); }))
            RefuseEntry(claim, "its queue of work is full");
    }));
}

void GameSession::EnterWorld(LoginKeyClaim const& claim, CharacterSummary const& character, std::optional<CharacterStats> const& stored, std::vector<CharacterSpell> const& spells)
{
    if (!_world)
    {
        RefuseEntry(claim, "the world session registry is unavailable");
        return;
    }
    CharacterSummary entering = character;
    std::shared_ptr<GameSession> previous = _world->FindSessionByCharacterId(character.Guid, this);
    Map* map = nullptr;
    uint16 mobileId = 0;
    bool resumed = false;
    std::optional<PlayerStats> resumedStats;
    std::optional<PlayerSpellbook> resumedSpellbook;
    PlayerMovement movement;
    MovementRelay relay;
    if (previous && previous->IsLinkDead() && !previous->CanResume(std::chrono::steady_clock::now()))
    {
        previous->LeaveWorld();
        _world->RemoveSession(previous.get());
        previous.reset();
        LoginKeyValidator::MarkOnline(claim);
    }
    if (previous && previous->_mapId)
    {
        map = sMapMgr.Find(*previous->_mapId);
        std::optional<uint16> const existingMobile = map ? map->GetMobileId(character.Guid) : std::nullopt;
        if (map && existingMobile)
        {
            resumed = true;
            mobileId = *existingMobile;
            entering.Zone = previous->_zonePath;
            PlayerPosition const& current = previous->_movement.GetPosition();
            entering.PositionX = current.X;
            entering.PositionY = current.Y;
            entering.PositionZ = current.Z;
            entering.Orientation = current.Yaw;
            resumedStats = previous->_stats;
            resumedSpellbook = previous->_spellbook;
            movement = previous->_movement;
            relay = previous->_relay;
        }
    }
    if (previous && !resumed)
    {
        previous->_superseded.store(true, std::memory_order_relaxed);
        previous->_intentionalDisconnect.store(true, std::memory_order_relaxed);
        previous->_attached.store(false, std::memory_order_relaxed);
        if (previous->IsOpen())
            previous->Kick("replaced by a newer attach for the same character");
        previous.reset();
    }

    std::string problem;
    std::optional<PlayerStats> stats = resumedStats ? std::move(resumedStats) :
        PlayerStats::Create(entering, stored, *sPlayerLevelMgr.GetLevels(), *sPlayerLevelMgr.GetStats(), problem);
    if (!stats)
    {
        RefuseEntry(claim, problem);
        return;
    }

    bool const placed = entering.PositionX != 0.0f || entering.PositionY != 0.0f || entering.PositionZ != 0.0f;
    ZonePlace const start = sZoneMgr.FindPlace(entering.Zone, ZoneLocations::StartName);
    if (!start.Found())
    {
        RefuseEntry(claim, fmt::format("wizard {} is in {}, and {}", character.Guid, Ambrose::ForLog(entering.Zone, 128), ZoneMgr::GetLookupName(start.Result)));
        return;
    }
    PlayerPlacement placement;
    placement.X = resumed ? movement.GetPosition().X : placed ? entering.PositionX : start.Location.X;
    placement.Y = resumed ? movement.GetPosition().Y : placed ? entering.PositionY : start.Location.Y;
    placement.Z = resumed ? movement.GetPosition().Z : placed ? entering.PositionZ : start.Location.Z;
    placement.Yaw = resumed ? movement.GetPosition().Yaw : placed ? entering.Orientation : start.Location.Yaw;

    if (!resumed)
    {
        map = &sMapMgr.FindOrCreatePublic(entering.Zone);
        std::optional<uint16> const addedMobile = sMapMgr.AddPlayer(*map, character.Guid);
        if (!addedMobile)
        {
            RefuseEntry(claim, fmt::format("instance {} of {} has no mobile id left", map->GetDynamicZoneId(), entering.Zone));
            return;
        }
        mobileId = *addedMobile;
        _mapId = map->GetDynamicZoneId();
        _zonePath = entering.Zone;
        _worldGuid = character.Guid;
    }
    if (!map)
    {
        RefuseEntry(claim, fmt::format("wizard {}'s existing instance is no longer available", character.Guid));
        return;
    }
    placement.MobileId = mobileId;
    _chatName = PackedName::ForWizard(entering.CustomName, entering.NameIndices, entering.Appearance.Gender);

    PlayerSpellbook spellbook = resumedSpellbook ? std::move(*resumedSpellbook) : PlayerSpellbook::FromStored(spells);
    std::vector<uint32> missing;
    std::vector<SpellTracker> const trackers = spellbook.Track(*sSpellMgr.GetSpells(), missing);
    if (!missing.empty())
        LOG_WARN("server.gamesession", "Session {} left {} spell(s) wizard {} knows out of its spellbook, since the spells this server holds do not name them: {}", GetSessionId(),
            missing.size(), character.Guid, fmt::join(missing, ", "));

    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    CoreObjectTypeTablePtr const types = sObjectSchemaMgr.GetCoreObjectTypes();
    std::shared_ptr<BehaviorClientClasses const> const behaviors = sObjectSchemaMgr.GetBehaviorClientClasses();
    std::shared_ptr<ObjectTemplate const> const playerTemplate = sObjectTemplateMgr.GetPlayer();
    uint32 const permissions = ChatMgr::PermissionsForMode(AccountMgr::EntryPermissions(_accountPermissions, sSettings.Get<uint32>("LoginComplete.Permissions")), _chatMode);
    PropertyObjectPtr const player = PlayerObjectBuilder::Build(catalog, *types, *behaviors, *playerTemplate, entering, *stats, trackers, placement, permissions, problem);
    ObjectField const* const field = ObjectFields::Find("MSG_LOGINCOMPLETE", "Data");
    EncodeResult const data = player && field ? CoreObjectSerializer::EncodeField(*field, *player, *types) : EncodeResult{};
    if (!player || !field || !data.Ok())
    {
        if (!resumed)
            LeaveWorld();
        RefuseEntry(claim, player ? fmt::format("its object does not encode: {}", data.Detail) : fmt::format("its object cannot be built: {}", problem));
        return;
    }
    if (std::string const folder = sSettings.Get<std::string>("LoginComplete.SaveDataTo"); !folder.empty())
    {
        std::error_code made;
        std::filesystem::path const path = ConfigMgr::PathFromUtf8(folder) / fmt::format("logincomplete-{}-{}.bin", character.Guid, NowEpochSeconds());
        std::filesystem::create_directories(path.parent_path(), made);
        std::ofstream saved(path, std::ios::binary | std::ios::trunc);
        saved.write(reinterpret_cast<char const*>(data.Bytes.data()), static_cast<std::streamsize>(data.Bytes.size()));
        if (saved)
            LOG_INFO("server.gamesession", "Session {} saved the MSG_LOGINCOMPLETE Data it sends to {}", GetSessionId(), ConfigMgr::PathToUtf8(path));
        else
            LOG_WARN("server.gamesession", "Session {} could not save its MSG_LOGINCOMPLETE Data to {}", GetSessionId(), ConfigMgr::PathToUtf8(path));
    }
    if (sLog.ShouldLog("server.gamesession", LogLevel::Debug))
    {
        BlobEnvelope::UnwrapResult const inner = BlobEnvelope::Unwrap(data.Bytes, BlobEnvelope::MaxPayloadSize);
        DecodeResult const back = CoreObjectSerializer::DecodeField(catalog, *field, data.Bytes, *types);
        LOG_DEBUG("server.gamesession", "Session {}'s MSG_LOGINCOMPLETE Data holds {} bytes inside its envelope, beginning {:02x}, and {} whole with {} issue(s)", GetSessionId(),
            inner.Data.size(), fmt::join(std::span<uint8 const>(inner.Data).first(std::min<std::size_t>(inner.Data.size(), 6)), " "),
            back.Ok() && back.Object ? "decodes" : "does not decode", back.Issues.size());
    }

    ObjectField const* const shownField = ObjectFields::Find("MSG_NEWOBJECT", "Data");
    SerializerOptions shownOptions;
    shownOptions.Mask = SerializerOptions::PublicMask;
    EncodeResult shown = shownField ? CoreObjectSerializer::EncodeField(*shownField, *player, *types, shownOptions) : EncodeResult{};
    if (!shownField || !shown.Ok())
        LOG_WARN("server.gamesession", "Session {}'s wizard {} cannot be shown to other wizards: {}", GetSessionId(), entering.Guid,
            shownField ? shown.Detail : std::string("MSG_NEWOBJECT's Data is not declared"));

    std::string criticalProblem;
    std::vector<uint8> const critical = MapObjectSpawner::EncodeCriticalObjects(catalog, *map, criticalProblem);
    if (!criticalProblem.empty())
        LOG_WARN("server.gamesession", "Session {} sends wizard {} no critical objects for {}: {}", GetSessionId(), entering.Guid, entering.Zone, criticalProblem);

    GameMessages::LoginComplete complete;
    complete.ZoneName = entering.Zone;
    complete.Data.assign(data.Bytes.begin(), data.Bytes.end());
    complete.ServerTime = static_cast<uint32>(NowEpochSeconds());
    complete.ZoneId = StringHash::KiStringHash(entering.Zone);
    complete.DynamicZoneId = map->GetDynamicZoneId();
    complete.DynamicServerProcId = map->GetDynamicZoneId();
    complete.Permissions = permissions;
    _chatFilter = ChatMgr::FilterFor(complete.Permissions);
    complete.IsCsr = _securityLevel.load(std::memory_order_relaxed) >= sSettings.Get<uint32>("LoginComplete.CSRSecurityLevel") ? 1 : 0;
    complete.TestServer = sSettings.Get<bool>("LoginComplete.TestServer") ? 1 : 0;
    complete.RealmName = sSettings.Get<std::string>("Realm.Name");
    complete.CriticalObjects.assign(critical.begin(), critical.end());
    SetCharacterName(sCharacterNameMgr.FormatName(entering.NameIndices, entering.Appearance.Gender).value_or(std::string()));
    if (resumed && previous)
        previous->TransferWorldStateTo(*this);
    else
    {
        _mapId = map->GetDynamicZoneId();
        _zonePath = entering.Zone;
        _worldGuid = character.Guid;
        _mobileId = mobileId;
        _movement.Reset({ placement.X, placement.Y, placement.Z, placement.Yaw }, 0);
        _relay.Reset(_movement);
        _characterRevision = entering.StateRevision;
    }
    _stats = std::move(stats);
    if (!resumed)
        _statsRevision = stored ? stored->Revision : 0;
    _spellbook = std::move(spellbook);
    if (resumed)
    {
        _movement = std::move(movement);
        _relay = std::move(relay);
    }
    _publicObject = shown.Ok() ? std::move(shown.Bytes) : std::vector<uint8>();
    _inWorld.store(true, std::memory_order_relaxed);
    _afkTimerStarted = false;
    SendDmlMessage(complete);
    if (_muteUntil != 0)
        SendMuteNotice();
    SendBadges();
    SendMapObjects(*map);
    SendCustomEmotes();
    if (!resumed)
        _arrived = true;
    SetStatus(SessionStatus::LoggedIn);
    LOG_DEBUG("server.gamesession", "Session {} sent MSG_LOGINCOMPLETE: zone {}, id {}, dynamic zone {} in process {}, server time {}, realm {}, permissions {:#x}, CSR {}, test server {}, critical objects {}",
        GetSessionId(), complete.ZoneName, complete.ZoneId, complete.DynamicZoneId, complete.DynamicServerProcId, complete.ServerTime, complete.RealmName, complete.Permissions,
        complete.IsCsr, complete.TestServer, complete.CriticalObjects.empty() ? "none" : "a list");
    LOG_INFO("server.gamesession", "Session {} put wizard {} in {} instance {} at ({}, {}, {}) with mobile id {}, level {} with {} of {} health and {} of {} mana and {} spell(s) in its book, and sent its {}-byte object and the zone's {} object(s)",
        GetSessionId(), character.Guid, entering.Zone, map->GetDynamicZoneId(), placement.X, placement.Y, placement.Z, placement.MobileId, _stats->GetLevel(), _stats->GetHitpoints(),
        _stats->GetMaxHitpoints(), _stats->GetMana(), _stats->GetMaxMana(), trackers.size(), data.Bytes.size(), map->GetObjects().size());
}

void GameSession::ApplyMute(uint64 until)
{
    _muteUntil = until;
    if (IsOpen())
        SendMuteNotice();
}

void GameSession::ClearMute()
{
    if (_muteUntil == 0)
        return;
    _muteUntil = 0;
    if (IsOpen())
        SendServerMessage(u"You have been unmuted.");
}

bool GameSession::RejectMutedSpeech()
{
    if (_muteUntil == 0)
        return false;
    if (NowEpochSeconds() >= static_cast<int64>(_muteUntil))
    {
        ClearMute();
        return false;
    }
    SendMuteNotice();
    return true;
}

void GameSession::SendMuteNotice()
{
    int64 const remaining = static_cast<int64>(_muteUntil) - NowEpochSeconds();
    if (remaining <= 0 || !IsOpen())
        return;
    GameMessages::Mute message;
    message.MuteTime = fmt::format("{}", remaining);
    message.ForceMessage = 1;
    SendDmlMessage(message);
}

void GameSession::ShowPlayer(GameSession const& other)
{
    GameMessages::NewObject message;
    message.Data.assign(other._publicObject.begin(), other._publicObject.end());
    SendDmlMessage(message);
    ShowMovementOf(other, other._relay.Current(other._movement));
    if (other._wizBangId != 0)
        ShowWizBangOf(other._worldGuid, other._wizBangId);
    if (other.IsLinkDead() && other._linkDeadNotified)
        ShowZombiePlayer(other);
}

void GameSession::ShowZombiePlayer(GameSession const& other)
{
    GameMessages::ZombiePlayer message;
    message.GlobalId = other._worldGuid;
    message.Remaining = other.GetLinkDeadRemaining(std::chrono::steady_clock::now()).count();
    SendDmlMessage(message);
}

void GameSession::HidePlayer(uint64 worldGuid)
{
    GameMessages::RemoveObject message;
    message.GameObjectId = worldGuid;
    SendDmlMessage(message);
}

void GameSession::ShowWizBangOf(uint64 worldGuid, uint32 wizBangId)
{
    GameMessages::WizBang message;
    message.GameObjectId = worldGuid;
    message.WizBangId = wizBangId;
    SendDmlMessage(message);
}

std::optional<uint8> GameSession::TakeJump() noexcept
{
    return std::exchange(_jump, std::nullopt);
}

void GameSession::ShowStateOf(uint64 worldGuid, uint32 state)
{
    GameMessages::EnterState message;
    message.GameObjectId = worldGuid;
    message.State = state;
    SendDmlMessage(message);
}

bool GameSession::TakeArrival() noexcept
{
    if (IsLinkDead() && !_linkDeadNotified)
        return false;
    return std::exchange(_arrived, false);
}

std::optional<WorldDeparture> GameSession::TakeDeparture() noexcept
{
    return std::exchange(_departure, std::nullopt);
}

MovementUpdate GameSession::TakeMovementUpdate(uint32 idleFlushes)
{
    if (!_mapId || _publicObject.empty())
        return {};
    return _relay.Take(_movement, idleFlushes);
}

void GameSession::ShowMovementOf(GameSession const& mover, MovementUpdate const& update)
{
    if (update.Move)
    {
        GameMessages::ServerMove move;
        move.LocationX = update.Move->X;
        move.LocationY = update.Move->Y;
        move.LocationZ = update.Move->Z;
        move.Direction = update.Move->Direction;
        move.MobileId = mover._mobileId;
        SendDmlMessage(move);
    }
    if (update.State)
    {
        GameMessages::MoveState state;
        state.GlobalId = mover._worldGuid;
        state.NewState = *update.State;
        SendDmlMessage(state);
    }
}

bool GameSession::TeleportWithinMap(PlayerPosition const& target, std::vector<std::shared_ptr<GameSession>> const& onlookers, std::string& problem)
{
    if (!MovementPacking::TryPackLocation(target.X) || !MovementPacking::TryPackLocation(target.Y) || !MovementPacking::TryPackLocation(target.Z))
    {
        problem = fmt::format("({}, {}, {}) lies outside the {} to {} a position can be sent as", target.X, target.Y, target.Z,
            MovementPacking::UnpackLocation(std::numeric_limits<int16>::min()), MovementPacking::UnpackLocation(std::numeric_limits<int16>::max()));
        return false;
    }
    if (!IsShown())
    {
        problem = "the wizard does not stand in a zone";
        return false;
    }
    std::optional<PackedMove> const teleported = _movement.Teleport(target);
    if (!teleported)
    {
        problem = fmt::format("the facing {} is not a number", target.Yaw);
        return false;
    }
    PackedMove const place = *teleported;
    _relay.Reset(_movement);
    std::size_t shown = 0;
    for (std::shared_ptr<GameSession> const& viewer : onlookers)
    {
        if (!viewer->IsOpen() || viewer->GetMapId() != _mapId)
            continue;
        viewer->ShowTeleportOf(*this, place);
        if (viewer.get() != this)
            ++shown;
    }
    LOG_INFO("server.gamesession", "Session {}'s wizard {} was teleported within {} to ({}, {}, {}) facing {}, shown to {} other wizard(s)", GetSessionId(), _worldGuid,
        Ambrose::ForLog(_zonePath, 128), _movement.GetPosition().X, _movement.GetPosition().Y, _movement.GetPosition().Z, _movement.GetPosition().Yaw, shown);
    return true;
}

void GameSession::ShowTeleportOf(GameSession const& mover, PackedMove const& place)
{
    GameMessages::ServerTeleport teleport;
    teleport.LocationX = place.X;
    teleport.LocationY = place.Y;
    teleport.LocationZ = place.Z;
    teleport.Direction = place.Direction;
    teleport.MobileId = mover._mobileId;
    SendDmlMessage(teleport);
}

void GameSession::SendMapObjects(Map const& map)
{
    for (MapObject const& object : map.GetObjects())
    {
        GameMessages::NewObject message;
        message.Data.assign(object.Data.begin(), object.Data.end());
        SendDmlMessage(message);
    }
}

void GameSession::SendCustomEmotes()
{
    std::array<uint32, 3> const& emotes = _stats->GetPurchasedCustomEmotes();
    std::array<uint32, 3> const& teleportEffects = _stats->GetPurchasedCustomTeleportEffects();
    for (uint8 rank = 0; rank < emotes.size(); ++rank)
    {
        GameMessages::UpdateCustomEmotes message;
        message.CustomEmotes = emotes[rank];
        message.CustomTeleportEffects = teleportEffects[rank];
        message.Rank = rank;
        SendDmlMessage(message);
    }
}

void GameSession::SendObjectChanges(MapObjectChanges const& changes)
{
    if (!_mapId || *_mapId != changes.DynamicZoneId)
        return;
    for (uint64 const removed : changes.Removed)
    {
        GameMessages::RemoveObject message;
        message.GameObjectId = removed;
        SendDmlMessage(message);
    }
    Map const* const map = sMapMgr.Find(*_mapId);
    if (!map)
        return;
    for (uint64 const added : changes.Added)
        if (MapObject const* const object = map->FindObject(added))
        {
            GameMessages::NewObject message;
            message.Data.assign(object->Data.begin(), object->Data.end());
            SendDmlMessage(message);
        }
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
    if (GetStatus() != SessionStatus::InWorld)
    {
        _afkStarted = std::chrono::steady_clock::now();
        _afkTimerStarted = true;
        _afkWarned = false;
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

SpellbookChange GameSession::LearnSpell(uint32 spellId)
{
    if (!_spellbook)
        return SpellbookChange::NotInWorld;
    std::shared_ptr<SpellStore const> const spells = sSpellMgr.GetSpells();
    SpellInfo const* const spell = spells->Find(spellId);
    if (!spell)
        return SpellbookChange::NoSuchSpell;
    std::optional<CharacterSpell> const row = _spellbook->Learn(spellId);
    if (!row)
        return SpellbookChange::AlreadyKnown;
    SaveSpell(*row);
    GameMessages::AddSpellToBook added;
    added.SpellId = static_cast<int32>(spellId);
    SendDmlMessage(added);
    LOG_INFO("server.gamesession", "Session {} taught wizard {} {} ({}), and its spellbook holds {} spell(s)", GetSessionId(), _worldGuid, spell->Name, spellId,
        _spellbook->GetSpells().size());
    return SpellbookChange::Learned;
}

SpellbookChange GameSession::UnlearnSpell(uint32 spellId)
{
    if (!_spellbook)
        return SpellbookChange::NotInWorld;
    std::optional<CharacterSpell> const row = _spellbook->Unlearn(spellId);
    if (!row)
        return SpellbookChange::NotKnown;
    SaveSpell(*row);
    GameMessages::RemoveSpellFromBook removed;
    removed.SpellId = static_cast<int32>(spellId);
    SendDmlMessage(removed);
    LOG_INFO("server.gamesession", "Session {} took spell {} from wizard {}, and its spellbook holds {} spell(s)", GetSessionId(), spellId, _worldGuid, _spellbook->GetSpells().size());
    return SpellbookChange::Unlearned;
}

void GameSession::SaveSpell(CharacterSpell const& spell)
{
    CharacterRepository::Statement statement = CharacterDatabase.IsOpen() ? CharacterRepository::PrepareSaveSpell(_worldGuid, spell) : nullptr;
    if (!statement)
    {
        LOG_ERROR("server.gamesession", "Session {} could not write spell {} of wizard {}'s spellbook, since the characters database is not open", GetSessionId(), spell.SpellId, _worldGuid);
        return;
    }
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
    _inWorld.store(false, std::memory_order_relaxed);
    _linkDead.store(false, std::memory_order_relaxed);
    _linkDeadStartPending.store(false, std::memory_order_relaxed);
    MarkOffline();
    SetCharacterName(std::string());
    _wizBangId = 0;
    _pendingWizBang.reset();
    if (_stats)
    {
        SaveStats();
        _stats.reset();
    }
    _spellbook.reset();
    if (std::optional<PlayerPosition> const moved = _movement.TakeWrite())
        SavePosition(*moved);
    if (!_mapId)
        return;
    if (!_publicObject.empty())
        _departure = WorldDeparture{ *_mapId, _worldGuid };
    _publicObject.clear();
    _arrived = false;
    _jump.reset();
    _speech.clear();
    if (Map* const map = sMapMgr.Find(*_mapId))
        sMapMgr.RemovePlayer(*map, _worldGuid);
    _mapId.reset();
}

void GameSession::TransferWorldStateTo(GameSession& replacement)
{
    replacement._mapId = _mapId;
    replacement._zonePath = _zonePath;
    replacement._worldGuid = _worldGuid;
    replacement._mobileId = _mobileId;
    replacement._movement = _movement;
    replacement._relay = _relay;
    replacement._characterRevision = _characterRevision;
    replacement._statsRevision = _statsRevision;
    replacement._arrived = _arrived;
    _superseded.store(true, std::memory_order_relaxed);
    _intentionalDisconnect.store(true, std::memory_order_relaxed);
    _attached.store(false, std::memory_order_relaxed);
    _inWorld.store(false, std::memory_order_relaxed);
    _linkDead.store(false, std::memory_order_relaxed);
    _linkDeadStartPending.store(false, std::memory_order_relaxed);
    _linkDeadNotified = false;
    _mapId.reset();
    _publicObject.clear();
    _stats.reset();
    _spellbook.reset();
    _worldGuid = 0;
    _zonePath.clear();
    _movement.Reset({}, 0);
    _relay.Reset(_movement);
    SetCharacterId(0);
    _arrived = false;
    _departure.reset();
    SetCharacterName(std::string());
    if (IsOpen())
        Kick("replaced by a newer attach for the same character");
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
