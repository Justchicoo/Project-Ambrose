/*
 * Project Ambrose by Imjustchico
 * Per-app message dispatch: rules that handle a declared message in given session statuses, list one as not handled yet, or refuse it as server-only, resolved by tag against each loaded catalog, with strikes for protocol violations, a per-session budget on dropped-message logging, and declarations of the messages the app sends.
 */

#ifndef AMBROSE_MESSAGEHANDLERTABLE_H
#define AMBROSE_MESSAGEHANDLERTABLE_H

#include "Frame.h"
#include "Log.h"
#include "MessageRegistry.h"
#include "SessionStatus.h"

#include <fmt/format.h>

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class MessageProcessing : uint8
{
    InPlace,
    Queued
};

enum class QueuedMessageDrain : uint8
{
    None,
    DrainedByOwner
};

enum class MessageRuleKind : uint8
{
    Handled,
    Pending,
    Refused
};

enum class DispatchResult : uint8
{
    Handled,
    Queued,
    NotHandled,
    WrongStatus,
    Refused,
    UnknownMessage,
    DecodeFailed,
    HandlerFailed,
    QueueFull,
    NoDefinitions
};

enum class MessageInvokeStatus : uint8
{
    Handled,
    HandledWithTrailingBytes,
    Truncated,
    NotDeclared
};

struct MessageRule
{
    uint8 ServiceId = 0;
    std::string Tag;
    std::string HandlerName;
    MessageRuleKind Kind = MessageRuleKind::Pending;
    SessionStatusMask Statuses = 0;
    MessageProcessing Processing = MessageProcessing::InPlace;
    bool (*Declare)(MessageRegistry& registry, std::vector<std::string>& errors) = nullptr;
};

class MessageHandlerTableBase
{
public:
    static constexpr char const* LogCategory = "network.opcode";

    MessageHandlerTableBase(MessageHandlerTableBase const&) = delete;
    MessageHandlerTableBase& operator=(MessageHandlerTableBase const&) = delete;

    std::string const& GetAppName() const noexcept { return _appName; }
    std::vector<MessageRule> const& GetRules() const noexcept { return _rules; }
    bool IsOwnService(uint8 serviceId) const noexcept;

    void Pending(uint8 serviceId, std::string_view tag, SessionStatusMask statuses);
    void Refuse(uint8 serviceId, std::string_view tag);

    template<DeclaredMessage T>
    void Sends()
    {
        _sentDeclarations.push_back([](MessageRegistry& registry, std::vector<std::string>& errors) { return registry.Declare<T>(errors); });
    }

    bool Declare(MessageRegistry& registry, std::vector<std::string>& errors) const;
    bool Validate(MessageCatalog const& catalog, std::vector<std::string>& errors) const;
    MessageRule const* FindRule(MessageCatalogPtr const& catalog, uint8 serviceId, uint8 order) const;

    static std::string DescribeMessage(MessageInfo const& info);

protected:
    MessageHandlerTableBase(std::string appName, std::vector<uint8> ownServices, QueuedMessageDrain drain);
    ~MessageHandlerTableBase() = default;

    std::size_t AddRule(MessageRule rule);
    std::size_t IndexOf(MessageRule const& rule) const noexcept { return static_cast<std::size_t>(&rule - _rules.data()); }

private:
    struct Resolution
    {
        std::weak_ptr<MessageCatalog const> Catalog;
        std::vector<uint16> Slots;
    };

    std::shared_ptr<Resolution const> Resolve(MessageCatalogPtr const& catalog) const;

    std::string _appName;
    std::vector<uint8> _ownServices;
    QueuedMessageDrain _drain;
    std::vector<MessageRule> _rules;
    std::vector<bool (*)(MessageRegistry& registry, std::vector<std::string>& errors)> _sentDeclarations;
    mutable std::mutex _resolutionMutex;
    mutable std::atomic<std::shared_ptr<Resolution const>> _resolution;
};

namespace MessageHandlerDetail
{
    template<typename Handler>
    struct Traits;

    template<typename Session, typename Message>
    struct Traits<void (Session::*)(Message&)>
    {
        using MessageType = Message;
    };
}

template<typename SessionT>
class MessageHandlerTable : public MessageHandlerTableBase
{
public:
    using Invoker = MessageInvokeStatus (*)(SessionT& session, MessageCatalog const& catalog, std::span<uint8 const> body);

