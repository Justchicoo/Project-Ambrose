/*
 * Project Ambrose by Imjustchico
 * Builds the activity query from the filter with every value bound rather than written into the SQL, reads one row more than the page so the cursor says whether more remain, loads each row's subjects in their order, matches an operator by the id the panel records and by the older user: form the relay once wrote, and sweeps by class with the class's names written in from the catalog, keeping the chain hash of every removed row whose next row stays before it deletes.
 */

#include "PanelActivity.h"
#include "AdminRouter.h"
#include "PanelActivityCatalog.h"
#include "PanelStore.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <utility>
#include <variant>

namespace
{
    using Bound = std::variant<int64, std::string>;

    std::optional<int64> Number(std::string_view text)
    {
        return Ambrose::StringTo<int64>(Ambrose::Trim(text));
    }

    bool IsOperatorId(std::string_view text)
    {
        std::optional<int64> const parsed = Number(text);
        return parsed && *parsed > 0;
    }

    void BindAll(PanelStore::Statement& statement, std::vector<Bound> const& values)
    {
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            int const position = static_cast<int>(index) + 1;
            if (std::holds_alternative<int64>(values[index]))
                statement.Bind(position, std::get<int64>(values[index]));
            else
                statement.Bind(position, std::get<std::string>(values[index]));
        }
    }

    std::string TextOr(PanelStore::Statement const& row, int column)
    {
        return row.IsNull(column) ? std::string() : row.Text(column);
    }

    AuditActor ActorOf(std::string_view stored)
    {
        if (stored == "user")
            return AuditActor::User;
        if (stored == "token")
            return AuditActor::Token;
        if (stored == "schedule")
            return AuditActor::Schedule;
        return AuditActor::System;
    }

    AuditResult StoredResult(std::string_view stored)
    {
        if (stored == "failed")
            return AuditResult::Failed;
        if (stored == "refused")
            return AuditResult::Refused;
        if (stored == "throttled")
            return AuditResult::Throttled;
        return AuditResult::Succeeded;
    }

    std::string OperatorId(AuditEvent const& event)
    {
        constexpr std::string_view Signed = "user:";
        if (event.ActorId.starts_with(Signed))
            return event.ActorId.substr(Signed.size());
        return event.ActorId;
    }

    std::string NameList(std::vector<std::string> const& names)
    {
        std::string list;
        for (std::string const& name : names)
            list += fmt::format("{}'{}'", list.empty() ? "" : ", ", name);
        return list.empty() ? std::string("''") : list;
    }

    std::string Expired(std::string_view alias, std::string const& highVolume, int64 securityCutoff, int64 highVolumeCutoff)
    {
        return fmt::format("(({0}.name IN ({1}) AND {0}.created_epoch_ms < {2}) OR ({0}.name NOT IN ({1}) AND {0}.created_epoch_ms < {3}))",
            alias, highVolume, highVolumeCutoff, securityCutoff);
    }

    std::optional<int64> CountOf(PanelStore& store, std::string const& sql, std::string& error)
    {
        std::optional<PanelStore::Statement> count = store.Prepare(sql, error);
        if (!count)
            return std::nullopt;
        if (!count->Step(error))
            return error.empty() ? std::optional<int64>(0) : std::nullopt;
        return count->Int64(0);
    }
}

