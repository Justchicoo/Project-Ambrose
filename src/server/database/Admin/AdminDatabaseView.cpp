/*
 * Project Ambrose by Imjustchico
 * Answers the database routes from the loader: a pending file is restart-required when it can change the schema or waits behind one that can, a request names a database this app opens or gets a 422 listing the ones it has, a refused apply answers 409 with the reason, and an apply that changed anything reloads each store registered for that database and reports every store's errors and warnings.
 */

#include "AdminDatabaseView.h"
#include "AdminRouter.h"
#include "ConfigMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <utility>

namespace
{
    nlohmann::json TextOrNull(std::string const& text)
    {
        return text.empty() ? nlohmann::json(nullptr) : nlohmann::json(text);
    }

    nlohmann::json StoresJson(std::vector<AdminDatabaseView::StoreResult> const& stores)
    {
        nlohmann::json list = nlohmann::json::array();
        for (AdminDatabaseView::StoreResult const& store : stores)
            list.push_back({ { "name", store.Name }, { "loaded", store.Result.Loaded }, { "errors", store.Result.Errors }, { "warnings", store.Result.Warnings } });
        return list;
    }
}

AdminDatabaseView::AdminDatabaseView(DatabaseLoader& loader) : _loader(loader)
{
}

std::string_view AdminDatabaseView::KindName(UpdateKind kind) noexcept
{
    return kind == UpdateKind::Schema ? "schema" : "data";
}

std::string AdminDatabaseView::StateName(UpdateState state)
{
    return Ambrose::ToLower(UpdateFetcher::ToString(state));
}

void AdminDatabaseView::AddStore(std::string name, std::string database, StoreReload reload)
{
    std::lock_guard const lock(_mutex);
    _stores.push_back(Store{ std::move(name), std::move(database), std::move(reload) });
}

std::vector<std::string> AdminDatabaseView::GetStores(std::string_view database) const
{
    std::vector<std::string> names;
    std::lock_guard const lock(_mutex);
    for (Store const& store : _stores)
        if (store.Database == database)
            names.push_back(store.Name);
    return names;
}

std::vector<AdminDatabaseView::StoreResult> AdminDatabaseView::ReloadStores(std::string_view database) const
{
    std::vector<Store> stores;
    {
        std::lock_guard const lock(_mutex);
        for (Store const& store : _stores)
            if (store.Database == database)
                stores.push_back(store);
    }
    std::vector<StoreResult> results;
    for (Store const& store : stores)
        results.push_back(StoreResult{ store.Name, store.Reload ? store.Reload() : AdminStoreReload{} });
    return results;
}

std::string AdminDatabaseView::StatusJson(std::vector<DatabaseStatus> const& statuses) const
{
    nlohmann::json databases = nlohmann::json::array();
    for (DatabaseStatus const& status : statuses)
    {
        nlohmann::json entry;
        entry["name"] = status.Name;
        entry["key"] = status.Key;
        entry["state"] = std::string(DatabaseLoader::StateName(status.State));
        entry["applying"] = status.Applying;
        entry["updates_enabled"] = status.UpdatesEnabled;
        entry["update_flag"] = status.UpdateFlag;
        entry["address"] = TextOrNull(status.Address);
        entry["pool"] = {
            { "async_connections", status.Use.AsyncConnections },
            { "sync_connections", status.Use.SyncConnections },
            { "async_active", status.Use.AsyncActive },
            { "sync_leased", status.Use.SyncLeased },
            { "sync_waiting", status.Use.SyncWaiting },
            { "queued", status.Use.Queued },
            { "reconnects", status.Use.Reconnects }
        };
        entry["stores"] = GetStores(status.Name);
        databases.push_back(std::move(entry));
    }
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["databases"] = std::move(databases);
    return body.dump();
}

std::string AdminDatabaseView::UpdatesJson(std::vector<DatabaseUpdates> const& databases)
{
    nlohmann::json list = nlohmann::json::array();
    for (DatabaseUpdates const& database : databases)
    {
        nlohmann::json applied = nlohmann::json::array();
        for (AppliedUpdate const& update : database.Report.Applied)
            applied.push_back({
                { "name", update.Name },
                { "state", StateName(update.State) },
                { "hash", update.Hash },
                { "applied_epoch_ms", update.AppliedAt * 1000 },
                { "took_ms", update.Milliseconds },
                { "present", update.Present },
                { "changed", update.Changed }
            });

        nlohmann::json pending = nlohmann::json::array();
        std::string blocker;
        for (PendingUpdate const& update : database.Report.Pending)
        {
            bool const renamed = !update.RenamedFrom.empty();
            bool const schema = !renamed && update.Classification.Kind == UpdateKind::Schema;
            nlohmann::json item;
            item["name"] = update.File.Name;
            item["state"] = StateName(update.File.State);
            item["file"] = ConfigMgr::PathToUtf8(update.File.Path);
            item["hash"] = update.Hash;
            item["kind"] = renamed ? std::string("rename") : std::string(KindName(update.Classification.Kind));
            item["transactional"] = !renamed && update.Classification.Transactional;
            item["line"] = update.Classification.Line == 0 ? nlohmann::json(nullptr) : nlohmann::json(update.Classification.Line);
            item["statement"] = TextOrNull(update.Classification.Statement);
            item["problem"] = TextOrNull(update.Classification.Problem);
            item["renamed_from"] = TextOrNull(update.RenamedFrom);
            item["restart_required"] = schema || !blocker.empty();
            item["waits_for"] = TextOrNull(blocker);
            if (schema && blocker.empty())
                blocker = update.File.Name;
            pending.push_back(std::move(item));
        }

        nlohmann::json entry;
        entry["name"] = database.Name;
        entry["listed"] = database.Listed;
        entry["error"] = TextOrNull(database.Error);
        entry["applied"] = std::move(applied);
        entry["pending"] = std::move(pending);
        list.push_back(std::move(entry));
    }
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["databases"] = std::move(list);
    return body.dump();
}

