/*
 * Project Ambrose by Imjustchico
 * Writes an event and its subjects through prepared statements, giving it an id when the caller has none, and runs a change and its record inside one transaction: the record is written first, so a change that cannot be recorded never happens, and either both are committed or the store is left as it was.
 */

#include "PanelAudit.h"
#include "AdminRouter.h"
#include "Base64.h"
#include "CryptoRandom.h"
#include "PanelStore.h"
#include "SHA256.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <array>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
    nlohmann::json OptionalJson(std::string const& value)
    {
        return value.empty() ? nlohmann::json(nullptr) : nlohmann::json(value);
    }

    std::string HashHex(SHA256::Digest const& digest)
    {
        std::string text;
        text.reserve(digest.size() * 2);
        for (uint8 const byte : digest)
            text += fmt::format("{:02x}", byte);
        return text;
    }

    nlohmann::json EventJson(AuditEvent const& event, std::string_view previousHash)
    {
        nlohmann::json subjects = nlohmann::json::array();
        for (AuditSubject const& subject : event.Subjects)
            subjects.push_back({ { "kind", subject.Kind }, { "id", OptionalJson(subject.Id) }, { "name", OptionalJson(subject.Name) } });
        return {
            { "id", event.DatabaseId },
            { "event_id", event.EventId },
            { "batch_id", OptionalJson(event.BatchId) },
            { "created_epoch_ms", event.CreatedEpochMs },
            { "name", event.Name },
            { "actor_type", PanelAudit::ToString(event.Actor) },
            { "actor_id", OptionalJson(event.ActorId) },
            { "actor_name", OptionalJson(event.ActorName) },
            { "address", OptionalJson(event.Address) },
            { "user_agent", OptionalJson(event.UserAgent) },
            { "node", OptionalJson(event.Node) },
            { "result", PanelAudit::ToString(event.Result) },
            { "error", OptionalJson(event.Error) },
            { "reason", OptionalJson(event.Reason) },
            { "properties", event.Properties.empty() ? std::string("{}") : event.Properties },
            { "subjects", std::move(subjects) },
            { "previous_hash", previousHash }
        };
    }

    std::string ChainHash(AuditEvent const& event, std::string_view previousHash)
    {
        return HashHex(SHA256::GetDigestOf(EventJson(event, previousHash).dump()));
    }

    std::optional<std::string> LatestHash(PanelStore& store, std::string& error)
    {
        std::optional<PanelStore::Statement> latest = store.Prepare("SELECT chain_hash FROM audit_event ORDER BY id DESC LIMIT 1", error);
        if (!latest)
            return std::nullopt;
        if (!latest->Step(error))
        {
            if (!error.empty())
                return std::nullopt;
            return std::string();
        }
        return latest->Text(0);
    }

    nlohmann::json StoredEventJson(PanelStore::Statement const& row, std::string_view previousHash, nlohmann::json subjects)
    {
        nlohmann::json value{
            { "id", row.Int64(0) },
            { "event_id", row.Text(1) },
            { "batch_id", row.IsNull(2) ? nlohmann::json(nullptr) : nlohmann::json(row.Text(2)) },
            { "created_epoch_ms", row.Int64(3) },
            { "name", row.Text(4) },
            { "actor_type", row.Text(5) },
            { "actor_id", row.IsNull(6) ? nlohmann::json(nullptr) : nlohmann::json(row.Text(6)) },
            { "actor_name", row.IsNull(7) ? nlohmann::json(nullptr) : nlohmann::json(row.Text(7)) },
            { "address", row.IsNull(8) ? nlohmann::json(nullptr) : nlohmann::json(row.Text(8)) },
            { "user_agent", row.IsNull(9) ? nlohmann::json(nullptr) : nlohmann::json(row.Text(9)) },
            { "node", row.IsNull(10) ? nlohmann::json(nullptr) : nlohmann::json(row.Text(10)) },
            { "result", row.Text(11) },
            { "error", row.IsNull(12) ? nlohmann::json(nullptr) : nlohmann::json(row.Text(12)) },
            { "reason", row.IsNull(13) ? nlohmann::json(nullptr) : nlohmann::json(row.Text(13)) },
            { "properties", row.Text(14) },
            { "subjects", std::move(subjects) },
            { "previous_hash", previousHash }
        };
        if (row.IsNull(5))
            value["actor_type"] = nullptr;
        return value;
    }
}

AuditEvent& AuditEvent::On(std::string kind, std::string id, std::string name)
{
    Subjects.push_back({ std::move(kind), std::move(id), std::move(name) });
    return *this;
}

AuditScope::AuditScope(AdminRequest const& request, std::string name)
{
    _event.EventId = PanelAudit::NewEventId();
    _event.Name = std::move(name);
    _event.Address = request.RemoteAddress;
    _event.UserAgent = request.UserAgent;
    if (request.Principal == "token")
    {
        _event.Actor = AuditActor::Token;
        _event.ActorId = request.Principal;
    }
    else if (request.Principal.starts_with("user:"))
    {
        _event.Actor = AuditActor::User;
        _event.ActorId = request.Principal.substr(5);
    }
    else
    {
        _event.Actor = AuditActor::System;
        _event.ActorId = request.Principal;
    }
}

