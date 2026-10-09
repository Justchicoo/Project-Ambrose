/*
 * Project Ambrose by Imjustchico
 * One recorded action and the things it acted on, written into the panel store's audit tables: an event carries the id a forwarder repeats safely, the batch it belongs to, its name in namespace:path.action form, who did it, from which address and agent, on which node, how it ended and why, and any properties worth keeping, and a scope writes it in the same transaction as the change it describes, so a change that is not recorded is not applied either; a chain verification names the first row that no longer verifies and the last row checked, since every row after the first broken one fails with it.
 */

#ifndef AMBROSE_PANELAUDIT_H
#define AMBROSE_PANELAUDIT_H

#include "Types.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class PanelStore;
struct AdminRequest;

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
    int64 DatabaseId = 0;
    int64 CreatedEpochMs = 0;
    std::vector<AuditSubject> Subjects;

    AuditEvent& On(std::string kind, std::string id, std::string name = {});
};

struct AuditChainVerification
{
    bool Valid = true;
    int64 RowsChecked = 0;
    int64 FirstInvalidId = 0;
    int64 LastRowId = 0;
    int64 ElapsedMs = 0;
    int64 PendingEvents = 0;
    std::string Problem;
};

class AuditScope
{
public:
    AuditScope(AdminRequest const& request, std::string name);

    AuditEvent& Event() { return _event; }
    AuditEvent const& Event() const { return _event; }
    AuditScope& Subject(std::string kind, std::string id, std::string name = {});

private:
    AuditEvent _event;
};

namespace PanelAudit
{
    constexpr int64 VerificationBudgetMs = 5000;

    std::string NewEventId();
    std::string_view ToString(AuditActor actor) noexcept;
    std::string_view ToString(AuditResult result) noexcept;

    bool Write(PanelStore& store, AuditEvent const& event, std::string& error);
    bool Record(PanelStore& store, AuditEvent const& event, std::function<bool(std::string& error)> const& change, std::string& error, bool queueForCollector = false);
    bool Record(PanelStore& store, AuditEvent& event, std::function<bool(AuditEvent& event, std::string& error)> const& change, std::string& error, bool queueForCollector = false);
    bool Finalize(PanelStore& store, std::string& error, bool queueForCollector = false);
    bool EnsureChain(PanelStore& store, std::string& error);
    bool VerifyChain(PanelStore& store, AuditChainVerification& verification, std::string& error);
    int64 PendingCount(PanelStore& store, std::string& error);
    int64 Count(PanelStore& store, std::string_view name);
}

#endif
