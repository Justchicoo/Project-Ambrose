/*
 * Project Ambrose by Imjustchico
 * One connected game client, on the network side a session like the login server's and on the game side the thing the world owns: what arrives is queued by the network thread that read it and run later by the world thread that owns the game state, so a handler never touches the world from two threads at once, and the account and character it belongs to are carried here for every later system to read. What arrives goes through the game server's own message table, which answers MSG_ATTACH and nothing else yet, so a message with no rule is counted and reported rather than acted on. An attach is judged by the handoff key it carries, which is spent against the login database off the network thread, so a client is only ever let in on a key that this attach won.
 */

#ifndef AMBROSE_GAMESESSION_H
#define AMBROSE_GAMESESSION_H

#include "AsyncCallbackProcessor.h"
#include "GameMessages.h"
#include "LoginKeyValidator.h"
#include "SessionBase.h"

#include <atomic>
#include <memory>
#include <string>

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

    std::size_t DrainQueue(std::size_t limit = MaxQueuedMessages);

    void HandleAttach(GameMessages::Attach& message);

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

    AsyncCallbackProcessor<CountedCallback> _countedCallbacks;
    AsyncCallbackProcessor<QueryCallback> _queryCallbacks;
    std::atomic<uint64> _accountId{ 0 };
    std::atomic<uint64> _characterId{ 0 };
    std::atomic<uint64> _unhandled{ 0 };
    std::atomic<bool> _attached{ false };
    std::atomic<bool> _attaching{ false };
};

#endif
