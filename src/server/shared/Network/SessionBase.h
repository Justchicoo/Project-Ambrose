/*
 * Project Ambrose by Imjustchico
 * A client session over one socket: sends SessionOffer at once, waits for a matching SessionAccept, answers and sends keepalives, hands DML messages to the app, and tracks its status, protocol strikes and queued inbound work.
 */

#ifndef AMBROSE_SESSIONBASE_H
#define AMBROSE_SESSIONBASE_H

#include "ControlMessages.h"
#include "SessionContext.h"
#include "SessionStatus.h"
#include "Socket.h"
#include "TokenBucket.h"

#include <asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <utility>

enum class SessionState : uint8
{
    Offered,
    Accepted
};

class SessionBase : public Socket
{
public:
    static constexpr std::size_t MaxPendingFrames = 256;
    static constexpr std::size_t MaxPendingBytes = std::size_t{ 1 } << 20;
    static constexpr std::chrono::seconds DisabledKeepAliveRecheck{ 1 };
    static constexpr std::size_t MaxQueuedMessages = 4096;
    static constexpr std::size_t MaxQueuedBytes = std::size_t{ 4 } << 20;

    SessionBase(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context);
    ~SessionBase() override;

    uint16 GetSessionId() const noexcept { return _sessionId; }
    SessionState GetState() const noexcept { return _state.load(std::memory_order_relaxed); }
    SessionTimestamp GetOfferTime() const noexcept;
    std::chrono::milliseconds GetAcceptRoundTrip() const noexcept { return std::chrono::milliseconds(_acceptRoundTripMs.load(std::memory_order_relaxed)); }
    std::chrono::milliseconds GetKeepAliveRoundTrip() const noexcept { return std::chrono::milliseconds(_keepAliveRoundTripMs.load(std::memory_order_relaxed)); }
    uint64 GetKeepAlivesSent() const noexcept { return _keepAlivesSent.load(std::memory_order_relaxed); }
    uint64 GetKeepAlivesAnswered() const noexcept { return _keepAlivesAnswered.load(std::memory_order_relaxed); }

    SessionStatus GetStatus() const noexcept { return _status.load(std::memory_order_relaxed); }
    void SetStatus(SessionStatus status) noexcept { _status.store(status, std::memory_order_relaxed); }
    uint32 GetStrikes() const noexcept { return _strikes.load(std::memory_order_relaxed); }
    bool IsKicked() const noexcept { return _kicked.load(std::memory_order_relaxed); }
    bool AddStrike(std::string_view reason);
    void Kick(std::string_view reason);
    bool AllowDropLog();
    bool QueueInbound(std::function<void()> work, std::size_t bytes = 0);
    std::size_t ProcessQueuedMessages(std::size_t limit = MaxQueuedMessages);
    std::size_t GetQueuedMessageCount() const;

protected:
    virtual void OnAccepted();
    virtual void OnMessage(DmlMessageData& message) = 0;
    virtual void OnSessionClosed();

    void SendDml(uint8 serviceId, uint8 order, std::span<uint8 const> body);
    void SetKeepAliveTimeoutSuspended(bool suspended) noexcept { _keepAliveTimeoutSuspended = suspended; }
    SessionContext& GetContext() const noexcept { return *_context; }

private:
    void OnStart() final;
    void OnFrame(Frame& frame) final;
    void OnClose() final;

    void HandleControl(Frame& frame);
    void HandleAccept(Frame const& frame);
    void HandleClientKeepAlive(Frame const& frame);
    void HandleKeepAliveResponse();
    void DispatchDml(Frame& frame);
    void StartAcceptTimer();
    void ScheduleKeepAlive(std::chrono::milliseconds delay);
    void SendKeepAlive();
    void CloseForProtocol(std::string const& reason);
    std::shared_ptr<SessionBase> Self();

    std::shared_ptr<SessionContext> _context;
    std::mutex _dropMutex;
    TokenBucket _dropBudget;
    uint16 _sessionId = 0;
    std::atomic<SessionStatus> _status{ SessionStatus::Connected };
    std::atomic<uint32> _strikes{ 0 };
    std::atomic<bool> _kicked{ false };
    mutable std::mutex _inboundMutex;
    std::deque<std::pair<std::function<void()>, std::size_t>> _inbound;
    std::size_t _inboundBytes = 0;
    std::atomic<SessionState> _state{ SessionState::Offered };
    std::atomic<uint64> _offerSeconds{ 0 };
    std::atomic<uint32> _offerMilliseconds{ 0 };
    std::atomic<int64> _acceptRoundTripMs{ -1 };
    std::atomic<int64> _keepAliveRoundTripMs{ -1 };
    std::atomic<uint64> _keepAlivesSent{ 0 };
    std::atomic<uint64> _keepAlivesAnswered{ 0 };
    asio::steady_timer _acceptTimer;
    asio::steady_timer _keepAliveTimer;
    asio::steady_timer _keepAliveResponseTimer;
    std::chrono::steady_clock::time_point _offerSentAt;
    std::chrono::steady_clock::time_point _acceptedAt;
    std::chrono::steady_clock::time_point _keepAliveSentAt;
    std::chrono::steady_clock::time_point _lastInbound;
    std::deque<Frame> _pendingFrames;
    std::size_t _pendingBytes = 0;
    uint32 _keepAliveSequence = 0;
    bool _awaitingKeepAlive = false;
    bool _keepAliveTimeoutSuspended = false;
    bool _idReleased = false;
};

#endif
