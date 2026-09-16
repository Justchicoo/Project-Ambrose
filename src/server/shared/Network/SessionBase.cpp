/*
 * Project Ambrose by Imjustchico
 * Runs the KI session handshake and keepalives on the socket's network thread, queues DML frames that arrive before SessionAccept, closes on mismatched ids, silence or too many strikes, holds inbound work for the app to drain, answers pings within the ping budget, and sends server messages and forced disconnects.
 */

#include "SessionBase.h"
#include "FrameWriter.h"
#include "Log.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <iterator>

namespace
{
    constexpr char const* SessionLog = "network.session";

    uint16 MillisecondsIntoSecond()
    {
        auto const now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<uint16>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count() % 1000);
    }

    int64 ElapsedMs(std::chrono::steady_clock::time_point since)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - since).count();
    }
}

SessionBase::SessionBase(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context)
    : Socket(std::move(socket), limits), _context(std::move(context)), _dropBudget(_context->GetSettings().DroppedMessageBurst, _context->GetSettings().DroppedMessagesPerSecond), _pingBudget(_context->GetSettings().PingBurst, _context->GetSettings().PingsPerSecond), _acceptTimer(GetExecutor()), _keepAliveTimer(GetExecutor()), _keepAliveResponseTimer(GetExecutor())
{
    if (std::optional<uint16> const id = _context->AllocateId())
        _sessionId = *id;
}

SessionBase::~SessionBase()
{
    if (!_idReleased && _sessionId != 0)
        _context->ReleaseId(_sessionId);
}

SessionTimestamp SessionBase::GetOfferTime() const noexcept
{
    uint64 const seconds = _offerSeconds.load(std::memory_order_relaxed);
    SessionTimestamp time;
    time.TimeHigh = static_cast<int32>(static_cast<uint32>(seconds >> 32));
    time.TimeLow = static_cast<int32>(static_cast<uint32>(seconds & 0xFFFFFFFFu));
    time.Milliseconds = _offerMilliseconds.load(std::memory_order_relaxed);
    return time;
}

void SessionBase::OnAccepted()
{
}

void SessionBase::OnSessionClosed()
{
}

bool SessionBase::AddStrike(std::string_view reason)
{
    uint32 const strikes = _strikes.fetch_add(1, std::memory_order_relaxed) + 1;
    uint32 const limit = _context->GetSettings().MaxStrikes;
    if (strikes < limit)
    {
        LOG_DEBUG(SessionLog, "Session {} strike {} of {}: {}", _sessionId, strikes, limit, reason);
        return true;
    }
    Kick(fmt::format("strike {} of {}, the last for {}", strikes, limit, reason));
    return false;
}

bool SessionBase::AllowDropLog()
{
    std::lock_guard const lock(_budgetMutex);
    return _dropBudget.TryConsume();
}

void SessionBase::Kick(std::string_view reason)
{
    if (_kicked.exchange(true, std::memory_order_relaxed))
        return;
    LOG_WARN(SessionLog, "Closing session {} from {}:{}: {}", _sessionId, GetRemoteAddress().to_string(), GetRemotePort(), reason);
    CloseSocket();
}

bool SessionBase::QueueInbound(std::function<void()> work, std::size_t bytes)
{
    std::size_t waiting = 0;
    std::size_t waitingBytes = 0;
    {
        std::lock_guard const lock(_inboundMutex);
        waiting = _inbound.size();
        waitingBytes = _inboundBytes;
        if (waiting < MaxQueuedMessages && waitingBytes + bytes <= MaxQueuedBytes && !IsKicked())
        {
            _inbound.emplace_back(std::move(work), bytes);
            _inboundBytes += bytes;
            return true;
        }
    }
    if (!IsKicked())
        Kick(fmt::format("{} messages of {} bytes were already waiting to be processed", waiting, waitingBytes));
    return false;
}

std::size_t SessionBase::ProcessQueuedMessages(std::size_t limit)
{
    std::deque<std::pair<std::function<void()>, std::size_t>> batch;
    {
        std::lock_guard const lock(_inboundMutex);
        std::size_t const count = std::min(limit, _inbound.size());
        auto const end = _inbound.begin() + static_cast<std::ptrdiff_t>(count);
        batch.assign(std::make_move_iterator(_inbound.begin()), std::make_move_iterator(end));
        _inbound.erase(_inbound.begin(), end);
        for (auto const& entry : batch)
            _inboundBytes -= entry.second;
    }
    std::size_t processed = 0;
    for (auto& entry : batch)
    {
        if (!IsOpen() || IsKicked())
            break;
        entry.first();
        ++processed;
    }
    return processed;
}