std::optional<PanelActivityFilter> PanelActivity::ParseFilter(AdminRequest const& request, std::string& field, std::string& error)
{
    PanelActivityFilter filter;
    filter.Limit = DefaultPageRows;
    auto const refuse = [&field, &error](std::string_view name, std::string message)
    {
        field = std::string(name);
        error = std::move(message);
        return std::nullopt;
    };

    filter.Prefix = std::string(Ambrose::Trim(request.Query("prefix")));
    if (filter.Prefix.size() > 128)
        return refuse("prefix", "An event prefix is at most 128 characters");
    for (char const c : filter.Prefix)
        if (!(std::islower(static_cast<unsigned char>(c)) || c == ':' || c == '.' || c == '_'))
            return refuse("prefix", "An event prefix is lower-case letters, underscores, a colon and dots, such as app:power");

    if (std::string_view const actor = Ambrose::Trim(request.Query("actor")); !actor.empty())
    {
        if (!IsOperatorId(actor))
            return refuse("actor", "The actor is an operator's id");
        filter.ActorId = std::string(actor);
    }
    if (std::string_view const user = Ambrose::Trim(request.Query("user")); !user.empty())
    {
        if (!IsOperatorId(user))
            return refuse("user", "The user is an operator's id");
        filter.Involving = std::string(user);
    }
    if (std::string_view const subject = Ambrose::Trim(request.Query("subject")); !subject.empty())
    {
        std::size_t const colon = subject.find(':');
        if (colon == std::string_view::npos || colon == 0 || colon + 1 == subject.size() || subject.size() > 256)
            return refuse("subject", "A subject is written kind:id, such as app:gameserver or realm:1");
        filter.SubjectKind = std::string(subject.substr(0, colon));
        filter.SubjectId = std::string(subject.substr(colon + 1));
    }
    if (std::string_view const result = Ambrose::Trim(request.Query("result")); !result.empty())
    {
        if (result == "ok" || result == "succeeded")
            filter.Result = "succeeded";
        else if (result == "denied" || result == "refused")
            filter.Result = "refused";
        else if (result == "failed" || result == "throttled")
            filter.Result = std::string(result);
        else
            return refuse("result", "The result is ok, denied, failed or throttled");
    }
    filter.Address = std::string(Ambrose::Trim(request.Query("address")));
    if (filter.Address.size() > 64)
        return refuse("address", "An address is at most 64 characters");

    for (auto const& [name, target] : { std::pair<std::string_view, int64*>{ "from", &filter.FromEpochMs }, { "to", &filter.ToEpochMs }, { "cursor", &filter.Before } })
    {
        std::string_view const text = Ambrose::Trim(request.Query(name));
        if (text.empty())
            continue;
        std::optional<int64> const parsed = Number(text);
        if (!parsed || *parsed < 0)
            return refuse(name, fmt::format("{} is a whole number of at least 0", name));
        *target = *parsed;
    }
    if (filter.FromEpochMs != 0 && filter.ToEpochMs != 0 && filter.FromEpochMs > filter.ToEpochMs)
        return refuse("from", "The time range starts after it ends");
    if (std::string_view const limit = Ambrose::Trim(request.Query("limit")); !limit.empty())
    {
        std::optional<int64> const parsed = Number(limit);
        if (!parsed || *parsed < 1 || *parsed > MaxPageRows)
            return refuse("limit", fmt::format("A page holds from 1 to {} rows", MaxPageRows));
        filter.Limit = static_cast<uint32>(*parsed);
    }
    return filter;
}

std::string_view PanelActivity::ResultName(std::string_view stored) noexcept
{
    if (stored == "succeeded")
        return "ok";
    if (stored == "refused")
        return "denied";
    if (stored == "throttled")
        return "throttled";
    return "failed";
}

