/*
 * Project Ambrose by Imjustchico
 * The queries themselves: one SELECT with every filter as a bound parameter, newest first with a cursor on the row id, addresses blanked unless the reader may see them or is the actor, CSV cells prefixed so a spreadsheet reads them as text, and a sweep that deletes by class retention and writes its own row.
 */

#include "PanelActivityLog.h"
#include "PanelActivityCatalog.h"
#include "PanelAudit.h"
#include "PanelStore.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <chrono>

namespace
{
    AuditEvent ToEvent(PanelActivityLog::Row const& row)
    {
        AuditEvent event;
        event.Name = row.Name;
        event.ActorName = row.ActorName;
        event.ActorId = row.ActorId;
        event.Address = row.Address;
        event.Reason = row.Reason;
        event.Error = row.Error;
        return event;
    }

    std::string Redact(std::string_view address, bool canSeeIps, std::string_view selfActorId, std::string_view actorId)
    {
        if (address.empty())
            return {};
        if (canSeeIps)
            return std::string(address);
        if (!selfActorId.empty() && selfActorId == actorId)
            return std::string(address);
        return {};
    }
}

namespace PanelActivityLog
{
    bool IsSecurityEvent(std::string_view name) noexcept
    {
        return name.starts_with("panel:session.") || name.starts_with("panel:step_up.") ||
            name.starts_with("user:two_factor.") || name.starts_with("user:recovery_codes.") ||
            name.starts_with("panel:user.") || name == "settings:secret.revealed" ||
            name.starts_with("panel:permission.") || name.starts_with("app:permission.");
    }

    std::string CsvCell(std::string_view value)
    {
        if (!value.empty() && (value[0] == '=' || value[0] == '+' || value[0] == '-' || value[0] == '@'))
            return fmt::format("\"'{}\"", value);
        bool quoted = value.find_first_of("\",\n\r") != std::string_view::npos;
        if (!quoted)
            return std::string(value);
        std::string out = "\"";
        for (char c : value)
        {
            if (c == '"')
                out += "\"\"";
            else
                out += c;
        }
        out += "\"";
        return out;
    }

    bool Query(PanelStore& store, Filter const& filter, bool canSeeIps, std::string_view selfActorId, Page& page, std::string& error)
    {
        int64 limit = std::clamp<int64>(filter.Limit, 1, MaxRows);
        std::string sql = "SELECT e.id, e.event_id, e.created_epoch_ms, e.name, e.actor_type, e.actor_id, e.actor_name,"
            " e.address, e.node, e.result, e.error, e.reason FROM audit_event e WHERE 1 = 1";
        if (!filter.EventPrefix.empty())
            sql += " AND e.name LIKE ? ESCAPE '\\'";
        if (!filter.Actor.empty())
            sql += " AND (e.actor_id = ? OR e.actor_name LIKE ? ESCAPE '\\')";
        if (!filter.Subject.empty())
            sql += " AND EXISTS (SELECT 1 FROM audit_subject s WHERE s.event = e.id AND (s.subject_id = ? OR s.name LIKE ? ESCAPE '\\'))";
        if (!filter.Result.empty())
            sql += " AND e.result = ?";
        if (filter.FromEpochMs > 0)
            sql += " AND e.created_epoch_ms >= ?";
        if (filter.ToEpochMs > 0)
            sql += " AND e.created_epoch_ms <= ?";
        if (!filter.Address.empty())
            sql += " AND e.address = ?";
        if (filter.Cursor > 0)
            sql += " AND e.id < ?";
        sql += " ORDER BY e.id DESC LIMIT ?";

        std::optional<PanelStore::Statement> rows = store.Prepare(sql, error);
        if (!rows)
            return false;
        int index = 1;
        auto like = [](std::string_view v)
        {
            std::string out;
            for (char c : v)
            {
                if (c == '%' || c == '_' || c == '\\')
                    out += '\\';
                out += c;
            }
            return out + "%";
        };
        if (!filter.EventPrefix.empty())
            rows->Bind(index++, like(filter.EventPrefix));
        if (!filter.Actor.empty())
        {
            rows->Bind(index++, filter.Actor);
            rows->Bind(index++, like(filter.Actor));
        }
        if (!filter.Subject.empty())
        {
            rows->Bind(index++, filter.Subject);
            rows->Bind(index++, like(filter.Subject));
        }
        if (!filter.Result.empty())
            rows->Bind(index++, filter.Result);
        if (filter.FromEpochMs > 0)
            rows->Bind(index++, filter.FromEpochMs);
        if (filter.ToEpochMs > 0)
            rows->Bind(index++, filter.ToEpochMs);
        if (!filter.Address.empty())
            rows->Bind(index++, filter.Address);
        if (filter.Cursor > 0)
            rows->Bind(index++, filter.Cursor);
        rows->Bind(index++, limit + 1);

        page.Rows.clear();
        while (true)
        {
            if (!rows->Step(error))
                return false;
            if (rows->IsNull(0))
                break;
            Row row;
            row.Id = rows->Int64(0);
            row.EventId = rows->Text(1);
            row.CreatedEpochMs = rows->Int64(2);
            row.Name = rows->Text(3);
            row.ActorType = rows->Text(4);
            row.ActorId = rows->Text(5);
            row.ActorName = rows->Text(6);
            std::string address = rows->Text(7);
            row.Address = Redact(address, canSeeIps, selfActorId, row.ActorId);
            row.Node = rows->Text(8);
            row.Result = rows->Text(9);
            row.Error = rows->Text(10);
            row.Reason = rows->Text(11);
            row.Sentence = PanelActivityCatalog::Render(row.Name, ToEvent(row));
            page.Rows.push_back(std::move(row));
        }
        page.HasMore = static_cast<int64>(page.Rows.size()) > limit;
        if (page.HasMore)
        {
            page.NextCursor = page.Rows.back().Id;
            page.Rows.pop_back();
        }
        return true;
    }

