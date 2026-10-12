/*
 * Project Ambrose by Imjustchico
 * Roles as rows built from the permission catalog rather than bundles compiled into the binary. The five shipped roles are seeded as rows like any other; an owner adds or edits custom roles from the roles page. A change builds a new resolution table, swaps it in under a lock, and the caller bumps the session generation of every affected user so the new keys apply at once. Owner-only keys stay owner-only whatever a role names: that rule lives at decision time in PanelAuthorization, not in the row.
 */

#include "PanelRoles.h"

#include "PanelPermissions.h"
#include "PanelStore.h"
#include "StringUtil.h"

namespace
{
    constexpr std::size_t MaxName = 48;

    bool NameOk(std::string_view name)
    {
        if (name.size() < 2 || name.size() > MaxName)
            return false;
        for (char c : name)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_'))
                return false;
        return true;
    }
}

PanelRoles::PanelRoles(PanelStore& store) : _store(store)
{
}

std::string_view PanelRoles::Explain(PanelRoleResult result) noexcept
{
    switch (result)
    {
        case PanelRoleResult::Ok: return "done";
        case PanelRoleResult::NameTooShort: return "the name needs at least two characters";
        case PanelRoleResult::NameTooLong: return "the name is too long";
        case PanelRoleResult::NameInvalid: return "the name holds a character it may not";
        case PanelRoleResult::NameTaken: return "that name is already taken";
        case PanelRoleResult::UnknownRole: return "there is no such role";
        case PanelRoleResult::UnknownUser: return "there is no such user";
        case PanelRoleResult::ShippedRole: return "a shipped role cannot be removed";
        case PanelRoleResult::KeyUnknown: return "the catalog holds no such permission";
        case PanelRoleResult::KeyNotHeld: return "the editor does not hold that permission";
        case PanelRoleResult::KeyOwnerOnly: return "that permission stays owner-only";
        case PanelRoleResult::LastOwnerRole: return "the last owner cannot lose the owner role";
        case PanelRoleResult::StoreFailed: return "the store refused the write";
    }
    return "refused";
}

bool PanelRoles::SeedShipped(std::string& error)
{
    for (PanelRole role : PanelPermissions::Roles())
    {
        std::string_view const name = PanelPermissions::NameOf(role);
        std::optional<PanelRoleRow> const existing = FindByName(name, error);
        if (!error.empty())
            return false;
        if (existing)
            continue;
        std::optional<PanelStore::Statement> insert = _store.Prepare(
            "INSERT INTO panel_role (name, description, owner_only, shipped, created_epoch_ms) VALUES (?1, ?2, ?3, 1, ?4)", error);
        if (!insert)
            return false;
        insert->Bind(1, name);
        insert->Bind(2, std::string("Shipped with the panel"));
        insert->Bind(3, role == PanelRole::Owner ? 1 : 0);
        insert->Bind(4, PanelStore::NowEpochMs());
        if (!insert->Run(error))
            return false;
        int64 const id = _store.LastInsertId();
        if (id <= 0)
            return false;
        std::vector<std::string> keys;
        for (std::string_view key : PanelPermissions::KeysOf(role))
            keys.emplace_back(key);
        if (!WriteKeys(id, keys, error))
            return false;
    }
    Rebuild(error);
    return error.empty();
}

std::vector<PanelRoleRow> PanelRoles::All(std::string& error)
{
    std::vector<PanelRoleRow> rows;
    std::optional<PanelStore::Statement> query = _store.Prepare(
        "SELECT id, name, description, owner_only, shipped, created_epoch_ms FROM panel_role ORDER BY shipped DESC, name", error);
    if (!query)
        return rows;
    while (query->Step(error))
        rows.push_back(Read(*query));
    if (!error.empty())
        rows.clear();
    for (PanelRoleRow& row : rows)
    {
        std::optional<PanelStore::Statement> keys = _store.Prepare("SELECT permission FROM panel_role_key WHERE role_id = ?1 ORDER BY permission", error);
        if (!keys)
        {
            rows.clear();
            return rows;
        }
        keys->Bind(1, row.Id);
        while (keys->Step(error))
            row.Keys.insert(keys->Text(0));
        if (!error.empty())
        {
            rows.clear();
            return rows;
        }
    }
    return rows;
}