bool PanelActivity::Read(PanelStore& store, PanelActivityFilter const& filter, PanelActivityPage& page, std::string& error)
{
    page = {};
    error.clear();
    std::string sql =
        "SELECT e.id, e.event_id, e.batch_id, e.created_epoch_ms, e.name, e.actor_type, e.actor_id, e.actor_name, e.address, e.user_agent, e.node, "
        "e.result, e.error, e.reason, e.properties, e.api_key_id FROM audit_event e WHERE 1 = 1";
    std::vector<Bound> values;
    if (!filter.Prefix.empty())
    {
        sql += " AND substr(e.name, 1, ?) = ?";
        values.emplace_back(static_cast<int64>(filter.Prefix.size()));
        values.emplace_back(filter.Prefix);
    }
    if (!filter.ActorId.empty())
    {
        sql += " AND e.actor_type = 'user' AND e.actor_id IN (?, ?)";
        values.emplace_back(filter.ActorId);
        values.emplace_back("user:" + filter.ActorId);
    }
    if (!filter.Involving.empty())
    {
        sql += " AND ((e.actor_type = 'user' AND e.actor_id IN (?, ?)) OR EXISTS (SELECT 1 FROM audit_subject s WHERE s.event = e.id AND s.kind = 'panel_user' AND s.subject_id = ?))";
        values.emplace_back(filter.Involving);
        values.emplace_back("user:" + filter.Involving);
        values.emplace_back(filter.Involving);
    }
    if (!filter.SubjectKind.empty())
    {
        sql += " AND EXISTS (SELECT 1 FROM audit_subject s WHERE s.event = e.id AND s.kind = ? AND s.subject_id = ?)";
        values.emplace_back(filter.SubjectKind);
        values.emplace_back(filter.SubjectId);
    }
    if (!filter.Result.empty())
    {
        sql += " AND e.result = ?";
        values.emplace_back(filter.Result);
    }
    if (!filter.Address.empty())
    {
        sql += " AND e.address = ?";
        values.emplace_back(filter.Address);
    }
    if (filter.FromEpochMs != 0)
    {
        sql += " AND e.created_epoch_ms >= ?";
        values.emplace_back(filter.FromEpochMs);
    }
    if (filter.ToEpochMs != 0)
    {
        sql += " AND e.created_epoch_ms <= ?";
        values.emplace_back(filter.ToEpochMs);
    }
    if (filter.Before != 0)
    {
        sql += " AND e.id < ?";
        values.emplace_back(filter.Before);
    }
    uint32 const limit = std::clamp<uint32>(filter.Limit, 1, MaxExportRows);
    sql += " ORDER BY e.id DESC LIMIT ?";
    values.emplace_back(static_cast<int64>(limit) + 1);

    std::optional<PanelStore::Statement> rows = store.Prepare(sql, error);
    if (!rows)
        return false;
    BindAll(*rows, values);
    while (rows->Step(error))
    {
        if (page.Rows.size() == limit)
        {
            page.NextCursor = page.Rows.back().DatabaseId;
            break;
        }
        AuditEvent& event = page.Rows.emplace_back();
        event.DatabaseId = rows->Int64(0);
        event.EventId = rows->Text(1);
        event.BatchId = TextOr(*rows, 2);
        event.CreatedEpochMs = rows->Int64(3);
        event.Name = rows->Text(4);
        event.Actor = ActorOf(TextOr(*rows, 5));
        event.ActorId = TextOr(*rows, 6);
        event.ActorName = TextOr(*rows, 7);
        event.Address = TextOr(*rows, 8);
        event.UserAgent = TextOr(*rows, 9);
        event.Node = TextOr(*rows, 10);
        event.Result = StoredResult(rows->Text(11));
        event.Error = TextOr(*rows, 12);
        event.Reason = TextOr(*rows, 13);
        event.Properties = rows->Text(14);
        event.ApiKeyId = TextOr(*rows, 15);
    }
    if (!error.empty())
        return false;
    rows.reset();

    std::optional<PanelStore::Statement> subjects = store.Prepare("SELECT kind, subject_id, name FROM audit_subject WHERE event = ? ORDER BY position", error);
    if (!subjects)
        return false;
    for (AuditEvent& event : page.Rows)
    {
        subjects->Reset();
        subjects->Bind(1, event.DatabaseId);
        while (subjects->Step(error))
            event.Subjects.push_back({ subjects->Text(0), TextOr(*subjects, 1), TextOr(*subjects, 2) });
        if (!error.empty())
            return false;
    }
    return true;
}

bool PanelActivity::ShowsAddress(AuditEvent const& event, std::string_view viewerId, bool seesEveryAddress)
{
    if (seesEveryAddress)
        return true;
    return !viewerId.empty() && event.Actor == AuditActor::User && OperatorId(event) == viewerId;
}

nlohmann::json PanelActivity::RowJson(AuditEvent const& event, bool showAddress)
{
    nlohmann::json subjects = nlohmann::json::array();
    for (AuditSubject const& subject : event.Subjects)
        subjects.push_back({ { "kind", subject.Kind }, { "id", subject.Id }, { "name", subject.Name } });
    nlohmann::json properties = nlohmann::json::parse(event.Properties.empty() ? std::string("{}") : event.Properties, nullptr, false);
    if (properties.is_discarded())
        properties = nlohmann::json::object();
    std::string const actorId = OperatorId(event);
    return {
        { "id", event.DatabaseId },
        { "event_id", event.EventId },
        { "batch_id", event.BatchId.empty() ? nlohmann::json(nullptr) : nlohmann::json(event.BatchId) },
        { "epoch_ms", event.CreatedEpochMs },
        { "name", event.Name },
        { "sentence", PanelActivityCatalog::Render(event) },
        { "class", PanelActivityCatalog::ClassName(PanelActivityCatalog::ClassOf(event.Name)) },
        { "actor", {
            { "type", PanelAudit::ToString(event.Actor) },
            { "id", actorId.empty() ? nlohmann::json(nullptr) : nlohmann::json(actorId) },
            { "name", PanelActivityCatalog::ActorText(event) } } },
        { "api_key_id", event.ApiKeyId.empty() ? nlohmann::json(nullptr) : nlohmann::json(event.ApiKeyId) },
        { "scheduled", event.Actor == AuditActor::Schedule },
        { "address_shown", showAddress },
        { "address", showAddress && !event.Address.empty() ? nlohmann::json(event.Address) : nlohmann::json(nullptr) },
        { "user_agent", showAddress && !event.UserAgent.empty() ? nlohmann::json(event.UserAgent) : nlohmann::json(nullptr) },
        { "node", event.Node.empty() ? nlohmann::json(nullptr) : nlohmann::json(event.Node) },
        { "result", ResultName(PanelAudit::ToString(event.Result)) },
        { "error", event.Error.empty() ? nlohmann::json(nullptr) : nlohmann::json(event.Error) },
        { "reason", event.Reason.empty() ? nlohmann::json(nullptr) : nlohmann::json(event.Reason) },
        { "properties", std::move(properties) },
        { "subjects", std::move(subjects) }
    };
}

