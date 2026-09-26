/*
 * Project Ambrose by Imjustchico
 * One connected game client, on the network side a session like the login server's and on the game side the thing the world owns: what arrives is queued by the network thread that read it and run later by the world thread that owns the game state, so a handler never touches the world from two threads at once, and the account and character it belongs to are carried here for every later system to read. What arrives goes through the game server's own message table, so a message with no rule is counted and reported rather than acted on. An attach is judged by the handoff key it carries, which is spent against the login database off the network thread, so a client is only ever let in on a key that this attach won; the wizard it names is then loaded, checked to belong to that account, placed in its zone's instance on the world thread and handed its own object in MSG_LOGINCOMPLETE, and when the client says it has loaded that zone the wizard is in the world. The wizard's name, as its client shows it, is kept for a command to find the session by, under a lock of its own since a console asks from another thread. The instance and zone it stands in are the world thread's alone, and it leaves them when the session goes. The WIZARD messages a client sends as it enters are answered by the handlers in game/Handlers. Where the wizard stands is kept from the moves its client sends, and written to its character row when it leaves the world, never as it moves. The wizard's stats are read with it, from its character_stats row, and kept on the world thread while it plays; they are written when they change and when it leaves, never on a timer, each write queued so a closing session need not wait on it and carrying the next revision of the wizard's row, so writes that land out of order leave the newest. Its spellbook is read with it too, from its character_spell rows, and the player object carries a tracker for each spell it knows; LearnSpell and UnlearnSpell change it on the world thread, for a command, a quest or a trainer alike, each change written at once under the spellbook's next revision and told to the client with MSG_ADDSPELLTOBOOK or MSG_REMOVESPELLFROMBOOK. The world ticks each session, and one that has neither attached nor begun to within Attach.Timeout of connecting is closed, so a socket that never says who it is cannot hold a slot.
 */

#ifndef AMBROSE_GAMESESSION_H
#define AMBROSE_GAMESESSION_H

#include "AsyncCallbackProcessor.h"
#include "CharacterSpell.h"
#include "CharacterStats.h"
#include "CharacterSummary.h"
#include "GameMessages.h"
#include "LoginKeyValidator.h"
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
#include <vector>

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
    void LeaveWorld();
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
    std::optional<PlayerStats> _stats;
    uint64 _statsRevision = 0;
    std::optional<PlayerSpellbook> _spellbook;
    PlayerMovement _movement;
    uint64 _characterRevision = 0;
    mutable std::mutex _nameMutex;
    std::string _characterName;
};

#endif
