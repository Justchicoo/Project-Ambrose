/*
 * Project Ambrose by Imjustchico
 * Roles as rows built from the permission catalog rather than bundles compiled into the binary. The five shipped roles are seeded as rows like any other; an owner adds or edits custom roles from the roles page. A change builds a new resolution table, swaps it in under a lock, and the caller bumps the session generation of every affected user so the new keys apply at once. Owner-only keys stay owner-only whatever a role names: that rule lives at decision time in PanelAuthorization, not in the row.
 */

#ifndef AMBROSE_PANELROLES_H
#define AMBROSE_PANELROLES_H

#include "Types.h"

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class PanelStore;

struct PanelRoleRow
{
    int64 Id = 0;
    std::string Name;
    std::string Description;
    bool OwnerOnly = false;
    bool Shipped = false;
    int64 CreatedEpochMs = 0;
    std::set<std::string, std::less<>> Keys;
};

enum class PanelRoleResult : uint8
{
    Ok,
    NameTooShort,
    NameTooLong,
    NameInvalid,
    NameTaken,
    UnknownRole,
    UnknownUser,
    ShippedRole,
    KeyUnknown,
    KeyNotHeld,
    KeyOwnerOnly,
    LastOwnerRole,
    StoreFailed
};

class PanelRoles
{
public:
    explicit PanelRoles(PanelStore& store);

    PanelRoles(PanelRoles const&) = delete;
    PanelRoles& operator=(PanelRoles const&) = delete;

    static std::string_view Explain(PanelRoleResult result) noexcept;

    bool SeedShipped(std::string& error);

    std::vector<PanelRoleRow> All(std::string& error);
    std::optional<PanelRoleRow> Find(int64 id, std::string& error);
    std::optional<PanelRoleRow> FindByName(std::string_view name, std::string& error);

    PanelRoleResult Create(std::string_view name, std::string_view description, std::vector<std::string> const& keys, int64 byUserId, std::set<std::string, std::less<>> const& editorKeys, int64* id, std::string& error);
    PanelRoleResult Update(int64 id, std::string_view name, std::string_view description, std::vector<std::string> const& keys, int64 byUserId, std::set<std::string, std::less<>> const& editorKeys, std::string& error);
    PanelRoleResult Remove(int64 id, std::string& error);

    PanelRoleResult Assign(int64 userId, int64 roleId, int64 whenEpochMs, std::string& error);
    PanelRoleResult Unassign(int64 userId, int64 roleId, std::string& error);
    std::vector<PanelRoleRow> RolesOf(int64 userId, std::string& error);
    std::vector<int64> HoldersOf(int64 roleId, std::string& error);

    std::set<std::string, std::less<>> KeysOfUser(int64 userId, std::string& error);
    bool UserHolds(int64 userId, std::string_view key, std::string& error);

    void Rebuild(std::string& error);

private:
    bool WriteKeys(int64 roleId, std::vector<std::string> const& keys, std::string& error);
    static PanelRoleRow Read(PanelStore::Statement const& row);

    PanelStore& _store;
    std::mutex _mutex;
    std::map<int64, std::set<std::string, std::less<>>> _resolution;
};

#endif
