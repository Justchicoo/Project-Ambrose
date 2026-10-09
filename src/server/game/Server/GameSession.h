/*
 * Project Ambrose by Imjustchico
 * A connected game client and the world-thread-owned wizard behind it: attach spends a one-use handoff key, loads and checks the character, then gives the client its object; movement, spellbook, backpack and live player stats stay with the world thread, stat changes update the HUD and character persistence, and the final position is saved on a clean exit, disconnect expiry or server stop. Intentional exits mark the character offline immediately, link-dead sockets retain the wizard and online claim for a live-configured grace period, and a replacement attach can take over the existing world placement without creating a duplicate. What its wizard says and the emotes it plays are kept until the world's next tick shows them to the wizards around it, which hear them under the name the client's name codec packs for it and the chat level its permissions give it, and a command line its account may run is run at the account's security level, with the reply sent back to its own chat window, and the wizbang its wizard's client names is kept for the wizards around it and shown to each that comes to see it, as are the game effects its wizard carries, each added or taken away on the world thread and shown to the wizard and every wizard in its instance at the next tick, and to each that comes to see it after the object. Its friend, best-friend, friend-cap and ignore messages are answered through the social manager, and the display name of the zone its wizard stands in is kept for the presence its friends are shown. Its backpack holds as many items as Inventory.Slots, read as it enters and shown to its client, and the live Inventory.ExtraSlots allow, read at each add, so an add to a full one is refused with MSG_ITEMDROP and stores nothing, and an item it trashes is taken only from its own backpack.
 */

#ifndef AMBROSE_GAMESESSION_H
#define AMBROSE_GAMESESSION_H

#include "AsyncCallbackProcessor.h"
#include "CharacterItem.h"
#include "CharacterSpell.h"
#include "CharacterStats.h"
#include "CharacterSummary.h"
#include "ChatMgr.h"
#include "GameEffectHolder.h"
#include "GameMessages.h"
#include "GameSessionWorld.h"
#include "ItemTemplateRecord.h"
#include "LoginKeyValidator.h"
#include "LootListBuilder.h"
#include "MapObjectSpawner.h"
#include "MovementRelay.h"
#include "PlayerBackpack.h"
#include "PlayerMovement.h"
#include "Player.h"
#include "PlayerSpellbook.h"
#include "PlayerStats.h"
#include "SessionBase.h"
#include "VisibilitySet.h"
#include "ZoneTransferQueue.h"
#include "ZoneTriggerMgr.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class InstanceSight;
struct ChatSpeaker;
class SocialMgr;

struct WorldDeparture
{
    uint32 MapId = 0;
    uint64 WorldGuid = 0;
};

struct GameEffectChange
{
    int32 InternalId = 0;
    uint32 EffectNameId = 0;
    std::optional<std::string> Data;
};

enum class SpellbookChange : uint8
{
    Learned,
    AlreadyKnown,
    Unlearned,
    NotKnown,
    NoSuchSpell,
    NotInWorld
};