    bool Details(PanelStore& store, int64 id, bool canSeeIps, std::string_view selfActorId, Row& row, std::string& properties, std::string& error)
    {
        std::optional<PanelStore::Statement> query = store.Prepare(
            "SELECT id, event_id, created_epoch_ms, name, actor_type, actor_id, actor_name, address, node, result, error, reason, properties"
            " FROM audit_event WHERE id = ?", error);
        if (!query)
            return false;
        query->Bind(1, id);
        if (!query->Step(error) || query->IsNull(0))
        {
            error = "no such activity row";
            return false;
        }
        row.Id = query->Int64(0);
        row.EventId = query->Text(1);
        row.CreatedEpochMs = query->Int64(2);
        row.Name = query->Text(3);
        row.ActorType = query->Text(4);
        row.ActorId = query->Text(5);
        row.ActorName = query->Text(6);
        row.Address = Redact(query->Text(7), canSeeIps, selfActorId, row.ActorId);
        row.Node = query->Text(8);
        row.Result = query->Text(9);
        row.Error = query->Text(10);
        row.Reason = query->Text(11);
        properties = query->Text(12);
        row.Sentence = PanelActivityCatalog::Render(row.Name, ToEvent(row));
        return true;
    }

    std::string ToCsv(std::vector<Row> const& rows)
    {
        std::string out = "id,time,name,sentence,actor,address,node,result,reason\r\n";
        for (Row const& row : rows)
        {
            out += CsvCell(std::to_string(row.Id)) + ",";
            out += CsvCell(std::to_string(row.CreatedEpochMs)) + ",";
            out += CsvCell(row.Name) + ",";
            out += CsvCell(row.Sentence) + ",";
            out += CsvCell(row.ActorName.empty() ? row.ActorId : row.ActorName) + ",";
            out += CsvCell(row.Address) + ",";
            out += CsvCell(row.Node) + ",";
            out += CsvCell(row.Result) + ",";
            out += CsvCell(row.Reason) + "\r\n";
        }
        return out;
    }

    std::string ToJson(std::vector<Row> const& rows)
    {
        nlohmann::json array = nlohmann::json::array();
        for (Row const& row : rows)
        {
            array.push_back({
                { "id", row.Id },
                { "event_id", row.EventId },
                { "created_epoch_ms", row.CreatedEpochMs },
                { "name", row.Name },
                { "sentence", row.Sentence },
                { "actor_type", row.ActorType },
                { "actor_id", row.ActorId },
                { "actor_name", row.ActorName },
                { "address", row.Address },
                { "node", row.Node },
                { "result", row.Result },
                { "error", row.Error },
                { "reason", row.Reason },
            });
        }
        return array.dump();
    }

    int64 Sweep(PanelStore& store, int64 securityRetentionDays, int64 highVolumeRetentionDays, std::string& error)
    {
        int64 const nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        int64 const securityCutoff = nowMs - securityRetentionDays * 86400000;
        int64 const volumeCutoff = nowMs - highVolumeRetentionDays * 86400000;

        if (!store.Begin(error))
            return -1;
        int64 removed = 0;
        std::optional<PanelStore::Statement> ids = store.Prepare("SELECT id, name, created_epoch_ms FROM audit_event", error);
        if (!ids)
        {
            store.Rollback();
            return -1;
        }
        std::vector<int64> expired;
        while (ids->Step(error))
        {
            if (ids->IsNull(0))
                break;
            std::string name = ids->Text(1);
            int64 created = ids->Int64(2);
            int64 cutoff = IsSecurityEvent(name) ? securityCutoff : volumeCutoff;
            if (created < cutoff)
                expired.push_back(ids->Int64(0));
        }
        if (!error.empty())
        {
            store.Rollback();
            return -1;
        }
        for (int64 id : expired)
        {
            std::optional<PanelStore::Statement> del = store.Prepare("DELETE FROM audit_event WHERE id = ?", error);
            if (!del)
            {
                store.Rollback();
                return -1;
            }
            del->Bind(1, id);
            if (!del->Run(error))
            {
                store.Rollback();
                return -1;
            }
            removed += store.Changed();
        }
        AuditEvent sweep;
        sweep.Name = std::string(SweepEventName);
        sweep.Actor = AuditActor::System;
        sweep.Reason = fmt::format("{} rows expired", removed);
        if (!PanelAudit::Write(store, sweep, error))
        {
            store.Rollback();
            return -1;
        }
        if (!store.Commit(error))
            return -1;
        return removed;
    }
}