AuditScope& AuditScope::Subject(std::string kind, std::string id, std::string name)
{
    _event.On(std::move(kind), std::move(id), std::move(name));
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
        "INSERT INTO audit_event (event_id, batch_id, created_epoch_ms, name, actor_type, actor_id, actor_name, address, user_agent, node, result, error, reason, properties, chain_hash)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '')", error);
    if (!insert)
        return false;
    std::string const eventId = event.EventId.empty() ? NewEventId() : event.EventId;
    insert->Bind(1, eventId);
    BindOrNull(*insert, 2, event.BatchId);
    insert->Bind(3, event.CreatedEpochMs == 0 ? PanelStore::NowEpochMs() : event.CreatedEpochMs);
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
    AuditEvent copy = event;
    return Record(store, copy, [&change](AuditEvent&, std::string& failure)
    {
        return !change || change(failure);
    }, error);
}

bool PanelAudit::Record(PanelStore& store, AuditEvent& event, std::function<bool(AuditEvent& event, std::string& error)> const& change, std::string& error)
{
    if (event.EventId.empty())
        event.EventId = NewEventId();
    event.CreatedEpochMs = PanelStore::NowEpochMs();
    if (!store.Begin(error))
        return false;
    std::optional<std::string> const previous = LatestHash(store, error);
    if (!previous)
    {
        store.Rollback();
        return false;
    }
    if (!Write(store, event, error))
    {
        store.Rollback();
        return false;
    }
    event.DatabaseId = store.LastInsertId();
    if (!event.Subjects.empty())
    {
        std::optional<PanelStore::Statement> id = store.Prepare("SELECT id FROM audit_event WHERE event_id = ?", error);
        if (!id)
        {
            store.Rollback();
            return false;
        }
        id->Bind(1, event.EventId);
        if (!id->Step(error))
        {
            store.Rollback();
            return false;
        }
        event.DatabaseId = id->Int64(0);
    }
    if (change && !change(event, error))
    {
        store.Rollback();
        return false;
    }
    std::optional<PanelStore::Statement> finalize = store.Prepare(
        "UPDATE audit_event SET result = ?, error = ?, reason = ?, properties = ?, chain_hash = ? WHERE id = ?", error);
    if (!finalize)
    {
        store.Rollback();
        return false;
    }
    finalize->Bind(1, ToString(event.Result));
    BindOrNull(*finalize, 2, event.Error);
    BindOrNull(*finalize, 3, event.Reason);
    finalize->Bind(4, event.Properties.empty() ? std::string("{}") : event.Properties);
    finalize->Bind(5, ChainHash(event, *previous));
    finalize->Bind(6, event.DatabaseId);
    if (!finalize->Run(error))
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

bool PanelAudit::EnsureChain(PanelStore& store, std::string& error)
{
    if (!store.Begin(error))
        return false;
    std::optional<PanelStore::Statement> rows = store.Prepare(
        "SELECT id, event_id, batch_id, created_epoch_ms, name, actor_type, actor_id, actor_name, address, user_agent, node, result, error, reason, properties, chain_hash "
        "FROM audit_event ORDER BY id", error);
    if (!rows)
    {
        store.Rollback();
        return false;
    }

    std::string previousHash;
    while (rows->Step(error))
    {
        int64 const id = rows->Int64(0);
        std::string const savedHash = rows->Text(15);
        if (!savedHash.empty())
        {
            previousHash = savedHash;
            continue;
        }

        std::optional<PanelStore::Statement> subjects = store.Prepare(
            "SELECT kind, subject_id, name FROM audit_subject WHERE event = ? ORDER BY position", error);
        if (!subjects)
        {
            store.Rollback();
            return false;
        }
        subjects->Bind(1, id);
        nlohmann::json subjectValues = nlohmann::json::array();
        while (subjects->Step(error))
        {
            nlohmann::json subject{
                { "kind", subjects->Text(0) },
                { "id", subjects->Text(1) },
                { "name", subjects->Text(2) }
            };
            if (subjects->IsNull(1))
                subject["id"] = nullptr;
            if (subjects->IsNull(2))
                subject["name"] = nullptr;
            subjectValues.push_back(std::move(subject));
        }
        if (!error.empty())
        {
            store.Rollback();
            return false;
        }
        nlohmann::json const rowValue = StoredEventJson(*rows, previousHash, std::move(subjectValues));
        std::string const hash = HashHex(SHA256::GetDigestOf(rowValue.dump()));
        std::optional<PanelStore::Statement> update = store.Prepare("UPDATE audit_event SET chain_hash = ? WHERE id = ?", error);
        if (!update)
        {
            store.Rollback();
            return false;
        }
        update->Bind(1, hash);
        update->Bind(2, id);
        if (!update->Run(error))
        {
            store.Rollback();
            return false;
        }
        previousHash = hash;
    }
    if (!error.empty())
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