class GameSession : public SessionBase
{
public:
    GameSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context);

    static void SetRealmId(uint32 realmId) noexcept;
    static uint32 GetRealmId() noexcept;
    using OnlookerSource = std::function<std::vector<std::shared_ptr<GameSession>>()>;

    static void SetTransferEndpoint(std::string address, uint16 port);
    static void SetOnlookerSource(OnlookerSource source);

    uint64 GetAccountId() const noexcept { return _accountId.load(std::memory_order_relaxed); }
    void SetAccountId(uint64 accountId) noexcept { _accountId.store(accountId, std::memory_order_relaxed); }

    uint64 GetCharacterId() const noexcept { return _characterId.load(std::memory_order_relaxed); }
    void SetCharacterId(uint64 characterId) noexcept { _characterId.store(characterId, std::memory_order_relaxed); }

    bool IsAttached() const noexcept { return _attached.load(std::memory_order_relaxed); }
    bool IsLinkDead() const noexcept { return _linkDead.load(std::memory_order_relaxed); }
    bool TakeLinkDeadStart() noexcept { return _linkDeadStartPending.exchange(false, std::memory_order_relaxed); }
    std::chrono::duration<float> GetLinkDeadRemaining(std::chrono::steady_clock::time_point now) const;
    bool CanResume(std::chrono::steady_clock::time_point now) const;
    void SetIntentionalDisconnect() noexcept { _intentionalDisconnect.store(true, std::memory_order_relaxed); }

    std::string GetCharacterName() const;
    void SetCharacterName(std::string name);
    std::string const& GetZonePath() const noexcept { return _zonePath; }
    std::string const& GetZoneDisplay() const noexcept { return _zoneDisplay; }

    std::size_t DrainQueue(std::size_t limit = MaxQueuedMessages);
    void WorldUpdate(std::chrono::steady_clock::time_point now);

    void HandleAttach(GameMessages::Attach& message);
    void HandleClientZoned(GameMessages::ClientZoned& message);
    void HandleClientMove(GameMessages::ClientMove& message);
    void HandleClientMoveState(GameMessages::ClientMoveState& message);
    void HandleJump(GameMessages::Jump& message);
    void HandleRequestRadialChat(GameMessages::RequestRadialChat& message);
    void HandleRequestRadialQuickChat(GameMessages::RequestRadialQuickChat& message);
    void HandleRequestRadialQuickChatExt(GameMessages::RequestRadialQuickChatExt& message);
    void HandleCoreEmote(GameMessages::CoreEmote& message);
    void HandleCorePiiRadialMenuEmote(GameMessages::CorePiiRadialMenuEmote& message);
    void HandleRequestPiiRadialMenuPlayEmote(GameMessages::RequestPiiRadialMenuPlayEmote& message);
    void HandleQueryLogout(GameMessages::QueryLogout& message);
    void HandleClientDisconnect(GameMessages::ClientDisconnect& message);
    void HandleNotAfk(GameMessages::NotAfk& message);
    void HandleZoneTransferAck(GameMessages::ZoneTransferAck& message);
    void HandleZoneTransferNack(GameMessages::ZoneTransferNack& message);
    void HandleRetryTeleport(GameMessages::RetryTeleport& message);
    bool RequestZoneTransfer(ZoneTransfer transfer, std::string& problem);
    bool IsTransferring() const noexcept { return _transfers.Busy(); }
    void HandlePostZoneEventFromClient(GameMessages::PostZoneEventFromClient& message);
    void ArriveInVolumes();
    void CheckVolumes();
    void LeaveWorld();
    std::optional<uint32> GetMapId() const noexcept { return _mapId; }
    uint64 GetWorldGuid() const noexcept { return _worldGuid; }
    bool IsShown() const noexcept { return _mapId.has_value() && !_publicObject.empty(); }
    bool TakeArrival() noexcept;
    std::optional<WorldDeparture> TakeDeparture() noexcept;
    void ShowPlayer(GameSession const& other);
    VisibilityChanges UpdateSight(Map const& map, InstanceSight const& sight, std::map<uint64, GameSession const*> const& wizards);
    bool Sees(uint64 id) const { return _sight.IsVisible(id); }
    void ForgetSight(uint64 id);
    static VisibilityRange SightRangeOf(Map const& map);
    void ShowZombiePlayer(GameSession const& other);
    void HidePlayer(uint64 worldGuid);
    void ShowWizBangOf(uint64 worldGuid, uint32 wizBangId);
    std::optional<uint32> TakeWizBangChange() noexcept { return std::exchange(_pendingWizBang, std::nullopt); }
    std::optional<uint8> TakeJump() noexcept;
    void ShowStateOf(uint64 worldGuid, uint32 state);
    std::vector<Speech> TakeSpeech();
    void HearSpeech(ChatSpeaker const& speaker, Speech const& speech);
    void HearCustomEmote(ChatSpeaker const& speaker, Speech const& speech);
    std::string const& GetChatName() const noexcept { return _chatName; }
    uint8 GetChatFilter() const noexcept { return _chatFilter; }
    uint8 GetSecurityLevel() const noexcept { return _securityLevel.load(std::memory_order_relaxed); }
    void SetSecurityLevel(uint8 level) noexcept { _securityLevel.store(level, std::memory_order_relaxed); }
    void SetChatMode(uint8 mode) noexcept { _chatMode = mode; }
    void ApplyMute(uint64 until);
    void ClearMute();
    MovementUpdate TakeMovementUpdate(uint32 idleFlushes);
    void ShowMovementOf(GameSession const& mover, MovementUpdate const& update);
    bool TeleportWithinMap(PlayerPosition const& target, std::vector<std::shared_ptr<GameSession>> const& onlookers, std::string& problem);
    void ShowTeleportOf(GameSession const& mover, PackedMove const& place);
    void SendObjectChanges(MapObjectChanges const& changes);
    PlayerStats const* GetStats() const noexcept { return _player ? &_player->GetStats() : nullptr; }
    Player* GetPlayer() noexcept { return _player ? &*_player : nullptr; }
    bool SetHealth(int32 value);
    bool SetMana(int32 value);
    bool SetGold(int64 value);
    int64 ModifyGold(int64 amount);
    bool SetPotionCapacity(uint32 capacity);
    bool SetPowerPip(float value);
    bool SetShadowPipRating(float value);
    void SendElixirStateChange(uint64 parentId, uint8 effectEnabled);
    PlayerMovement const& GetMovement() const noexcept { return _movement; }
    PlayerSpellbook const* GetSpellbook() const noexcept { return _spellbook ? &*_spellbook : nullptr; }
    SpellbookChange LearnSpell(uint32 spellId);
    SpellbookChange UnlearnSpell(uint32 spellId);
    PlayerBackpack const* GetBackpack() const noexcept { return _backpack ? &*_backpack : nullptr; }
    uint32 GetBackpackCapacity() const;
    BackpackAdd AddItem(ItemTemplateRecord const& itemTemplate, uint32 quantity);
    std::optional<CharacterItem> RemoveItem(uint64 itemGuid);
    BackpackTrashResult TrashItem(uint64 itemGuid, uint32 templateId);
    bool ShowLoot(std::vector<LootItem> const& items);
    void HandleTrashInventoryItem(GameMessages::TrashInventoryItem& message);
    std::optional<int32> AddGameEffect(PropertyObjectPtr effect, std::string& problem);
    std::optional<ActiveGameEffect> RemoveGameEffect(int32 internalId);
    GameEffectHolder const& GetGameEffects() const noexcept { return _effects; }
    std::vector<GameEffectChange> TakeGameEffectChanges() noexcept { return std::exchange(_effectChanges, {}); }
    void ShowGameEffectOf(uint64 worldGuid, GameEffectChange const& change);

    void HandleGetTimedAccessPasses(GameMessages::GetTimedAccessPasses& message);
    void HandleGetSubscriberOnlyItems(GameMessages::GetSubscriberOnlyItems& message);
    void HandleCrownBalance(GameMessages::CrownBalance& message);
    void HandleDoneShopping(GameMessages::DoneShopping& message);
    void HandleLogClientResolution(GameMessages::LogClientResolution& message);
    void HandleLogPatchClientPatchTime(GameMessages::LogPatchClientPatchTime& message);
    void HandleQuestFinderOption(GameMessages::QuestFinderOption& message);
    void HandleUsePotion(GameMessages::UsePotion& message);
    void SendBadges();
    void HandlePlayerWizBang(GameMessages::PlayerWizBang& message);

    void HandleBuddyRequestList(GameMessages::BuddyRequestList& message);
    void HandleBuddyRequestAdd(GameMessages::BuddyRequestAdd& message);
    void HandleBuddyRequestAccept(GameMessages::BuddyRequestAccept& message);
    void HandleBuddyRequestDeny(GameMessages::BuddyRequestDeny& message);
    void HandleBuddyRequestDrop(GameMessages::BuddyRequestDrop& message);
    void HandleBestFriend(GameMessages::BestFriend& message);
    void HandleRequestMaxFriends(GameMessages::RequestMaxFriends& message);
    void HandleIgnoreAdd(GameMessages::IgnoreAdd& message);
    void HandleIgnoreDrop(GameMessages::IgnoreDrop& message);

    void HandleCombatMove(GameMessages::CombatMove& message);
    void HandleCombatDraw(GameMessages::CombatDraw& message);
    void HandleCombatAFK(GameMessages::CombatAFK& message);
    void HandleCombatVictory(GameMessages::CombatVictory& message);
    void HandlePetWillCast(GameMessages::PetWillCast& message);
    void HandleDismissSummon(GameMessages::DismissSummon& message);
    void HandleCombatCheat(GameMessages::CombatCheat& message);

    void ProcessCallbacks();

    uint64 GetUnhandledMessageCount() const noexcept { return _unhandled.load(std::memory_order_relaxed); }