    MessageHandlerTable(std::string appName, std::vector<uint8> ownServices, QueuedMessageDrain drain = QueuedMessageDrain::None)
        : MessageHandlerTableBase(std::move(appName), std::move(ownServices), drain)
    {
    }

    template<auto Handler>
    void Accept(SessionStatusMask statuses, MessageProcessing processing, std::string_view handlerName);

    DispatchResult Dispatch(SessionT& session, MessageCatalogPtr const& catalog, DmlMessageData& message) const;

private:
    DispatchResult Run(SessionT& session, MessageCatalog const& catalog, std::size_t index, uint8 serviceId, uint8 order, std::span<uint8 const> body) const;
    static void Drop(SessionT& session, LogLevel level, std::string const& text);
    static void Violation(SessionT& session, std::string const& text, std::string const& strike);

    std::vector<Invoker> _invokers;
};

template<typename SessionT>
template<auto Handler>
void MessageHandlerTable<SessionT>::Accept(SessionStatusMask statuses, MessageProcessing processing, std::string_view handlerName)
{
    using Message = typename MessageHandlerDetail::Traits<decltype(Handler)>::MessageType;
    MessageRule rule;
    rule.ServiceId = static_cast<uint8>(Message::ServiceId);
    rule.Tag = std::string(Message::Tag);
    rule.HandlerName = std::string(handlerName);
    rule.Kind = MessageRuleKind::Handled;
    rule.Statuses = statuses;
    rule.Processing = processing;
    rule.Declare = [](MessageRegistry& registry, std::vector<std::string>& errors) { return registry.Declare<Message>(errors); };
    std::size_t const index = AddRule(std::move(rule));
    if (_invokers.size() <= index)
        _invokers.resize(index + 1, nullptr);
    _invokers[index] = [](SessionT& session, MessageCatalog const& catalog, std::span<uint8 const> body)
    {
        if (!catalog.IsDeclared<Message>())
            return MessageInvokeStatus::NotDeclared;
        Message message;
        MessageDecodeStatus const status = catalog.Decode(body, message);
        if (status == MessageDecodeStatus::Truncated)
            return MessageInvokeStatus::Truncated;
        (session.*Handler)(message);
        return status == MessageDecodeStatus::TrailingBytes ? MessageInvokeStatus::HandledWithTrailingBytes : MessageInvokeStatus::Handled;
    };
}

template<typename SessionT>
void MessageHandlerTable<SessionT>::Drop(SessionT& session, LogLevel level, std::string const& text)
{
    if (session.AllowDropLog())
    {
        LOG_DYNAMIC(level, LogCategory, "{}", text);
        return;
    }
    session.AddStrike("dropped messages faster than the session's drop budget allows");
}

template<typename SessionT>
void MessageHandlerTable<SessionT>::Violation(SessionT& session, std::string const& text, std::string const& strike)
{
    if (session.AllowDropLog())
        LOG_WARN(LogCategory, "{}", text);
    session.AddStrike(strike);
}

