/*
 * Project Ambrose by Imjustchico
 * A connected game client and the world-thread-owned wizard behind it: attach spends a one-use handoff key, loads and checks the character, then gives the client its object; movement, spellbook and stats stay with the world thread and the final position is saved on a clean exit, disconnect expiry or server stop. Intentional exits mark the character offline immediately, link-dead sockets retain the wizard and online claim for a live-configured grace period, and a replacement attach can take over the existing world placement without creating a duplicate. What its wizard says and the emotes it plays are kept until the world's next tick shows them to the wizards around it, which hear them under the name the client's name codec packs for it and the chat level its permissions give it, and a command line its account may run is run at the account's security level, with the reply sent back to its own chat window.
 */

#ifndef AMBROSE_GAMESESSION_H
#define AMBROSE_GAMESESSION_H

#include "AsyncCallbackProcessor.h"
#include "CharacterSpell.h"
#include "CharacterStats.h"
#include "CharacterSummary.h"
#include "ChatMgr.h"
#include "GameMessages.h"
#include "GameSessionWorld.h"
#include "LoginKeyValidator.h"
#include "MapObjectSpawner.h"
#include "MovementRelay.h"
#include "PlayerMovement.h"
#include "PlayerSpellbook.h"
#include "PlayerStats.h"
#include "SessionBase.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ChatSpeaker;

struct WorldDeparture
{
    uint32 MapId = 0;
    uint64 WorldGuid = 0;
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
    void HandleQueryLogout(GameMessages::QueryLogout& message);
    void HandleClientDisconnect(GameMessages::ClientDisconnect& message);
    void HandleNotAfk(GameMessages::NotAfk& message);
    void LeaveWorld();
    std::optional<uint32> GetMapId() const noexcept { return _mapId; }
    uint64 GetWorldGuid() const noexcept { return _worldGuid; }
    bool IsShown() const noexcept { return _mapId.has_value() && !_publicObject.empty(); }
    bool TakeArrival() noexcept;
    std::optional<WorldDeparture> TakeDeparture() noexcept;
    void ShowPlayer(GameSession const& other);
    void ShowZombiePlayer(GameSession const& other);
    void HidePlayer(uint64 worldGuid);
    std::optional<uint8> TakeJump() noexcept;
    void ShowStateOf(uint64 worldGuid, uint32 state);
    std::vector<Speech> TakeSpeech();
    void HearSpeech(ChatSpeaker const& speaker, Speech const& speech);
    std::string const& GetChatName() const noexcept { return _chatName; }
    uint8 GetChatFilter() const noexcept { return _chatFilter; }
    uint8 GetSecurityLevel() const noexcept { return _securityLevel.load(std::memory_order_relaxed); }
    void SetSecurityLevel(uint8 level) noexcept { _securityLevel.store(level, std::memory_order_relaxed); }
    MovementUpdate TakeMovementUpdate(uint32 idleFlushes);
    void ShowMovementOf(GameSession const& mover, MovementUpdate const& update);
    void SendObjectChanges(MapObjectChanges const& changes);
    PlayerStats const* GetStats() const noexcept { return _stats ? &*_stats : nullptr; }
    PlayerMovement const& GetMovement() const noexcept { return _movement; }
    PlayerSpellbook const* GetSpellbook() const noexcept { return _spellbook ? &*_spellbook : nullptr; }
    SpellbookChange LearnSpell(uint32 spellId);
    SpellbookChange UnlearnSpell(uint32 spellId);

    void HandleGetTimedAccessPasses(GameMessages::GetTimedAccessPasses& message);
    void HandleGetSubscriberOnlyItems(GameMessages::GetSubscriberOnlyItems& message);
    void HandleCrownBalance(GameMessages::CrownBalance& message);
    void HandleDoneShopping(GameMessages::DoneShopping& message);
    void HandleLogClientResolution(GameMessages::LogClientResolution& message);
    void HandleLogPatchClientPatchTime(GameMessages::LogPatchClientPatchTime& message);
    void HandleQuestFinderOption(GameMessages::QuestFinderOption& message);
    void SendBadges();

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
    friend struct GameSessionLifecycleTestAccess;

    std::shared_ptr<GameSession> SharedSelf();
    SQLOperation::CompletionHandler MakeCompletionHandler();
    void Diagnose(LoginKeyClaim claim, int64 now);
    void AcceptAttach(LoginKeyClaim const& claim);
    void SendMapObjects(Map const& map);
    void RefuseAttach(LoginKeyClaim const& claim, LoginKeyVerdict verdict);
    void LoadAccount(LoginKeyClaim const& claim);
    void LoadCharacter(LoginKeyClaim const& claim);
    void LoadStats(LoginKeyClaim const& claim, CharacterSummary character);
    void LoadSpells(LoginKeyClaim const& claim, CharacterSummary character, std::optional<CharacterStats> stored);
    void EnterWorld(LoginKeyClaim const& claim, CharacterSummary const& character, std::optional<CharacterStats> const& stored, std::vector<CharacterSpell> const& spells);
    void SaveStats();
    void SaveSpell(CharacterSpell const& spell);
    void SavePosition(PlayerPosition const& position);
    void RefuseEntry(LoginKeyClaim const& claim, std::string const& reason);
    bool CanSpeak(std::string_view what) const;
    void QueueSpeech(Speech speech, std::string_view what);
    void QueueEmote(std::string_view name, uint8 excludeOriginator, std::string_view what);
    void MarkOffline();
    void TransferWorldStateTo(GameSession& replacement);
    bool TakeCommandLine(std::string_view packed);

    AsyncCallbackProcessor<CountedCallback> _countedCallbacks;
    AsyncCallbackProcessor<QueryCallback> _queryCallbacks;
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
    std::chrono::steady_clock::time_point const _connectedAt = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point _afkStarted;
    bool _afkTimerStarted = false;
    bool _afkWarned = false;
    bool _linkDeadNotified = false;
    uint8 _reattach = 0;
    std::optional<uint32> _mapId;
    std::string _zonePath;
    uint64 _worldGuid = 0;
    std::optional<PlayerStats> _stats;
    uint64 _statsRevision = 0;
    std::optional<PlayerSpellbook> _spellbook;
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
};

#endif
