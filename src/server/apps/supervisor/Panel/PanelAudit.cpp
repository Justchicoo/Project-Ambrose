/*
 * Project Ambrose by Imjustchico
 * Writes an event and its subjects through prepared statements, giving it an id when the caller has none, and runs a change and its record inside one transaction: the record is written first, so a change that cannot be recorded never happens, and either both are committed or the store is left as it was; each new row is chained from the stored head rather than by walking the store, and a store whose rows and head disagree still opens and keeps recording so the break is shown rather than stopping the panel.
 */

#include "PanelAudit.h"
#include "AdminRouter.h"
#include "Base64.h"
#include "CryptoRandom.h"
#include "PanelStore.h"
#include "SHA256.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    std::string HashHex(SHA256::Digest const& digest)
    {
        constexpr char Hex[] = "0123456789abcdef";
        std::string text(digest.size() * 2, '\0');
        for (std::size_t index = 0; index < digest.size(); ++index)
        {
            text[index * 2] = Hex[digest[index] >> 4];
            text[index * 2 + 1] = Hex[digest[index] & 0x0f];
        }
        return text;
    }

    struct ChainTip
    {
        int64 Id = 0;
        std::string Hash;
    };

    std::optional<ChainTip> StoredTip(PanelStore& store, std::string& error)
    {
        std::optional<PanelStore::Statement> head = store.Prepare("SELECT last_event_id, last_hash FROM audit_chain_head WHERE id = 1", error);
        if (!head)
            return std::nullopt;
        if (!head->Step(error))
        {
            if (!error.empty())
                return std::nullopt;
            error = "the audit chain head is missing";
            return std::nullopt;
        }
        return ChainTip{ head->Int64(0), head->Text(1) };
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

    void MarkInvalid(AuditChainVerification& verification, int64 id, std::string problem)
    {
        if (verification.Valid)
        {
            verification.Valid = false;
            verification.FirstInvalidId = id;
            verification.Problem = std::move(problem);
        }
    }

    bool VerifyRows(PanelStore& store, AuditChainVerification& verification, int64& lastId, std::string& lastHash,
        std::string& error, bool allowUnhashedTail = false, ChainTip* verifiedPrefix = nullptr)
    {
        verification = {};
        lastId = 0;
        lastHash.clear();
        bool unhashedTail = false;
        if (verifiedPrefix)
            *verifiedPrefix = {};
        std::optional<PanelStore::Statement> rows = store.Prepare(
            "SELECT id, event_id, batch_id, created_epoch_ms, name, actor_type, actor_id, actor_name, address, user_agent, node, result, error, reason, properties, chain_hash "
            "FROM audit_event ORDER BY id", error);
        if (!rows)
            return false;
        std::optional<PanelStore::Statement> subjects = store.Prepare(
            "SELECT kind, subject_id, name FROM audit_subject WHERE event = ? ORDER BY position", error);
        if (!subjects)
            return false;

        while (rows->Step(error))
        {
            int64 const id = rows->Int64(0);
            ++verification.RowsChecked;
            if (id != lastId + 1)
                MarkInvalid(verification, lastId + 1, fmt::format("audit row {} is missing", lastId + 1));

            subjects->Reset();
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
                return false;

            nlohmann::json const rowValue = StoredEventJson(*rows, lastHash, std::move(subjectValues));
            std::string const expected = HashHex(SHA256::GetDigestOf(rowValue.dump()));
            std::string const savedHash = rows->Text(15);
            if (savedHash.empty() && allowUnhashedTail)
                unhashedTail = true;
            else if (unhashedTail)
                MarkInvalid(verification, id, fmt::format("audit row {} is hashed after an unhashed row", id));
            else if (savedHash != expected)
                MarkInvalid(verification, id, fmt::format("audit row {} does not match its contents or previous row", id));
            else if (verifiedPrefix)
                *verifiedPrefix = { id, expected };
            lastId = id;
            lastHash = std::move(expected);
        }
        verification.LastRowId = lastId;
        return error.empty();
    }

    bool SetChainTip(PanelStore& store, int64 id, std::string_view hash, std::string& error)
    {
        std::optional<PanelStore::Statement> update = store.Prepare(
            "UPDATE audit_chain_head SET last_event_id = ?, last_hash = ? WHERE id = 1", error);
        if (!update)
            return false;
        update->Bind(1, id);
        update->Bind(2, hash);
        if (!update->Run(error))
            return false;
        if (store.Changed() != 1)
        {
            error = "the audit chain head is missing";
            return false;
        }
        return true;
    }

    bool FinalizePending(PanelStore& store, ChainTip tip, std::string& error, bool queueForCollector)
    {
        std::optional<PanelStore::Statement> rows = store.Prepare(
            "SELECT id, event_id, batch_id, created_epoch_ms, name, actor_type, actor_id, actor_name, address, user_agent, node, result, error, reason, properties, chain_hash "
            "FROM audit_event WHERE chain_hash = '' ORDER BY id", error);
        if (!rows)
            return false;
        std::optional<PanelStore::Statement> subjects = store.Prepare(
            "SELECT kind, subject_id, name FROM audit_subject WHERE event = ? ORDER BY position", error);
        if (!subjects)
            return false;
        std::optional<PanelStore::Statement> update = store.Prepare("UPDATE audit_event SET chain_hash = ? WHERE id = ?", error);
        if (!update)
            return false;
        std::optional<PanelStore::Statement> enqueue;
        if (queueForCollector)
        {
            enqueue = store.Prepare("INSERT INTO panel_audit_outbox (event_id, payload) VALUES (?, ?)", error);
            if (!enqueue)
                return false;
        }

        std::vector<std::pair<int64, nlohmann::json>> pending;
        while (rows->Step(error))
        {
            int64 const id = rows->Int64(0);
            subjects->Reset();
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
                return false;
            pending.emplace_back(id, StoredEventJson(*rows, {}, std::move(subjectValues)));
        }
        if (!error.empty())
            return false;
        rows.reset();

        for (auto& [id, value] : pending)
        {
            value["previous_hash"] = tip.Hash;
            std::string const hash = HashHex(SHA256::GetDigestOf(value.dump()));
            update->Reset();
            update->Bind(1, hash);
            update->Bind(2, id);
            if (!update->Run(error))
                return false;
            if (enqueue)
            {
                value["chain_hash"] = hash;
                enqueue->Reset();
                enqueue->Bind(1, value["event_id"].get<std::string>());
                enqueue->Bind(2, value.dump());
                if (!enqueue->Run(error))
                    return false;
            }
            tip = { id, hash };
        }
        return SetChainTip(store, tip.Id, tip.Hash, error);
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

bool PanelAudit::Record(PanelStore& store, AuditEvent const& event, std::function<bool(std::string& error)> const& change, std::string& error, bool queueForCollector)
{
    AuditEvent copy = event;
    return Record(store, copy, [&change](AuditEvent&, std::string& failure)
    {
        return !change || change(failure);
    }, error, queueForCollector);
}

bool PanelAudit::Record(PanelStore& store, AuditEvent& event, std::function<bool(AuditEvent& event, std::string& error)> const& change, std::string& error, bool queueForCollector)
{
    error.clear();
    if (!store.Begin(error))
        return false;
    if (event.EventId.empty())
        event.EventId = NewEventId();
    event.CreatedEpochMs = PanelStore::NowEpochMs();
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
        "UPDATE audit_event SET result = ?, error = ?, reason = ?, properties = ? WHERE id = ?", error);
    if (!finalize)
    {
        store.Rollback();
        return false;
    }
    finalize->Bind(1, ToString(event.Result));
    BindOrNull(*finalize, 2, event.Error);
    BindOrNull(*finalize, 3, event.Reason);
    finalize->Bind(4, event.Properties.empty() ? std::string("{}") : event.Properties);
    finalize->Bind(5, event.DatabaseId);
    if (!finalize->Run(error) || !Finalize(store, error, queueForCollector))
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

bool PanelAudit::Finalize(PanelStore& store, std::string& error, bool queueForCollector)
{
    error.clear();
    std::optional<ChainTip> const tip = StoredTip(store, error);
    if (!tip)
        return false;
    return FinalizePending(store, *tip, error, queueForCollector);
}

bool PanelAudit::EnsureChain(PanelStore& store, std::string& error)
{
    error.clear();
    if (!store.Begin(error))
        return false;
    std::optional<ChainTip> headTip = StoredTip(store, error);
    if (!headTip)
    {
        store.Rollback();
        return false;
    }
    if (headTip->Id != 0 || !headTip->Hash.empty())
    {
        store.Rollback();
        return true;
    }
    AuditChainVerification verification;
    int64 lastId = 0;
    std::string lastHash;
    ChainTip verifiedPrefix;
    if (!VerifyRows(store, verification, lastId, lastHash, error, true, &verifiedPrefix))
    {
        store.Rollback();
        return false;
    }
    if (!verification.Valid)
    {
        store.Rollback();
        return true;
    }
    if (!FinalizePending(store, std::move(verifiedPrefix), error, false))
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

bool PanelAudit::VerifyChain(PanelStore& store, AuditChainVerification& verification, std::string& error)
{
    error.clear();
    auto const started = std::chrono::steady_clock::now();
    int64 lastId = 0;
    std::string lastHash;
    if (!VerifyRows(store, verification, lastId, lastHash, error))
        return false;

    std::optional<PanelStore::Statement> head = store.Prepare("SELECT last_event_id, last_hash FROM audit_chain_head WHERE id = 1", error);
    if (!head)
        return false;
    if (!head->Step(error))
    {
        if (!error.empty())
            return false;
        MarkInvalid(verification, lastId == 0 ? 1 : lastId, "the audit chain head is missing");
        verification.LastRowId = std::max(lastId, verification.FirstInvalidId);
        verification.ElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        return true;
    }
    int64 const expectedId = head->Int64(0);
    std::string const expectedHash = head->Text(1);
    if (expectedId != lastId)
    {
        int64 const missing = expectedId > lastId ? lastId + 1 : expectedId;
        MarkInvalid(verification, missing, fmt::format("the audit chain head expects row {} but the last stored row is {}", expectedId, lastId));
    }
    else if (expectedHash != lastHash)
        MarkInvalid(verification, lastId == 0 ? 1 : lastId, "the audit chain head does not match the last row");
    verification.LastRowId = std::max(lastId, verification.FirstInvalidId);
    verification.ElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
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

int64 PanelAudit::PendingCount(PanelStore& store, std::string& error)
{
    error.clear();
    std::optional<PanelStore::Statement> rows = store.Prepare(
        "SELECT COUNT(*) FROM panel_audit_outbox WHERE delivered_epoch_ms IS NULL", error);
    if (!rows || !rows->Step(error))
        return -1;
    return rows->Int64(0);
}
