/*
 * Project Ambrose by Imjustchico
 * The catalog itself, written out rather than generated, because it is read by people deciding what to grant and every entry's sentence is the only thing standing between an operator and a permission they did not understand. Roles are built from it by rule rather than by a second list: the owner holds everything, an administrator holds everything not reserved to the owner, and the others name what they hold, so a key added to the catalog is never silently granted to a role that was never considered for it.
 */

#include "PanelPermissions.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <unordered_map>

namespace
{
    constexpr bool Danger = true;
    constexpr bool Owner = true;
    constexpr bool OptIn = true;

    std::vector<PanelPermission> BuildCatalog()
    {
        return {
            { "status", "status.read", "See an app's state, address and build", false, false, false },

            { "console", "console.read", "Read an app's console output", false, false, false },
            { "console", "console.write", "Run commands on an app", false, false, false },
            { "console", "console.raw", "Write to an app's standard input when its admin API is down", Danger, Owner, false },

            { "power", "power.start", "Start an app", false, false, false },
            { "power", "power.stop", "Stop an app", false, false, false },
            { "power", "power.restart", "Restart an app", false, false, false },
            { "power", "power.kill", "End an app at once, which can lose unsaved character state", Danger, false, false },

            { "launch", "launch.read", "See how an app is launched", false, false, false },
            { "launch", "launch.edit", "Change how an app is launched", false, false, false },

            { "network", "network.read", "See an app's ports and listen addresses", false, false, false },
            { "network", "network.edit", "Change an app's ports and listen addresses", false, false, false },

            { "settings", "settings.read", "Read an app's settings", false, false, false },
            { "settings", "settings.edit", "Change an app's settings", false, false, false },
            { "settings", "settings.edit.restricted", "Change settings held back because a wrong value stops the app", Danger, false, false },
            { "settings", "settings.secrets.read", "See the value of a secret setting instead of a mask", Danger, false, false },

            { "reload", "reload.read", "See what can be reloaded", false, false, false },
            { "reload", "reload.run", "Reload an app's content or settings", false, false, false },

            { "files", "files.list", "List files in a root", false, false, false },
            { "files", "files.read", "Read a file", false, false, false },
            { "files", "files.download", "Download a file", false, false, false },
            { "files", "files.write", "Write a file", false, false, false },
            { "files", "files.upload", "Upload a file", false, false, false },
            { "files", "files.delete", "Delete a file into the trash", false, false, false },
            { "files", "files.purge", "Empty the trash, which cannot be undone", Danger, false, false },
            { "files", "files.archive", "Make and unpack archives", false, false, false },
            { "files", "files.permissions", "Change a file's permissions", false, false, false },
            { "files", "files.roots", "Define which folders the panel may reach at all", Danger, Owner, false },
            { "files", "files.pull", "Fetch a file from a URL onto this machine", false, false, OptIn },
            { "files", "files.sftp", "Reach the file roots over SFTP", false, false, OptIn },

            { "backups", "backups.read", "See what backups exist", false, false, false },
            { "backups", "backups.create", "Take a backup", false, false, false },
            { "backups", "backups.download", "Download a backup, which holds account verifiers and config secrets", Danger, false, false },
            { "backups", "backups.restore", "Restore a backup over what is there now", Danger, false, false },
            { "backups", "backups.restore.players", "Restore player data, which can undo bans and password changes", Danger, false, false },
            { "backups", "backups.delete", "Delete a backup", false, false, false },
            { "backups", "backups.pin", "Keep a backup from being cleared away", false, false, false },
            { "backups", "backups.settings", "Change how backups are taken and kept", false, false, false },
            { "backups", "backups.key", "Export the key that opens backup archives", Danger, Owner, false },

            { "schedules", "schedules.read", "See scheduled tasks", false, false, false },
            { "schedules", "schedules.edit", "Add and change scheduled tasks", false, false, false },
            { "schedules", "schedules.run", "Run a scheduled task now", false, false, false },
            { "schedules", "schedules.delete", "Delete a scheduled task", false, false, false },

            { "updates", "updates.read", "See what updates are available", false, false, false },
            { "updates", "updates.apply", "Apply an update to this installation", Danger, false, false },
            { "updates", "updates.rollback", "Go back to the previous version", false, false, false },

            { "clientdata", "clientdata.read", "See which client install and type dump are in use", false, false, false },
            { "clientdata", "clientdata.rebuild", "Build the type dump again from the install", false, false, false },
            { "clientdata", "clientdata.switch", "Use a different client install", false, false, false },

            { "patch", "patch.read", "See the patch manifest and its revisions", false, false, false },
            { "patch", "patch.publish", "Publish a patch revision to players", Danger, Owner, false },
            { "patch", "patch.key", "Use or export the operator's signing key", Danger, Owner, false },

            { "database", "database.read", "See database state and pending updates", false, false, false },
            { "database", "database.hosts", "Change which database server an app uses", Danger, false, false },
            { "database", "database.rotate", "Rotate database credentials", false, false, false },
            { "database", "database.secrets.read", "See database passwords instead of a mask", Danger, false, false },

            { "world", "world.read", "Read world data", false, false, false },
            { "world", "world.edit", "Change world data", false, false, false },
            { "world", "world.export", "Export world data", false, false, false },

            { "realms", "realms.read", "See the realms and their zones", false, false, false },
            { "realms", "realms.edit", "Add and change realms", false, false, false },
            { "realms", "realms.maintenance", "Close a realm to players", false, false, false },
            { "realms", "realms.queue", "Control the admission queue", false, false, false },

            { "accounts", "accounts.read", "See accounts", false, false, false },
            { "accounts", "accounts.pii.read", "See an account's email, last address and machine id", false, false, false },
            { "accounts", "accounts.create", "Create an account", false, false, false },
            { "accounts", "accounts.edit", "Change an account", false, false, false },
            { "accounts", "accounts.password", "Set an account's password", false, false, false },
            { "accounts", "accounts.security", "Change an account's two-factor and security settings", Danger, false, false },
            { "accounts", "accounts.ban", "Ban an account", false, false, false },
            { "accounts", "accounts.unban", "Lift a ban", false, false, false },
            { "accounts", "accounts.delete", "Delete an account and everything it owns", Danger, false, false },
            { "accounts", "accounts.registration", "Control whether players may sign up", false, false, false },

            { "characters", "characters.read", "See characters", false, false, false },
            { "characters", "characters.rename", "Rename a character", false, false, false },
            { "characters", "characters.restore", "Restore a deleted character", false, false, false },
            { "characters", "characters.edit", "Change a character", false, false, false },
            { "characters", "characters.delete", "Delete a character", Danger, false, false },

            { "players", "players.read", "See who is playing", false, false, false },
            { "players", "players.kick", "Disconnect a player", false, false, false },
            { "players", "players.mute", "Mute a player", false, false, false },
            { "players", "players.teleport", "Move a player", false, false, false },

            { "moderation", "reports.read", "Read player reports", false, false, false },
            { "moderation", "reports.claim", "Take a report to work on", false, false, false },
            { "moderation", "reports.action", "Act on a report", false, false, false },
            { "moderation", "chat.read", "Read what players said to each other, recorded each time it is read", Danger, false, false },

            { "announcements", "announcements.send", "Send an announcement to players", false, false, false },
            { "announcements", "events.manage", "Run timed events", false, false, false },

            { "metrics", "metrics.read", "See figures, graphs and the analytics pages", false, false, false },
            { "metrics", "alerts.read", "See alert rules and what they fired on", false, false, false },
            { "metrics", "alerts.manage", "Add and change alert rules", false, false, false },

            { "activity", "activity.read", "Read the activity log", false, false, false },
            { "activity", "activity.ip.read", "See the addresses in the activity log", false, false, false },
            { "activity", "activity.export", "Export the activity log", false, false, false },

            { "users", "users.read", "See the panel's operators", false, false, false },
            { "users", "users.invite", "Invite an operator", false, false, false },
            { "users", "users.update", "Change an operator's role or grants", false, false, false },
            { "users", "users.delete", "Remove an operator", false, false, false },

            { "apikeys", "apikeys.manage", "Manage other operators' API keys; everyone manages their own", false, false, false },

            { "nodes", "nodes.read", "See the machines this panel manages", false, false, false },
            { "nodes", "nodes.manage", "Add, change and remove machines", Danger, false, false },
            { "nodes", "nodes.move", "Move an app between machines", false, false, false },

            { "panel", "panel.settings", "Change the panel's own settings", Danger, false, false },
            { "panel", "panel.maintenance", "Close the whole installation to players", Danger, false, false },
            { "panel", "panel.status", "Post on the public status page", false, false, false },

            { "debug", "debug.errors", "See full error text instead of a correlation id", false, false, false },
        };
    }

