/*
 * Project Ambrose by Imjustchico
 * The database half of the admin API: each database's state, address and pool use on GET /api/database, its applied and pending update files on GET /api/database/updates with every pending file marked data-only or restart-required and why, a live apply of the pending data-only files on POST /api/database/apply that reloads the stores reading that database, and POST /api/database/reload for those stores alone.
 */

#ifndef AMBROSE_ADMINDATABASEVIEW_H
#define AMBROSE_ADMINDATABASEVIEW_H

#include "DatabaseLoader.h"

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class AdminRouter;
struct AdminResponse;

struct AdminStoreReload
{
    bool Loaded = false;
    std::vector<std::string> Errors;
    std::vector<std::string> Warnings;
};

class AdminDatabaseView
{
public:
    using StoreReload = std::function<AdminStoreReload()>;

    struct StoreResult
    {
        std::string Name;
        AdminStoreReload Result;
    };

    static constexpr int SchemaVersion = 1;

    explicit AdminDatabaseView(DatabaseLoader& loader);

    AdminDatabaseView(AdminDatabaseView const&) = delete;
    AdminDatabaseView& operator=(AdminDatabaseView const&) = delete;

    void AddStore(std::string name, std::string database, StoreReload reload);
    void Register(AdminRouter& router);

    std::vector<StoreResult> ReloadStores(std::string_view database) const;
    std::vector<std::string> GetStores(std::string_view database) const;

    std::string StatusJson(std::vector<DatabaseStatus> const& statuses) const;
    static std::string UpdatesJson(std::vector<DatabaseUpdates> const& databases);
    static std::string ApplyJson(std::string_view database, UpdateSummary const& summary, std::vector<StoreResult> const& stores);
    static std::string ReloadJson(std::string_view database, std::vector<StoreResult> const& stores);
    static std::string_view KindName(UpdateKind kind) noexcept;
    static std::string StateName(UpdateState state);

private:
    struct Store
    {
        std::string Name;
        std::string Database;
        StoreReload Reload;
    };

    AdminResponse Apply(std::string const& body);
    AdminResponse Reload(std::string const& body) const;
    bool ReadDatabase(std::string const& body, std::string& database, AdminResponse& refusal) const;

    DatabaseLoader& _loader;
    mutable std::mutex _mutex;
    std::vector<Store> _stores;
};

#endif