std::string AdminDatabaseView::ApplyJson(std::string_view database, UpdateSummary const& summary, std::vector<StoreResult> const& stores)
{
    nlohmann::json body;
    body["database"] = std::string(database);
    body["succeeded"] = summary.Succeeded;
    body["applied"] = summary.AppliedNames;
    body["stopped_at"] = TextOrNull(summary.StoppedAt);
    body["failed_at"] = TextOrNull(summary.FailedAt);
    body["failure"] = TextOrNull(summary.Failure);
    body["stores"] = StoresJson(stores);
    return body.dump();
}

std::string AdminDatabaseView::ReloadJson(std::string_view database, std::vector<StoreResult> const& stores)
{
    nlohmann::json body;
    body["database"] = std::string(database);
    body["stores"] = StoresJson(stores);
    return body.dump();
}

bool AdminDatabaseView::ReadDatabase(std::string const& body, std::string& database, AdminResponse& refusal) const
{
    std::vector<std::string> const names = _loader.GetNames();
    std::string const choices = fmt::format("{}", fmt::join(names, ", "));
    nlohmann::json const parsed = nlohmann::json::parse(body, nullptr, false);
    if (!parsed.is_object())
    {
        refusal = AdminResponse::Invalid("The request takes a JSON object that names a database", { { "database", fmt::format("Name one of {}", choices) } });
        return false;
    }
    std::vector<std::pair<std::string, std::string>> fields;
    for (auto const& [key, value] : parsed.items())
        if (key != "database")
            fields.emplace_back(key, "The request takes only database");
    auto const named = parsed.find("database");
    if (named == parsed.end() || !named->is_string())
        fields.emplace_back("database", fmt::format("Name one of {}", choices));
    else
    {
        database = named->get<std::string>();
        if (std::ranges::find(names, database) == names.end())
            fields.emplace_back("database", fmt::format("This app has no {} database; name one of {}", database, choices));
    }
    if (fields.empty())
        return true;
    refusal = AdminResponse::Invalid("The request does not name a database this app opens", std::move(fields));
    return false;
}

AdminResponse AdminDatabaseView::Apply(std::string const& body)
{
    std::string database;
    AdminResponse refusal;
    if (!ReadDatabase(body, database, refusal))
        return refusal;
    DatabaseApplyResult const result = _loader.ApplyDataUpdates(database);
    switch (result.Refusal)
    {
        case DatabaseApplyRefusal::None: break;
        case DatabaseApplyRefusal::Unknown: return AdminResponse::Invalid("The request does not name a database this app opens", { { "database", result.Message } });
        case DatabaseApplyRefusal::Busy: return AdminResponse::Problem(409, "updater_busy", result.Message);
        case DatabaseApplyRefusal::NotOpen: return AdminResponse::Problem(409, "database_not_open", result.Message);
        case DatabaseApplyRefusal::UpdatesDisabled: return AdminResponse::Problem(409, "updates_disabled", result.Message);
        case DatabaseApplyRefusal::BadConnection: return AdminResponse::Problem(409, "bad_connection", result.Message);
    }
    std::vector<StoreResult> const stores = result.Summary.Applied != 0 ? ReloadStores(database) : std::vector<StoreResult>{};
    return AdminResponse::Json(200, ApplyJson(database, result.Summary, stores));
}

AdminResponse AdminDatabaseView::Reload(std::string const& body) const
{
    std::string database;
    AdminResponse refusal;
    if (!ReadDatabase(body, database, refusal))
        return refusal;
    return AdminResponse::Json(200, ReloadJson(database, ReloadStores(database)));
}

void AdminDatabaseView::Register(AdminRouter& router)
{
    router.AddOpen("GET", "/api/database", [this](AdminRequest const&) { return AdminResponse::Json(200, StatusJson(_loader.GetStatus())); });
    router.AddOpen("GET", "/api/database/updates", [this](AdminRequest const&)
    {
        std::vector<DatabaseUpdates> databases;
        for (std::string const& name : _loader.GetNames())
            databases.push_back(_loader.InspectUpdates(name));
        return AdminResponse::Json(200, UpdatesJson(databases));
    });
    router.AddOpen("POST", "/api/database/apply", [this](AdminRequest const& request) { return Apply(request.Body); });
    router.AddOpen("POST", "/api/database/reload", [this](AdminRequest const& request) { return Reload(request.Body); });
}
