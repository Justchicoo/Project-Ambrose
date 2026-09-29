/*
 * Project Ambrose by Imjustchico
 * Implements a connected game session from attach through in-world play: dispatches messages on the world thread, maintains and saves wizard state, and sends object, movement and player-wizbang updates to clients in the instance.
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
#include "SpellMgr.h"
#include "StringHash.h"
#include "StringUtil.h"
#include "TypeRegistry.h"
#include "ZoneMgr.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

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
    placement.Yaw = placed ? character.Orientation : start.Location.Yaw;

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

    PlayerSpellbook spellbook = PlayerSpellbook::FromStored(spells);
    std::vector<uint32> missing;
    std::vector<SpellTracker> const trackers = spellbook.Track(*sSpellMgr.GetSpells(), missing);
    if (!missing.empty())
        LOG_WARN("server.gamesession", "Session {} left {} spell(s) wizard {} knows out of its spellbook, since the spells this server holds do not name them: {}", GetSessionId(),
            missing.size(), character.Guid, fmt::join(missing, ", "));

    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    CoreObjectTypeTablePtr const types = sObjectSchemaMgr.GetCoreObjectTypes();
    std::shared_ptr<BehaviorClientClasses const> const behaviors = sObjectSchemaMgr.GetBehaviorClientClasses();
    std::shared_ptr<ObjectTemplate const> const playerTemplate = sObjectTemplateMgr.GetPlayer();
    PropertyObjectPtr const player = PlayerObjectBuilder::Build(catalog, *types, *behaviors, *playerTemplate, character, *stats, trackers, placement, problem);
    ObjectField const* const field = ObjectFields::Find("MSG_LOGINCOMPLETE", "Data");
    EncodeResult const data = player && field ? CoreObjectSerializer::EncodeField(*field, *player, *types) : EncodeResult{};
    if (!player || !field || !data.Ok())
    {
        LeaveWorld();
        RefuseEntry(claim, player ? fmt::format("its object does not encode: {}", data.Detail) : fmt::format("its object cannot be built: {}", problem));
        return;
    }

    ObjectField const* const shownField = ObjectFields::Find("MSG_NEWOBJECT", "Data");
    SerializerOptions shownOptions;
    shownOptions.Mask = SerializerOptions::PublicMask;
    EncodeResult shown = shownField ? CoreObjectSerializer::EncodeField(*shownField, *player, *types, shownOptions) : EncodeResult{};
    if (!shownField || !shown.Ok())
        LOG_WARN("server.gamesession", "Session {}'s wizard {} cannot be shown to other wizards: {}", GetSessionId(), character.Guid,
            shownField ? shown.Detail : std::string("MSG_NEWOBJECT's Data is not declared"));

    std::string criticalProblem;
    std::vector<uint8> const critical = MapObjectSpawner::EncodeCriticalObjects(catalog, map, criticalProblem);
    if (!criticalProblem.empty())
        LOG_WARN("server.gamesession", "Session {} sends wizard {} no critical objects for {}: {}", GetSessionId(), character.Guid, character.Zone, criticalProblem);

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
    complete.CriticalObjects.assign(critical.begin(), critical.end());
    SetCharacterName(sCharacterNameMgr.FormatName(character.NameIndices, character.Appearance.Gender).value_or(std::string()));
    _stats = std::move(stats);
    _statsRevision = stored ? stored->Revision : 0;
    _spellbook = std::move(spellbook);
    _movement.Reset({ placement.X, placement.Y, placement.Z, placement.Yaw }, 0);
    _relay.Reset(_movement);
    _mobileId = placement.MobileId;
    _publicObject = shown.Ok() ? std::move(shown.Bytes) : std::vector<uint8>();
    _characterRevision = character.StateRevision;
    SendDmlMessage(complete);
    SendMapObjects(map);
    _arrived = true;
    SetStatus(SessionStatus::LoggedIn);
    LOG_DEBUG("server.gamesession", "Session {} sent MSG_LOGINCOMPLETE: zone {}, id {}, dynamic zone {} in process {}, server time {}, realm {}, permissions {:#x}, CSR {}, test server {}, critical objects {}",
        GetSessionId(), complete.ZoneName, complete.ZoneId, complete.DynamicZoneId, complete.DynamicServerProcId, complete.ServerTime, complete.RealmName, complete.Permissions,
        complete.IsCsr, complete.TestServer, complete.CriticalObjects.empty() ? "none" : "a list");
    LOG_INFO("server.gamesession", "Session {} put wizard {} in {} instance {} at ({}, {}, {}) with mobile id {}, level {} with {} of {} health and {} of {} mana and {} spell(s) in its book, and sent its {}-byte object and the zone's {} object(s)",
        GetSessionId(), character.Guid, character.Zone, map.GetDynamicZoneId(), placement.X, placement.Y, placement.Z, placement.MobileId, _stats->GetLevel(), _stats->GetHitpoints(),
        _stats->GetMaxHitpoints(), _stats->GetMana(), _stats->GetMaxMana(), trackers.size(), data.Bytes.size(), map.GetObjects().size());
}

void GameSession::ShowPlayer(GameSession const& other)
{
    GameMessages::NewObject message;
    message.Data.assign(other._publicObject.begin(), other._publicObject.end());
    SendDmlMessage(message);
    ShowMovementOf(other, other._relay.Current(other._movement));
    if (other._wizBangId != 0)
        ShowWizBangOf(other._worldGuid, other._wizBangId);
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

void GameSession::SendMapObjects(Map const& map)
{
    for (MapObject const& object : map.GetObjects())
    {
        GameMessages::NewObject message;
        message.Data.assign(object.Data.begin(), object.Data.end());
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