std::string PanelActivity::CsvField(std::string_view value)
{
    std::string field = "\"";
    if (!value.empty() && (value.front() == '=' || value.front() == '+' || value.front() == '-' || value.front() == '@' || value.front() == '\t' || value.front() == '\r'))
        field += '\'';
    for (char const c : value)
    {
        if (c == '"')
            field += "\"\"";
        else
            field += c;
    }
    field += '"';
    return field;
}

std::string PanelActivity::Csv(std::vector<AuditEvent> const& rows, std::function<bool(AuditEvent const&)> const& showAddress)
{
    std::string csv = "id,epoch_ms,name,sentence,actor,actor_id,api_key_id,address,result,reason,error,subjects\r\n";
    for (AuditEvent const& event : rows)
    {
        std::string subjects;
        for (AuditSubject const& subject : event.Subjects)
            subjects += fmt::format("{}{}:{}", subjects.empty() ? "" : " ", subject.Kind, subject.Id);
        bool const shown = showAddress && showAddress(event);
        csv += fmt::format("{},{},{},{},{},{},{},{},{},{},{},{}\r\n",
            event.DatabaseId, event.CreatedEpochMs, CsvField(event.Name), CsvField(PanelActivityCatalog::Render(event)), CsvField(PanelActivityCatalog::ActorText(event)),
            CsvField(OperatorId(event)), CsvField(event.ApiKeyId), CsvField(shown ? event.Address : std::string()), CsvField(ResultName(PanelAudit::ToString(event.Result))),
            CsvField(event.Reason), CsvField(event.Error), CsvField(subjects));
    }
    return csv;
}

bool PanelActivity::Sweep(PanelStore& store, int64 nowEpochMs, int64 securityDays, int64 highVolumeDays, PanelActivitySweep& removed, std::string& error)
{
    removed = {};
    constexpr int64 DayMs = 86400000;
    int64 const securityCutoff = nowEpochMs - securityDays * DayMs;
    int64 const highVolumeCutoff = nowEpochMs - highVolumeDays * DayMs;
    std::string const highVolume = NameList(PanelActivityCatalog::NamesOf(PanelActivityClass::HighVolume));

    std::optional<int64> const security = CountOf(store,
        fmt::format("SELECT COUNT(*) FROM audit_event e WHERE e.name NOT IN ({}) AND e.created_epoch_ms < {}", highVolume, securityCutoff), error);
    if (!security)
        return false;
    std::optional<int64> const high = CountOf(store,
        fmt::format("SELECT COUNT(*) FROM audit_event e WHERE e.name IN ({}) AND e.created_epoch_ms < {}", highVolume, highVolumeCutoff), error);
    if (!high)
        return false;
    removed.Security = *security;
    removed.HighVolume = *high;
    if (removed.Security + removed.HighVolume == 0)
        return true;

    std::string const expiredRow = Expired("d", highVolume, securityCutoff, highVolumeCutoff);
    std::string const expiredNext = Expired("n", highVolume, securityCutoff, highVolumeCutoff);
    std::optional<PanelStore::Statement> anchor = store.Prepare(fmt::format(
        "INSERT OR IGNORE INTO audit_pruned (id, chain_hash, removed_epoch_ms) SELECT d.id, d.chain_hash, ? FROM audit_event d WHERE {} "
        "AND EXISTS (SELECT 1 FROM audit_event n WHERE n.id = (SELECT MIN(m.id) FROM audit_event m WHERE m.id > d.id) AND NOT {})",
        expiredRow, expiredNext), error);
    if (!anchor)
        return false;
    anchor->Bind(1, nowEpochMs);
    if (!anchor->Run(error))
        return false;
    removed.Anchored = store.Changed();
    anchor.reset();

    std::optional<PanelStore::Statement> sweep = store.Prepare(fmt::format("DELETE FROM audit_event WHERE id IN (SELECT d.id FROM audit_event d WHERE {})", expiredRow), error);
    if (!sweep)
        return false;
    return sweep->Run(error);
}
