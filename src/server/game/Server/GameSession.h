/*
 * Project Ambrose by Imjustchico
 * Declares one connected game client, its queued world-thread state, handlers, and outbound updates to other wizards.
 */

#ifndef AMBROSE_GAMESESSION_H
#define AMBROSE_GAMESESSION_H

#include "AsyncCallbackProcessor.h"
#include "CharacterSpell.h"
#include "CharacterStats.h"
#include "CharacterSummary.h"
#include "ChatMgr.h"
#include "GameMessages.h"
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
#include <utility>
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
    void LeaveWorld();
    std::optional<uint32> GetMapId() const noexcept { return _mapId; }
    uint64 GetWorldGuid() const noexcept { return _worldGuid; }
    bool IsShown() const noexcept { return _mapId.has_value() && !_publicObject.empty(); }
    bool TakeArrival() noexcept;
    std::optional<WorldDeparture> TakeDeparture() noexcept;
    void ShowPlayer(GameSession const& other);
    void HidePlayer(uint64 worldGuid);
    void ShowWizBangOf(uint64 worldGuid, uint32 wizBangId);
    std::optional<uint32> TakeWizBangChange() noexcept { return std::exchange(_pendingWizBang, std::nullopt); }
    std::optional<uint8> TakeJump() noexcept;
    void ShowStateOf(uint64 worldGuid, uint32 state);
    std::vector<Speech> TakeSpeech();
    void HearSpeech(ChatSpeaker const& speaker, Speech const& speech);
    std::string const& GetChatName() const noexcept { return _chatName; }
    uint8 GetChatFilter() const noexcept { return _chatFilter; }
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
    void HandlePlayerWizBang(GameMessages::PlayerWizBang& message);

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

    AsyncCallbackProcessor<CountedCallback> _countedCallbacks;
    AsyncCallbackProcessor<QueryCallback> _queryCallbacks;
    std::atomic<uint64> _accountId{ 0 };
    std::atomic<uint64> _characterId{ 0 };
    std::atomic<uint64> _unhandled{ 0 };
    std::atomic<bool> _attached{ false };
    std::atomic<bool> _attaching{ false };
    std::atomic<uint8> _securityLevel{ 0 };
    std::chrono::steady_clock::time_point const _connectedAt = std::chrono::steady_clock::now();
    std::optional<uint32> _mapId;
    std::string _zonePath;
    uint64 _worldGuid = 0;
    uint32 _wizBangId = 0;
    std::optional<uint32> _pendingWizBang;
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
    uint16 _mobileId = 0;
    uint64 _characterRevision = 0;
    mutable std::mutex _nameMutex;
    std::string _characterName;
};

#endif
