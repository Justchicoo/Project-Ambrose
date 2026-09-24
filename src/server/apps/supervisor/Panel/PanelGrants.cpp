/*
 * Project Ambrose by Imjustchico
 * Reading and writing grants. A grant naming a permission the catalog does not hold is refused when it is given rather than ignored when it is read, because a grant nobody can act on is a promise somebody thinks they made.
 */

#include "PanelGrants.h"

#include "PanelPermissions.h"
#include "PanelStore.h"

PanelGrants::PanelGrants(PanelStore& store) : _store(store)
{
}

bool PanelGrants::Give(int64 userId, std::string_view app, std::string_view permission, int64 byUserId, int64 whenEpochMs, std::string& error)
{
    if (!PanelPermissions::Holds(permission))
    {
        error = "there is no permission called " + std::string(permission);
        return false;
    }
    if (app.empty())
    {
        error = "a grant names the app it is for";
        return false;
    }
    if (byUserId == userId)
    {
        error = "nobody gives themselves a grant";
        return false;
    }
    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT OR IGNORE INTO panel_grant (user_id, app, permission, granted_epoch_ms, granted_by) VALUES (?1, ?2, ?3, ?4, ?5)", error);
    if (!insert)
        return false;
    insert->Bind(1, userId);
    insert->Bind(2, app);
    insert->Bind(3, permission);
    insert->Bind(4, whenEpochMs);
    if (byUserId > 0)
        insert->Bind(5, byUserId);
    else
        insert->BindNull(5);
    if (!insert->Run(error))
        return false;
    return _store.Changed() == 0 || Bump(userId, error);
}

bool PanelGrants::Take(int64 userId, std::string_view app, std::string_view permission, int64 byUserId, std::string& error)
{
    if (byUserId == userId)
    {
        error = "nobody takes away their own grant";
        return false;
    }
    std::optional<PanelStore::Statement> remove = _store.Prepare("DELETE FROM panel_grant WHERE user_id = ?1 AND app = ?2 AND permission = ?3", error);
    if (!remove)
        return false;
    remove->Bind(1, userId);
    remove->Bind(2, app);
    remove->Bind(3, permission);
    if (!remove->Run(error))
        return false;
    return _store.Changed() == 0 || Bump(userId, error);
}

bool PanelGrants::TakeAll(int64 userId, std::string& error)
{
    std::optional<PanelStore::Statement> remove = _store.Prepare("DELETE FROM panel_grant WHERE user_id = ?1", error);
    if (!remove)
        return false;
    remove->Bind(1, userId);
    if (!remove->Run(error))
        return false;
    return _store.Changed() == 0 || Bump(userId, error);
}

bool PanelGrants::Bump(int64 userId, std::string& error)
{
    std::optional<PanelStore::Statement> bump = _store.Prepare("UPDATE panel_user SET generation = generation + 1 WHERE id = ?1", error);
    if (!bump)
        return false;
    bump->Bind(1, userId);
    return bump->Run(error);
}

std::vector<PanelGrant> PanelGrants::Of(int64 userId, std::string& error) const
{
    std::vector<PanelGrant> grants;
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT id, user_id, app, permission, granted_epoch_ms FROM panel_grant WHERE user_id = ?1 ORDER BY app, permission", error);
    if (!rows)
        return grants;
    rows->Bind(1, userId);
    while (rows->Step(error))
    {
        PanelGrant grant;
        grant.Id = rows->Int64(0);
        grant.UserId = rows->Int64(1);
        grant.App = rows->Text(2);
        grant.Permission = rows->Text(3);
        grant.GrantedEpochMs = rows->Int64(4);
        grants.push_back(std::move(grant));
    }
    if (!error.empty())
        grants.clear();
    return grants;
}

bool PanelGrants::Holds(int64 userId, std::string_view app, std::string_view permission, std::string& error) const
{
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT 1 FROM panel_grant WHERE user_id = ?1 AND app = ?2 AND permission = ?3", error);
    if (!rows)
        return false;
    rows->Bind(1, userId);
    rows->Bind(2, app);
    rows->Bind(3, permission);
    return rows->Step(error);
}

std::set<std::string, std::less<>> PanelGrants::AppsOf(int64 userId, std::string& error) const
{
    std::set<std::string, std::less<>> apps;
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT DISTINCT app FROM panel_grant WHERE user_id = ?1", error);
    if (!rows)
        return apps;
    rows->Bind(1, userId);
    while (rows->Step(error))
        apps.insert(rows->Text(0));
    return apps;
}
