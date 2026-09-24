/*
 * Project Ambrose by Imjustchico
 * One recorded action and the things it acted on, written into the panel store's audit tables: an event carries the id a forwarder repeats safely, the batch it belongs to, its name in namespace:path.action form, who did it, from which address and agent, on which node, how it ended and why, and any properties worth keeping, and a scope writes it in the same transaction as the change it describes, so a change that is not recorded is not applied either.
 */

#ifndef AMBROSE_PANELAUDIT_H
#define AMBROSE_PANELAUDIT_H

#include "Types.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class PanelStore;

enum class AuditActor : uint8
{
    User,
    Token,
    Schedule,
    System
};

enum class AuditResult : uint8
{
    Succeeded,
    Failed,
    Refused,
    Throttled
};

struct AuditSubject
{
    std::string Kind;
    std::string Id;
    std::string Name;
};

struct AuditEvent
{
    std::string EventId;
    std::string BatchId;
    std::string Name;
    AuditActor Actor = AuditActor::System;
    std::string ActorId;
    std::string ActorName;
    std::string Address;
    std::string UserAgent;
    std::string Node;
    AuditResult Result = AuditResult::Succeeded;
    std::string Error;
    std::string Reason;
    std::string Properties = "{}";
    std::vector<AuditSubject> Subjects;

    AuditEvent& On(std::string kind, std::string id, std::string name = {});
};

namespace PanelAudit
{
    std::string NewEventId();
    std::string_view ToString(AuditActor actor) noexcept;
    std::string_view ToString(AuditResult result) noexcept;

    bool Write(PanelStore& store, AuditEvent const& event, std::string& error);
    bool Record(PanelStore& store, AuditEvent const& event, std::function<bool(std::string& error)> const& change, std::string& error);
    int64 Count(PanelStore& store, std::string_view name);
}

#endif
