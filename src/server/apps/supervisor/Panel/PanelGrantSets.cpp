/*
 * Project Ambrose by Imjustchico
 * Reads the one row a user and scope share, defaulting to an empty set at version zero when there is none, and writes a new permission list only when the version still matches what the editor read: an update names the row's version in its WHERE clause and answers stale when another writer got there first, while a first write needs version zero and answers stale when the row appeared meanwhile.
 */

#include "PanelGrantSets.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>
#include <utility>

namespace
{
    constexpr std::string_view Columns = "scope, version, permissions, updated_epoch_ms";
}

PanelGrantSets::PanelGrantSets(PanelStore& store) : _store(store)
{
}

PanelGrantSet PanelGrantSets::Read(int64 userId, PanelStore::Statement const& row)
{
    PanelGrantSet set;
    set.UserId = userId;
    set.Scope = row.Text(0);
    set.Version = row.Int64(1);
    nlohmann::json const permissions = nlohmann::json::parse(row.Text(2), nullptr, false);
    if (permissions.is_array())
        for (nlohmann::json const& permission : permissions)
            if (permission.is_string())
                set.Permissions.push_back(permission.get<std::string>());
    set.UpdatedEpochMs = row.Int64(3);
    return set;
}

std::string PanelGrantSets::PermissionsJson(std::vector<std::string> const& permissions)
{
    nlohmann::json array = nlohmann::json::array();
    for (std::string const& permission : permissions)
        array.push_back(permission);
    return array.dump();
}

bool PanelGrantSets::Get(int64 userId, std::string_view scope, PanelGrantSet& set, std::string& error) const
{
    error.clear();
    set = PanelGrantSet{};
    set.UserId = userId;
    set.Scope = std::string(scope);
    std::optional<PanelStore::Statement> query = _store.Prepare(
        fmt::format("SELECT {} FROM panel_grant_set WHERE user_id = ? AND scope = ?", Columns), error);
    if (!query)
        return false;
    query->Bind(1, userId);
    query->Bind(2, scope);
    bool const found = query->Step(error);
    if (!error.empty())
        return false;
    if (found)
        set = Read(userId, *query);
    return true;
}

std::vector<PanelGrantSet> PanelGrantSets::OfUser(int64 userId, std::string& error) const
{
    error.clear();
    std::vector<PanelGrantSet> sets;
    std::optional<PanelStore::Statement> query = _store.Prepare(
        fmt::format("SELECT {} FROM panel_grant_set WHERE user_id = ? ORDER BY scope", Columns), error);
    if (!query)
        return sets;
    query->Bind(1, userId);
    while (query->Step(error))
        sets.push_back(Read(userId, *query));
    return sets;
}

PanelGrantSetSave PanelGrantSets::Save(int64 userId, std::string_view scope, std::vector<std::string> permissions, int64 expectedVersion, int64 byUserId, int64 whenEpochMs, PanelGrantSet& saved, std::string& error)
{
    error.clear();
    std::sort(permissions.begin(), permissions.end());
    permissions.erase(std::unique(permissions.begin(), permissions.end()), permissions.end());
    std::optional<PanelStore::Statement> update = _store.Prepare(
        "UPDATE panel_grant_set SET version = version + 1, permissions = ?, updated_epoch_ms = ?, updated_by = ?"
        " WHERE user_id = ? AND scope = ? AND version = ? RETURNING version, updated_epoch_ms",
        error);
    if (!update)
        return PanelGrantSetSave::StoreFailed;
    update->Bind(1, PermissionsJson(permissions));
    update->Bind(2, whenEpochMs);
    if (byUserId == 0)
        update->BindNull(3);
    else
        update->Bind(3, byUserId);
    update->Bind(4, userId);
    update->Bind(5, scope);
    update->Bind(6, expectedVersion);
    bool const matched = update->Step(error);
    if (!error.empty())
        return PanelGrantSetSave::StoreFailed;
    if (matched)
    {
        saved.UserId = userId;
        saved.Scope = std::string(scope);
        saved.Version = update->Int64(0);
        saved.Permissions = std::move(permissions);
        saved.UpdatedEpochMs = update->Int64(1);
        if (!update->Run(error))
            return PanelGrantSetSave::StoreFailed;
        return PanelGrantSetSave::Saved;
    }
    update.reset();
    if (expectedVersion != 0)
        return PanelGrantSetSave::StaleVersion;

    std::optional<PanelStore::Statement> insert = _store.Prepare(
        "INSERT INTO panel_grant_set (user_id, scope, version, permissions, updated_epoch_ms, updated_by) VALUES (?, ?, 1, ?, ?, ?)", error);
    if (!insert)
        return PanelGrantSetSave::StoreFailed;
    insert->Bind(1, userId);
    insert->Bind(2, scope);
    insert->Bind(3, PermissionsJson(permissions));
    insert->Bind(4, whenEpochMs);
    if (byUserId == 0)
        insert->BindNull(5);
    else
        insert->Bind(5, byUserId);
    if (!insert->Run(error))
    {
        if (error.find("UNIQUE") != std::string::npos || error.find("PRIMARY") != std::string::npos)
        {
            error.clear();
            return PanelGrantSetSave::StaleVersion;
        }
        return PanelGrantSetSave::StoreFailed;
    }
    saved.UserId = userId;
    saved.Scope = std::string(scope);
    saved.Version = 1;
    saved.Permissions = std::move(permissions);
    saved.UpdatedEpochMs = whenEpochMs;
    return PanelGrantSetSave::Saved;
}

bool PanelGrantSets::Holds(int64 userId, std::string_view scope, std::string_view permission, std::string& error) const
{
    error.clear();
    PanelGrantSet set;
    if (!Get(userId, scope, set, error) || !error.empty())
        return false;
    return std::find(set.Permissions.begin(), set.Permissions.end(), permission) != set.Permissions.end();
}

bool PanelGrantSets::HoldsAny(int64 userId, std::string_view scope, std::string& error) const
{
    error.clear();
    PanelGrantSet set;
    if (!Get(userId, scope, set, error) || !error.empty())
        return false;
    return !set.Permissions.empty();
}

bool PanelGrantSets::Merge(int64 userId, std::string_view scope, std::vector<std::string> add, int64 byUserId, int64 whenEpochMs, PanelGrantSet& saved, std::string& error)
{
    error.clear();
    for (int attempt = 0; attempt < 5; ++attempt)
    {
        PanelGrantSet current;
        if (!Get(userId, scope, current, error) || !error.empty())
            return false;
        std::set<std::string> merged(current.Permissions.begin(), current.Permissions.end());
        for (std::string const& key : add)
            merged.emplace(key);
        std::vector<std::string> permissions(merged.begin(), merged.end());
        PanelGrantSetSave const outcome = Save(userId, scope, std::move(permissions), current.Version, byUserId, whenEpochMs, saved, error);
        if (outcome == PanelGrantSetSave::Saved)
            return true;
        if (outcome != PanelGrantSetSave::StaleVersion)
            return false;
        error.clear();
    }
    error = "the grant set changed under the merge too many times";
    return false;
}
