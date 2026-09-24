/*
 * Project Ambrose by Imjustchico
 * Writes an event and its subjects through prepared statements, giving it an id when the caller has none, and runs a change and its record inside one transaction: the record is written first, so a change that cannot be recorded never happens, and either both are committed or the store is left as it was.
 */

#include "PanelAudit.h"
#include "Base64.h"
#include "CryptoRandom.h"
#include "PanelStore.h"

#include <fmt/format.h>

#include <array>
#include <optional>
#include <utility>

AuditEvent& AuditEvent::On(std::string kind, std::string id, std::string name)
{
    Subjects.push_back({ std::move(kind), std::move(id), std::move(name) });
    return *this;
}

std::string PanelAudit::NewEventId()
{
    std::array<uint8, 16> const bytes = Ambrose::Crypto::GetRandomArray<16>();
    return Base64::Encode(bytes, Base64::Alphabet::UrlSafe, Base64::Padding::Omitted);
}

std::string_view PanelAudit::ToString(AuditActor actor) noexcept
{
    switch (actor)
    {
        case AuditActor::User: return "user";
        case AuditActor::Token: return "token";
        case AuditActor::Schedule: return "schedule";
        case AuditActor::System: break;
    }
    return "system";
}

std::string_view PanelAudit::ToString(AuditResult result) noexcept
{
    switch (result)
    {
        case AuditResult::Succeeded: return "succeeded";
        case AuditResult::Failed: return "failed";
        case AuditResult::Refused: return "refused";
        case AuditResult::Throttled: break;
    }
    return "throttled";
}

namespace
{
    void BindOrNull(PanelStore::Statement& statement, int index, std::string const& value)
    {
        if (value.empty())
            statement.BindNull(index);
        else
            statement.Bind(index, value);
    }
}

bool PanelAudit::Write(PanelStore& store, AuditEvent const& event, std::string& error)
{
    error.clear();
    if (event.Name.empty())
    {
        error = "an audit event has no name";
        return false;
    }
    std::optional<PanelStore::Statement> insert = store.Prepare(
        "INSERT INTO audit_event (event_id, batch_id, created_epoch_ms, name, actor_type, actor_id, actor_name, address, user_agent, node, result, error, reason, properties)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", error);
    if (!insert)
        return false;
    std::string const eventId = event.EventId.empty() ? NewEventId() : event.EventId;
    insert->Bind(1, eventId);
    BindOrNull(*insert, 2, event.BatchId);
    insert->Bind(3, PanelStore::NowEpochMs());
    insert->Bind(4, event.Name);
    insert->Bind(5, ToString(event.Actor));
    BindOrNull(*insert, 6, event.ActorId);
    BindOrNull(*insert, 7, event.ActorName);
    BindOrNull(*insert, 8, event.Address);
    BindOrNull(*insert, 9, event.UserAgent);
    BindOrNull(*insert, 10, event.Node);
    insert->Bind(11, ToString(event.Result));
    BindOrNull(*insert, 12, event.Error);
    BindOrNull(*insert, 13, event.Reason);
    insert->Bind(14, event.Properties.empty() ? std::string("{}") : event.Properties);
    if (!insert->Run(error))
    {
        error = fmt::format("the audit row for {} could not be written: {}", event.Name, error);
        return false;
    }
    insert.reset();

    int64 const row = store.LastInsertId();
    if (event.Subjects.empty())
        return true;
    std::optional<PanelStore::Statement> subject = store.Prepare("INSERT INTO audit_subject (event, position, kind, subject_id, name) VALUES (?, ?, ?, ?, ?)", error);
    if (!subject)
        return false;
    for (std::size_t index = 0; index < event.Subjects.size(); ++index)
    {
        AuditSubject const& what = event.Subjects[index];
        subject->Reset();
        subject->Bind(1, row);
        subject->Bind(2, static_cast<int64>(index));
        subject->Bind(3, what.Kind);
        BindOrNull(*subject, 4, what.Id);
        BindOrNull(*subject, 5, what.Name);
        if (!subject->Run(error))
        {
            error = fmt::format("a subject of the audit row for {} could not be written: {}", event.Name, error);
            return false;
        }
    }
    return true;
}

bool PanelAudit::Record(PanelStore& store, AuditEvent const& event, std::function<bool(std::string& error)> const& change, std::string& error)
{
    if (!store.Begin(error))
        return false;
    if (!Write(store, event, error))
    {
        store.Rollback();
        return false;
    }
    if (change && !change(error))
    {
        store.Rollback();
        return false;
    }
    if (!store.Commit(error))
    {
        store.Rollback();
        return false;
    }
    return true;
}

int64 PanelAudit::Count(PanelStore& store, std::string_view name)
{
    std::string error;
    std::optional<PanelStore::Statement> rows = store.Prepare("SELECT COUNT(*) FROM audit_event WHERE name = ?", error);
    if (!rows)
        return -1;
    rows->Bind(1, name);
    return rows->Step(error) ? rows->Int64(0) : -1;
}