std::optional<PanelRoleRow> PanelRoles::Find(int64 id, std::string& error)
{
    std::optional<PanelStore::Statement> query = _store.Prepare(
        "SELECT id, name, description, owner_only, shipped, created_epoch_ms FROM panel_role WHERE id = ?1", error);
    if (!query)
        return std::nullopt;
    query->Bind(1, id);
    if (!query->Step(error))
        return std::nullopt;
    PanelRoleRow row = Read(*query);
    std::optional<PanelStore::Statement> keys = _store.Prepare("SELECT permission FROM panel_role_key WHERE role_id = ?1 ORDER BY permission", error);
    if (!keys)
        return std::nullopt;
    keys->Bind(1, id);
    while (keys->Step(error))
        row.Keys.insert(keys->Text(0));
    if (!error.empty())
        return std::nullopt;
    return row;
}

std::optional<PanelRoleRow> PanelRoles::FindByName(std::string_view name, std::string& error)
{
    std::optional<PanelStore::Statement> query = _store.Prepare(
        "SELECT id, name, description, owner_only, shipped, created_epoch_ms FROM panel_role WHERE name = ?1 COLLATE NOCASE", error);
    if (!query)
        return std::nullopt;
    query->Bind(1, name);
    if (!query->Step(error))
        return std::nullopt;
    PanelRoleRow row = Read(*query);
    std::optional<PanelStore::Statement> keys = _store.Prepare("SELECT permission FROM panel_role_key WHERE role_id = ?1 ORDER BY permission", error);
    if (!keys)
        return std::nullopt;
    keys->Bind(1, row.Id);
    while (keys->Step(error))
        row.Keys.insert(keys->Text(0));
    if (!error.empty())
        return std::nullopt;
    return row;
}

PanelRoleResult PanelRoles::Create(std::string_view name, std::string_view description, std::vector<std::string> const& keys, int64 byUserId, std::set<std::string, std::less<>> const& editorKeys, int64* id, std::string& error)
{
    if (name.size() < 2)
        return PanelRoleResult::NameTooShort;
    if (name.size() > MaxName)
        return PanelRoleResult::NameTooLong;
    if (!NameOk(name))
        return PanelRoleResult::NameInvalid;
    if (FindByName(name, error))
        return PanelRoleResult::NameTaken;
    if (!error.empty())
        return PanelRoleResult::StoreFailed;
    for (std::string const& key : keys)
    {
        PanelPermission const* const held = PanelPermissions::Find(key);
        if (!held)
            return PanelRoleResult::KeyUnknown;
        if (held->OwnerOnly)
            return PanelRoleResult::KeyOwnerOnly;
        if (!editorKeys.contains(key))
            return PanelRoleResult::KeyNotHeld;
    }
    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT INTO panel_role (name, description, owner_only, shipped, created_epoch_ms) VALUES (?1, ?2, 0, 0, ?3)", error);
    if (!insert)
        return PanelRoleResult::StoreFailed;
    insert->Bind(1, name);
    insert->Bind(2, Ambrose::Trim(description).substr(0, 200));
    insert->Bind(3, PanelStore::NowEpochMs());
    if (!insert->Run(error))
        return PanelRoleResult::StoreFailed;
    int64 const roleId = _store.LastInsertId();
    if (roleId <= 0)
        return PanelRoleResult::StoreFailed;
    if (!WriteKeys(roleId, keys, error))
        return PanelRoleResult::StoreFailed;
    (void)byUserId;
    Rebuild(error);
    if (!error.empty())
        return PanelRoleResult::StoreFailed;
    if (id)
        *id = roleId;
    return PanelRoleResult::Ok;
}