template<typename SessionT>
DispatchResult MessageHandlerTable<SessionT>::Dispatch(SessionT& session, MessageCatalogPtr const& catalog, DmlMessageData& message) const
{
    uint8 const serviceId = message.ServiceId;
    uint8 const order = message.Order;
    if (!catalog)
    {
        Drop(session, LogLevel::Info, fmt::format("Unknown message ({}:{}) from session {}, {} bytes; no message definitions are loaded", serviceId, order, session.GetSessionId(), message.Body.size()));
        return DispatchResult::NoDefinitions;
    }

    MessageInfo const* const info = catalog->Find(serviceId, order);
    if (!info)
    {
        Violation(session, fmt::format("Session {} sent message ({}:{}), which the message definitions do not have", session.GetSessionId(), serviceId, order), fmt::format("unknown message ({}:{})", serviceId, order));
        return DispatchResult::UnknownMessage;
    }

    std::string const name = DescribeMessage(*info);
    if (message.Body.size() < info->MinSize)
    {
        Violation(session, fmt::format("Dropped {} from session {}: its {}-byte body is shorter than the {} bytes its definition needs", name, session.GetSessionId(), message.Body.size(), info->MinSize), fmt::format("{} with a truncated body", name));
        return DispatchResult::DecodeFailed;
    }

    MessageRule const* const rule = FindRule(catalog, serviceId, order);
    if (!rule)
    {
        if (IsOwnService(serviceId))
        {
            Drop(session, LogLevel::Info, fmt::format("Session {} sent {}, which {} does not handle yet", session.GetSessionId(), name, GetAppName()));
            return DispatchResult::NotHandled;
        }
        Violation(session, fmt::format("Session {} sent {}, which {} never accepts", session.GetSessionId(), name, GetAppName()), fmt::format("{}, which {} never accepts", name, GetAppName()));
        return DispatchResult::Refused;
    }
    if (rule->Kind == MessageRuleKind::Refused)
    {
        Violation(session, fmt::format("Session {} sent {}, which only the server sends", session.GetSessionId(), name), fmt::format("{}, which only the server sends", name));
        return DispatchResult::Refused;
    }

    SessionStatus const status = session.GetStatus();
    if (!SessionStatuses::Allows(rule->Statuses, status))
    {
        Drop(session, LogLevel::Warn, fmt::format("Session {} received {} in state {}, which needs {}", session.GetSessionId(), info->Definition->Tag, SessionStatuses::GetName(status), SessionStatuses::Describe(rule->Statuses)));
        return DispatchResult::WrongStatus;
    }
    if (rule->Kind == MessageRuleKind::Pending)
    {
        Drop(session, LogLevel::Info, fmt::format("Session {} sent {}, which {} does not handle yet", session.GetSessionId(), name, GetAppName()));
        return DispatchResult::NotHandled;
    }

    std::size_t const index = IndexOf(*rule);
    if (rule->Processing == MessageProcessing::Queued)
    {
        std::size_t const bytes = message.Body.size();
        bool const queued = session.QueueInbound([this, &session, catalog, index, serviceId, order, body = std::move(message.Body)]()
        {
            MessageRule const& queuedRule = GetRules()[index];
            SessionStatus const current = session.GetStatus();
            if (!SessionStatuses::Allows(queuedRule.Statuses, current))
            {
                Drop(session, LogLevel::Warn, fmt::format("Session {} received {} in state {} by the time it was processed, which needs {}", session.GetSessionId(), queuedRule.Tag, SessionStatuses::GetName(current), SessionStatuses::Describe(queuedRule.Statuses)));
                return;
            }
            Run(session, *catalog, index, serviceId, order, body);
        }, bytes);
        return queued ? DispatchResult::Queued : DispatchResult::QueueFull;
    }
    return Run(session, *catalog, index, serviceId, order, message.Body);
}

template<typename SessionT>
DispatchResult MessageHandlerTable<SessionT>::Run(SessionT& session, MessageCatalog const& catalog, std::size_t index, uint8 serviceId, uint8 order, std::span<uint8 const> body) const
{
    MessageRule const& rule = GetRules()[index];
    MessageInfo const* const info = catalog.Find(serviceId, order);
    std::string const name = info ? DescribeMessage(*info) : fmt::format("message ({}:{})", serviceId, order);
    Invoker const invoke = index < _invokers.size() ? _invokers[index] : nullptr;
    if (!invoke)
    {
        LOG_ERROR(LogCategory, "{} has no handler for {}", GetAppName(), name);
        return DispatchResult::HandlerFailed;
    }

    LOG_DEBUG(LogCategory, "{} from session {}, {} bytes", name, session.GetSessionId(), body.size());
    MessageInvokeStatus status = MessageInvokeStatus::Handled;
    try
    {
        status = invoke(session, catalog, body);
    }
    catch (std::exception const& failure)
    {
        LOG_ERROR(LogCategory, "{} failed on {} from session {}: {}", rule.HandlerName, name, session.GetSessionId(), failure.what());
        session.Kick(fmt::format("{} failed on {}", rule.HandlerName, name));
        return DispatchResult::HandlerFailed;
    }

    switch (status)
    {
        case MessageInvokeStatus::NotDeclared:
            LOG_ERROR(LogCategory, "{} cannot decode {}: its declaration did not resolve against the loaded message definitions", GetAppName(), name);
            return DispatchResult::HandlerFailed;
        case MessageInvokeStatus::Truncated:
            Violation(session, fmt::format("Dropped {} from session {}: its {}-byte body is shorter than the definition", name, session.GetSessionId(), body.size()), fmt::format("{} with a truncated body", name));
            return DispatchResult::DecodeFailed;
        case MessageInvokeStatus::HandledWithTrailingBytes:
            LOG_DEBUG(LogCategory, "{} from session {} carried bytes past its definition", name, session.GetSessionId());
            return DispatchResult::Handled;
        case MessageInvokeStatus::Handled:
            break;
    }
    return DispatchResult::Handled;
}

#endif