    std::vector<std::string_view> KeysMatching(bool (*wanted)(PanelPermission const&))
    {
        std::vector<std::string_view> keys;
        for (PanelPermission const& permission : PanelPermissions::All())
            if (wanted(permission))
                keys.push_back(permission.Key);
        return keys;
    }

    bool NotOwnerOnly(PanelPermission const& permission) { return !permission.OwnerOnly && !permission.OptIn; }

    bool Everything(PanelPermission const&) { return true; }

    std::vector<std::string_view> const OperatorKeys{
        "status.read", "console.read", "console.write", "power.start", "power.stop", "power.restart",
        "settings.read", "reload.read", "reload.run", "files.list", "files.read", "files.download",
        "backups.read", "backups.create", "schedules.read", "schedules.edit", "schedules.run",
        "clientdata.read", "database.read", "realms.read", "players.read", "players.kick",
        "metrics.read", "alerts.read", "activity.read", "users.read", "nodes.read", "updates.read", "launch.read", "network.read"
    };

    std::vector<std::string_view> const GameMasterKeys{
        "status.read", "console.read", "world.read", "realms.read", "realms.maintenance",
        "accounts.read", "accounts.ban", "accounts.unban", "characters.read", "characters.rename", "characters.restore",
        "players.read", "players.kick", "players.mute", "players.teleport",
        "reports.read", "reports.claim", "reports.action", "announcements.send", "events.manage",
        "metrics.read", "activity.read"
    };