PanelRoleResult PanelRoles::Update(int64 id, std::string_view name, std::string_view description, std::vector<std::string> const& keys, int64 byUserId, std::set<std::string, std::less<>> const& editorKeys, std::string& error)
{
    std::optional<PanelRoleRow> const row = Find(id, error);
    if (!error.empty())
        return PanelRoleResult::StoreFailed;
    if (!row)
        return PanelRoleResult::UnknownRole;
    if (name.size() < 2)
        return PanelRoleResult::NameTooShort;
    if (name.size() > MaxName)
        return PanelRoleResult::NameTooLong;
    if (!NameOk(name))
        return PanelRoleResult::NameInvalid;
    std::optional<PanelRoleRow> const clash = FindByName(name, error);
    if (!error.empty())
        return PanelRoleResult::StoreFailed;
    if (clash && clash->Id != id)
        return PanelRoleResult::NameTaken;
    for (std::string const& key : keys)
    {
        PanelPermission const* const held = PanelPermissions::Find(key);
        if (!held)
            return PanelRoleResult::KeyUnknown;
        if (held->OwnerOnly && !row->OwnerOnly)
            return PanelRoleResult::KeyOwnerOnly;
        if (!editorKeys.contains(key))
            return PanelRoleResult::KeyNotHeld;
    }
    std::optional<PanelStore::Statement> update = _store.Prepare("UPDATE panel_role SET name = ?1, description = ?2 WHERE id = ?3", error);
    if (!update)
        return PanelRoleResult::StoreFailed;
    update->Bind(1, name);
    update->Bind(2, Ambrose::Trim(description).substr(0, 200));
    update->Bind(3, id);
    if (!update->Run(error))
        return PanelRoleResult::StoreFailed;
    if (!WriteKeys(id, keys, error))
        return PanelRoleResult::StoreFailed;
    (void)byUserId;
    Rebuild(error);
    if (!error.empty())
        return PanelRoleResult::StoreFailed;
    return PanelRoleResult::Ok;
}

PanelRoleResult PanelRoles::Remove(int64 id, std::string& error)
{
    std::optional<PanelRoleRow> const row = Find(id, error);
    if (!error.empty())
        return PanelRoleResult::StoreFailed;
    if (!row)
        return PanelRoleResult::UnknownRole;
    if (row->Shipped)
        return PanelRoleResult::ShippedRole;
    std::vector<int64> const holders = HoldersOf(id, error);
    if (!error.empty())
        return PanelRoleResult::StoreFailed;
    std::optional<PanelStore::Statement> remove = _store.Prepare("DELETE FROM panel_role WHERE id = ?1", error);
    if (!remove)
        return PanelRoleResult::StoreFailed;
    remove->Bind(1, id);
    if (!remove->Run(error))
        return PanelRoleResult::StoreFailed;
    Rebuild(error);
    if (!error.empty())
        return PanelRoleResult::StoreFailed;
    (void)holders;
    return PanelRoleResult::Ok;
}

PanelRoleResult PanelRoles::Assign(int64 userId, int64 roleId, int64 whenEpochMs, std::string& error)
{
    std::optional<PanelStore::Statement> user = _store.Prepare("SELECT id FROM panel_user WHERE id = ?1", error);
    if (!user)
        return PanelRoleResult::StoreFailed;
    user->Bind(1, userId);
    if (!user->Step(error))
        return PanelRoleResult::UnknownUser;
    if (!Find(roleId, error))
    {
        if (!error.empty())
            return PanelRoleResult::StoreFailed;
        return PanelRoleResult::UnknownRole;
    }
    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT OR IGNORE INTO panel_user_role (user_id, role_id, granted_epoch_ms) VALUES (?1, ?2, ?3)", error);
    if (!insert)
        return PanelRoleResult::StoreFailed;
    insert->Bind(1, userId);
    insert->Bind(2, roleId);
    insert->Bind(3, whenEpochMs);
    if (!insert->Run(error))
        return PanelRoleResult::StoreFailed;
    return PanelRoleResult::Ok;
}

PanelRoleResult PanelRoles::Unassign(int64 userId, int64 roleId, std::string& error)
{
    std::optional<PanelStore::Statement> remove = _store.Prepare("DELETE FROM panel_user_role WHERE user_id = ?1 AND role_id = ?2", error);
    if (!remove)
        return PanelRoleResult::StoreFailed;
    remove->Bind(1, userId);
    remove->Bind(2, roleId);
    if (!remove->Run(error))
        return PanelRoleResult::StoreFailed;
    return PanelRoleResult::Ok;
}