protected:
    void OnMessage(DmlMessageData& message) override;
    void OnSessionClosed() override;

private:
    friend class World;
    friend class SocialMgr;
    friend struct GameSessionLifecycleTestAccess;
    friend struct GameSessionInventoryTestAccess;
    friend struct SocialMgrTestAccess;
    friend struct ChatHandlerTestAccess;

    std::shared_ptr<GameSession> SharedSelf();
    SQLOperation::CompletionHandler MakeCompletionHandler();
    void CheckTransferPassKey(LoginKeyClaim claim, std::string passKey, int64 now);
    void ConsumeKey(LoginKeyClaim claim, int64 now);
    void Diagnose(LoginKeyClaim claim, int64 now);
    void AcceptAttach(LoginKeyClaim const& claim);
    void SendCustomEmotes();
    void RefuseAttach(LoginKeyClaim const& claim, LoginKeyVerdict verdict);
    void LoadAccount(LoginKeyClaim const& claim);
    void LoadCharacter(LoginKeyClaim const& claim);
    void LoadStats(LoginKeyClaim const& claim, CharacterSummary character);
    void LoadSpells(LoginKeyClaim const& claim, CharacterSummary character, std::optional<CharacterStats> stored);
    void LoadInventory(LoginKeyClaim const& claim, CharacterSummary character, std::optional<CharacterStats> stored, std::vector<CharacterSpell> spells);
    void EnterWorld(LoginKeyClaim const& claim, CharacterSummary const& character, std::optional<CharacterStats> const& stored, std::vector<CharacterSpell> const& spells,
        std::vector<CharacterItem> const& items);
    void SaveStats();
    void SaveStatsIfDirty();
    void SaveSpell(CharacterSpell const& spell);
    void SaveNewItem(CharacterItem const& item);
    void DeleteStoredItem(uint64 itemGuid);
    void SendItemAdded(ItemTemplateRecord const& itemTemplate, CharacterItem const& item);
    void SendItemRemoved(uint64 itemGuid);
    void SavePosition(PlayerPosition const& position);
    void SendHealthUpdate(uint8 displayDiff);
    void SendManaUpdate(uint8 displayDiff);
    void SendGoldUpdate();
    void SendPotionUpdate();
    void RefuseEntry(LoginKeyClaim const& claim, std::string const& reason);
    bool CanSpeak(std::string_view what) const;
    bool RejectClosedChat();
    bool RejectMutedSpeech();
    void SendMuteNotice();
    void QueueSpeech(Speech speech, std::string_view what);
    void QueueEmote(std::string_view name, uint8 excludeOriginator, std::string_view what);
    void MarkOffline();
    void TransferWorldStateTo(GameSession& replacement);
    bool TakeCommandLine(std::string_view packed);
    std::vector<std::string> PostZoneEvent(std::string_view event, std::chrono::steady_clock::time_point now);
    void FollowReloadedVolumes();
    void WalkThroughDoor(std::vector<std::string> const& doors);
    void ShowGameEffectsOf(GameSession const& other);

    AsyncCallbackProcessor<CountedCallback> _countedCallbacks;
    AsyncCallbackProcessor<QueryCallback> _queryCallbacks;
    AsyncCallbackProcessor<TransactionCallback> _transactionCallbacks;
    ZoneTransferQueue _transfers;
    std::optional<GameMessages::ServerTransfer> _lastTransfer;
    std::shared_ptr<ZoneTriggerData const> _volumeData;
    VisibilitySet _sight;
    std::vector<VolumePresence> _volumePresence;
    std::atomic<uint64> _accountId{ 0 };
    std::atomic<uint64> _characterId{ 0 };
    std::atomic<uint64> _unhandled{ 0 };
    GameSessionWorld* _world = nullptr;
    std::atomic<bool> _attached{ false };
    std::atomic<bool> _attaching{ false };
    std::atomic<bool> _intentionalDisconnect{ false };
    std::atomic<bool> _superseded{ false };
    std::atomic<bool> _inWorld{ false };
    std::atomic<bool> _linkDead{ false };
    std::atomic<bool> _linkDeadStartPending{ false };
    std::atomic<int64> _socketLostAtNanoseconds{ 0 };
    std::atomic<uint8> _securityLevel{ 0 };
    uint8 _chatMode = 0;
    uint64 _muteUntil = 0;
    std::optional<uint32> _accountPermissions;
    std::chrono::steady_clock::time_point const _connectedAt = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point _afkStarted;
    bool _afkTimerStarted = false;
    bool _afkWarned = false;
    bool _linkDeadNotified = false;
    uint8 _reattach = 0;
    std::optional<uint32> _mapId;
    std::string _zonePath;
    uint64 _worldGuid = 0;
    uint32 _wizBangId = 0;
    std::optional<uint32> _pendingWizBang;
    std::optional<Player> _player;
    uint64 _statsRevision = 0;
    std::optional<PlayerSpellbook> _spellbook;
    std::optional<PlayerBackpack> _backpack;
    int64 _itemsAllowed = 0;
    GameEffectHolder _effects;
    std::vector<GameEffectChange> _effectChanges;
    PlayerMovement _movement;
    MovementRelay _relay;
    std::vector<uint8> _publicObject;
    bool _arrived = false;
    std::optional<WorldDeparture> _departure;
    std::optional<uint8> _jump;
    std::vector<Speech> _speech;
    std::string _chatName;
    uint8 _chatFilter = 0;
    bool _hideNextChatEmote = false;
    uint16 _mobileId = 0;
    uint64 _characterRevision = 0;
    mutable std::mutex _nameMutex;
    std::string _characterName;
    std::string _zoneDisplay;
};

#endif