    std::vector<std::string_view> const ViewerKeys{
        "status.read", "console.read", "settings.read", "reload.read", "files.list", "files.read",
        "backups.read", "schedules.read", "updates.read", "clientdata.read", "database.read", "world.read",
        "realms.read", "accounts.read", "characters.read", "players.read", "reports.read",
        "metrics.read", "alerts.read", "activity.read", "users.read", "nodes.read", "launch.read", "network.read"
    };
}

std::vector<PanelPermission> const& PanelPermissions::All()
{
    static std::vector<PanelPermission> const catalog = BuildCatalog();
    return catalog;
}

PanelPermission const* PanelPermissions::Find(std::string_view key)
{
    static std::unordered_map<std::string_view, PanelPermission const*> const index = []
    {
        std::unordered_map<std::string_view, PanelPermission const*> built;
        for (PanelPermission const& permission : All())
            built.emplace(permission.Key, &permission);
        return built;
    }();
    auto const found = index.find(key);
    return found == index.end() ? nullptr : found->second;
}

std::string_view PanelPermissions::NameOf(PanelRole role) noexcept
{
    switch (role)
    {
        case PanelRole::Owner: return "owner";
        case PanelRole::Admin: return "admin";
        case PanelRole::Operator: return "operator";
        case PanelRole::GameMaster: return "game master";
        case PanelRole::Viewer: return "viewer";
    }
    return "viewer";
}

bool PanelPermissions::ParseRole(std::string_view text, PanelRole& role) noexcept
{
    for (PanelRole const candidate : Roles())
    {
        if (NameOf(candidate) == text)
        {
            role = candidate;
            return true;
        }
    }
    return false;
}

std::vector<PanelRole> const& PanelPermissions::Roles()
{
    static std::vector<PanelRole> const roles{ PanelRole::Owner, PanelRole::Admin, PanelRole::Operator, PanelRole::GameMaster, PanelRole::Viewer };
    return roles;
}

std::vector<std::string_view> const& PanelPermissions::KeysOf(PanelRole role)
{
    static std::vector<std::string_view> const owner = KeysMatching(Everything);
    static std::vector<std::string_view> const admin = KeysMatching(NotOwnerOnly);
    switch (role)
    {
        case PanelRole::Owner: return owner;
        case PanelRole::Admin: return admin;
        case PanelRole::Operator: return OperatorKeys;
        case PanelRole::GameMaster: return GameMasterKeys;
        case PanelRole::Viewer: return ViewerKeys;
    }
    return ViewerKeys;
}

bool PanelPermissions::RoleAllows(PanelRole role, std::string_view key)
{
    if (!Holds(key))
        return false;
    std::vector<std::string_view> const& keys = KeysOf(role);
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

std::string PanelPermissions::CatalogJson()
{
    nlohmann::json permissions = nlohmann::json::array();
    for (PanelPermission const& permission : All())
    {
        nlohmann::json entry;
        entry["group"] = permission.Group;
        entry["key"] = permission.Key;
        entry["description"] = permission.Description;
        entry["danger"] = permission.Danger;
        entry["owner_only"] = permission.OwnerOnly;
        entry["opt_in"] = permission.OptIn;
        permissions.push_back(std::move(entry));
    }

    nlohmann::json roles = nlohmann::json::array();
    for (PanelRole const role : Roles())
    {
        nlohmann::json entry;
        entry["name"] = NameOf(role);
        entry["keys"] = KeysOf(role);
        roles.push_back(std::move(entry));
    }

    nlohmann::json body;
    body["schema"] = 1;
    body["permissions"] = std::move(permissions);
    body["roles"] = std::move(roles);
    return body.dump();
}