std::size_t SessionBase::GetQueuedMessageCount() const
{
    std::lock_guard const lock(_inboundMutex);
    return _inbound.size();
}

bool SessionBase::SendServerMessage(std::u16string text, bool modal)
{
    SystemMessages::ServerMessage message;
    message.Modal = modal ? 1 : 0;
    message.Message = std::move(text);
    return SendDmlMessage(message);
}

void SessionBase::KickPlayer(uint32 type, std::string_view reason)
{
    if (!IsOpen() || _kicked.exchange(true, std::memory_order_relaxed))
        return;
    LOG_INFO(SessionLog, "Kicking session {} from {}:{} with disconnect type {}: {}", _sessionId, GetRemoteAddress().to_string(), GetRemotePort(), type, Ambrose::ForLog(reason, 256));
    try
    {
        SystemMessages::ForceDisconnect message;
        message.Type = type;
        message.TimeStamp = SystemMessages::FormatTimeStamp(std::chrono::system_clock::now());
        message.Message = std::string(Ambrose::TruncateUtf8(reason, MaxKickReasonBytes));
        EncodeAndQueue(message);
    }
    catch (std::exception const& failure)
    {
        ReportSendFailure(SystemMessages::ForceDisconnect::Tag, failure.what());
    }
    DelayedCloseSocket();
}

void SessionBase::HandlePing(SystemMessages::Ping&)
{
    bool allowed = false;
    {
        std::lock_guard const lock(_budgetMutex);
        allowed = _pingBudget.TryConsume();
    }
    if (!allowed)
    {
        AddStrike("MSG_PING sent faster than the session's ping budget allows");
        return;
    }
    SendDmlMessage(SystemMessages::PingRsp{});
}

void SessionBase::ReportSendFailure(std::string_view tag, std::string_view reason) const
{
    LOG_ERROR(SessionLog, "Could not send {} to session {}: {}", tag, _sessionId, reason);
}

void SessionBase::SendDml(uint8 serviceId, uint8 order, std::span<uint8 const> body)
{
    ByteBuffer frame;
    FrameWriter::WriteDml(frame, serviceId, order, body, GetLongFrameLength());
    QueueFrame(std::move(frame));
}

void SessionBase::OnStart()
{
    if (_sessionId == 0)
    {
        LOG_WARN(SessionLog, "Refusing {}:{}: every session id is in use", GetRemoteAddress().to_string(), GetRemotePort());
        CloseNow();
        return;
    }

    SessionOffer offer;
    offer.SessionId = _sessionId;
    offer.Time = SessionTimestamp::FromTimePoint(std::chrono::system_clock::now());
    _offerSeconds.store(offer.Time.GetSeconds(), std::memory_order_relaxed);
    _offerMilliseconds.store(offer.Time.Milliseconds, std::memory_order_relaxed);
    ByteBuffer frame;
    ControlMessages::WriteFrame(frame, offer);
    QueueFrame(std::move(frame));
    _offerSentAt = std::chrono::steady_clock::now();
    _lastInbound = _offerSentAt;
    LOG_INFO(SessionLog, "Session {} offered to {}:{}", _sessionId, GetRemoteAddress().to_string(), GetRemotePort());
    StartAcceptTimer();
}

void SessionBase::OnFrame(Frame& frame)
{
    if (IsKicked())
        return;
    _lastInbound = std::chrono::steady_clock::now();
    if (frame.IsControl)
    {
        HandleControl(frame);
        return;
    }
    if (GetState() == SessionState::Offered)
    {
        if (_pendingFrames.size() >= MaxPendingFrames || _pendingBytes + frame.Payload.size() > MaxPendingBytes)
        {
            CloseForProtocol(fmt::format("more than {} frames or {} bytes arrived before SessionAccept", MaxPendingFrames, MaxPendingBytes));
            return;
        }
        _pendingBytes += frame.Payload.size();
        _pendingFrames.push_back(std::move(frame));
        return;
    }
    DispatchDml(frame);
}