std::vector<PanelRoleRow> PanelRoles::RolesOf(int64 userId, std::string& error)
{
    std::vector<PanelRoleRow> rows;
    std::optional<PanelStore::Statement> query = _store.Prepare(
        "SELECT r.id, r.name, r.description, r.owner_only, r.shipped, r.created_epoch_ms FROM panel_role r"
        " JOIN panel_user_role ur ON ur.role_id = r.id WHERE ur.user_id = ?1 ORDER BY r.name",
        error);
    if (!query)
        return rows;
    query->Bind(1, userId);
    while (query->Step(error))
        rows.push_back(Read(*query));
    if (!error.empty())
        rows.clear();
    for (PanelRoleRow& row : rows)
    {
        std::optional<PanelStore::Statement> keys = _store.Prepare("SELECT permission FROM panel_role_key WHERE role_id = ?1 ORDER BY permission", error);
        if (!keys)
        {
            rows.clear();
            return rows;
        }
        keys->Bind(1, row.Id);
        while (keys->Step(error))
            row.Keys.insert(keys->Text(0));
        if (!error.empty())
        {
            rows.clear();
            return rows;
        }
    }
    return rows;
}

std::vector<int64> PanelRoles::HoldersOf(int64 roleId, std::string& error)
{
    std::vector<int64> ids;
    std::optional<PanelStore::Statement> query = _store.Prepare("SELECT user_id FROM panel_user_role WHERE role_id = ?1", error);
    if (!query)
        return ids;
    query->Bind(1, roleId);
    while (query->Step(error))
        ids.push_back(query->Int64(0));
    if (!error.empty())
        ids.clear();
    return ids;
}

std::set<std::string, std::less<>> PanelRoles::KeysOfUser(int64 userId, std::string& error)
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::set<std::string, std::less<>> keys;
    std::optional<PanelStore::Statement> query = _store.Prepare("SELECT role_id FROM panel_user_role WHERE user_id = ?1", error);
    if (!query)
        return keys;
    query->Bind(1, userId);
    while (query->Step(error))
    {
        auto const found = _resolution.find(query->Int64(0));
        if (found != _resolution.end())
            keys.insert(found->second.begin(), found->second.end());
    }
    if (!error.empty())
        keys.clear();
    return keys;
}

bool PanelRoles::UserHolds(int64 userId, std::string_view key, std::string& error)
{
    return KeysOfUser(userId, error).contains(key) && error.empty();
}

void PanelRoles::Rebuild(std::string& error)
{
    std::map<int64, std::set<std::string, std::less<>>> fresh;
    std::optional<PanelStore::Statement> roles = _store.Prepare("SELECT id FROM panel_role", error);
    if (!roles)
        return;
    while (roles->Step(error))
    {
        int64 const roleId = roles->Int64(0);
        std::optional<PanelStore::Statement> keys = _store.Prepare("SELECT permission FROM panel_role_key WHERE role_id = ?1", error);
        if (!keys)
            return;
        keys->Bind(1, roleId);
        while (keys->Step(error))
            fresh[roleId].insert(keys->Text(0));
        if (!error.empty())
            return;
    }
    if (!error.empty())
        return;
    std::lock_guard<std::mutex> lock(_mutex);
    _resolution.swap(fresh);
}

bool PanelRoles::WriteKeys(int64 roleId, std::vector<std::string> const& keys, std::string& error)
{
    std::optional<PanelStore::Statement> clear = _store.Prepare("DELETE FROM panel_role_key WHERE role_id = ?1", error);
    if (!clear)
        return false;
    clear->Bind(1, roleId);
    if (!clear->Run(error))
        return false;
    for (std::string const& key : keys)
    {
        std::optional<PanelStore::Statement> insert = _store.Prepare("INSERT OR IGNORE INTO panel_role_key (role_id, permission) VALUES (?1, ?2)", error);
        if (!insert)
            return false;
        insert->Bind(1, roleId);
        insert->Bind(2, key);
        if (!insert->Run(error))
            return false;
    }
    return true;
}

PanelRoleRow PanelRoles::Read(PanelStore::Statement const& row)
{
    PanelRoleRow role;
    role.Id = row.Int64(0);
    role.Name = row.Text(1);
    role.Description = row.Text(2);
    role.OwnerOnly = row.Int64(3) != 0;
    role.Shipped = row.Int64(4) != 0;
    role.CreatedEpochMs = row.Int64(5);
    return role;
}