void SessionBase::OnClose()
{
    _acceptTimer.cancel();
    _keepAliveTimer.cancel();
    _keepAliveResponseTimer.cancel();
    _pendingFrames.clear();
    _pendingBytes = 0;
    {
        std::lock_guard const lock(_inboundMutex);
        _inbound.clear();
        _inboundBytes = 0;
    }
    if (_sessionId != 0 && !_idReleased)
    {
        _idReleased = true;
        _context->ReleaseId(_sessionId);
    }
    LOG_DEBUG(SessionLog, "Session {} closed", _sessionId);
    OnSessionClosed();
}

void SessionBase::HandleControl(Frame& frame)
{
    std::optional<ControlOpcode> const opcode = ControlMessages::GetOpcode(frame);
    if (!opcode)
    {
        LOG_DEBUG(SessionLog, "Session {} ignored control opcode {}", _sessionId, frame.Opcode);
        return;
    }
    switch (*opcode)
    {
        case ControlOpcode::SessionAccept:
            HandleAccept(frame);
            break;
        case ControlOpcode::KeepAlive:
            HandleClientKeepAlive(frame);
            break;
        case ControlOpcode::KeepAliveRsp:
            HandleKeepAliveResponse();
            break;
        case ControlOpcode::SessionOffer:
            LOG_DEBUG(SessionLog, "Session {} ignored a SessionOffer from the client", _sessionId);
            break;
    }
}

void SessionBase::HandleAccept(Frame const& frame)
{
    std::optional<SessionAccept> const accept = ControlMessages::DecodeSessionAccept(frame.Payload);
    if (!accept)
    {
        CloseForProtocol(fmt::format("SessionAccept body has {} bytes, expected at least {}", frame.Payload.size(), SessionAccept::BodySize));
        return;
    }
    if (accept->SessionId != _sessionId)
    {
        CloseForProtocol(fmt::format("SessionAccept names session {}", accept->SessionId));
        return;
    }
    if (GetState() != SessionState::Offered)
    {
        LOG_DEBUG(SessionLog, "Session {} ignored a repeated SessionAccept", _sessionId);
        return;
    }

    _acceptTimer.cancel();
    _acceptedAt = std::chrono::steady_clock::now();
    int64 const roundTrip = ElapsedMs(_offerSentAt);
    _acceptRoundTripMs.store(roundTrip, std::memory_order_relaxed);
    _state.store(SessionState::Accepted, std::memory_order_relaxed);
    LOG_INFO(SessionLog, "Session {} accepted by {}:{} after {} ms", _sessionId, GetRemoteAddress().to_string(), GetRemotePort(), roundTrip);

    OnAccepted();
    while (IsOpen() && !IsKicked() && !_pendingFrames.empty())
    {
        Frame pending = std::move(_pendingFrames.front());
        _pendingFrames.pop_front();
        _pendingBytes -= pending.Payload.size();
        DispatchDml(pending);
    }
    if (IsOpen())
        ScheduleKeepAlive(_context->GetSettings().KeepAliveInterval);
}

void SessionBase::HandleClientKeepAlive(Frame const& frame)
{
    std::optional<ClientKeepAlive> const keepAlive = ControlMessages::DecodeClientKeepAlive(frame.Payload);
    if (!keepAlive)
    {
        CloseForProtocol(fmt::format("KeepAlive body has {} bytes, expected {}", frame.Payload.size(), ClientKeepAlive::BodySize));
        return;
    }
    if (keepAlive->SessionId != _sessionId)
    {
        CloseForProtocol(fmt::format("KeepAlive names session {}", keepAlive->SessionId));
        return;
    }

    KeepAliveResponse response;
    response.SessionId = _sessionId;
    response.Milliseconds = MillisecondsIntoSecond();
    response.ElapsedMinutes = keepAlive->ElapsedMinutes;
    ByteBuffer out;
    ControlMessages::WriteFrame(out, response);
    QueueFrame(std::move(out));
    LOG_DEBUG(SessionLog, "Session {} keepalive from the client at {} minutes, answered", _sessionId, keepAlive->ElapsedMinutes);
}

void SessionBase::HandleKeepAliveResponse()
{
    if (!_awaitingKeepAlive)
    {
        LOG_DEBUG(SessionLog, "Session {} got an unrequested KeepAliveRsp", _sessionId);
        return;
    }
    _awaitingKeepAlive = false;
    _keepAliveResponseTimer.cancel();
    int64 const roundTrip = ElapsedMs(_keepAliveSentAt);
    _keepAliveRoundTripMs.store(roundTrip, std::memory_order_relaxed);
    _keepAlivesAnswered.fetch_add(1, std::memory_order_relaxed);
    LOG_DEBUG(SessionLog, "Session {} keepalive answered by the client after {} ms", _sessionId, roundTrip);
}

void SessionBase::DispatchDml(Frame& frame)
{
    std::vector<DmlMessageData> messages;
    FrameError const error = FrameLayout::SplitDmlMessages(frame.Payload, messages);
    if (error != FrameError::None)
    {
        OnProtocolError(error);
        CloseNow();
        return;
    }
    for (DmlMessageData& message : messages)
    {
        OnMessage(message);
        if (!IsOpen() || IsKicked())
            return;
    }
}

void SessionBase::StartAcceptTimer()
{
    std::chrono::milliseconds const timeout = _context->GetSettings().AcceptTimeout;
    _acceptTimer.expires_after(timeout);
    _acceptTimer.async_wait([weak = std::weak_ptr<SessionBase>(Self()), timeout](std::error_code const& error)
    {
        std::shared_ptr<SessionBase> const session = weak.lock();
        if (error || !session || !session->IsOpen() || session->GetState() != SessionState::Offered)
            return;
        LOG_WARN(SessionLog, "Closing session {} from {}:{}: no SessionAccept within {} ms", session->_sessionId, session->GetRemoteAddress().to_string(), session->GetRemotePort(), timeout.count());
        session->CloseNow();
    });
}

void SessionBase::ScheduleKeepAlive(std::chrono::milliseconds delay)
{
    bool const disabled = delay.count() <= 0;
    _keepAliveTimer.expires_after(disabled ? std::chrono::milliseconds(DisabledKeepAliveRecheck) : delay);
    _keepAliveTimer.async_wait([weak = std::weak_ptr<SessionBase>(Self()), disabled](std::error_code const& error)
    {
        std::shared_ptr<SessionBase> const session = weak.lock();
        if (error || !session || !session->IsOpen())
            return;
        std::chrono::milliseconds const interval = session->_context->GetSettings().KeepAliveInterval;
        if (!disabled && interval.count() > 0 && !session->_awaitingKeepAlive)
            session->SendKeepAlive();
        session->ScheduleKeepAlive(interval);
    });
}

void SessionBase::SendKeepAlive()
{
    ServerKeepAlive keepAlive;
    keepAlive.SessionId = _sessionId;
    keepAlive.Milliseconds = static_cast<uint32>(ElapsedMs(_acceptedAt));
    ByteBuffer out;
    ControlMessages::WriteFrame(out, keepAlive);
    QueueFrame(std::move(out));
    _keepAliveSentAt = std::chrono::steady_clock::now();
    _awaitingKeepAlive = true;
    uint32 const sequence = ++_keepAliveSequence;
    _keepAlivesSent.fetch_add(1, std::memory_order_relaxed);
    LOG_DEBUG(SessionLog, "Session {} keepalive sent to the client", _sessionId);

    std::chrono::milliseconds const timeout = _context->GetSettings().KeepAliveTimeout;
    _keepAliveResponseTimer.expires_after(timeout);
    _keepAliveResponseTimer.async_wait([weak = std::weak_ptr<SessionBase>(Self()), timeout, sequence](std::error_code const& error)
    {
        std::shared_ptr<SessionBase> const session = weak.lock();
        if (error || !session || !session->IsOpen() || !session->_awaitingKeepAlive || session->_keepAliveSequence != sequence)
            return;
        session->_awaitingKeepAlive = false;
        if (session->_lastInbound >= session->_keepAliveSentAt || session->_keepAliveTimeoutSuspended)
            return;
        LOG_WARN(SessionLog, "Closing session {} from {}:{}: nothing received within {} ms of a keepalive", session->_sessionId, session->GetRemoteAddress().to_string(), session->GetRemotePort(), timeout.count());
        session->CloseNow();
    });
}

void SessionBase::CloseForProtocol(std::string const& reason)
{
    LOG_WARN(SessionLog, "Closing session {} from {}:{}: {}", _sessionId, GetRemoteAddress().to_string(), GetRemotePort(), reason);
    CloseNow();
}

std::shared_ptr<SessionBase> SessionBase::Self()
{
    return std::static_pointer_cast<SessionBase>(shared_from_this());
}
