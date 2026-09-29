/*
 * Project Ambrose by Imjustchico
 * Reads every persisted value, writes a change or a whole batch and one audit row per change inside a single transaction that is rolled back if any statement fails, removes a value that was reset, and reads a key's history newest first; times are kept in epoch milliseconds in the store and handed over in the epoch seconds the registry works in.
 */

#include "PanelSettingStore.h"

#include "PanelStore.h"

#include <optional>
#include <utility>

namespace
{
    constexpr char const* ClosedStore = "the panel store is not open, so no live setting of the supervisor can be read or changed";
}

PanelSettingStore::PanelSettingStore(PanelStore& store, std::mutex& mutex) : _store(store), _mutex(mutex)
{
}

bool PanelSettingStore::Load(std::map<std::string, std::string, std::less<>>& values, std::string& error)
{
    std::lock_guard const lock(_mutex);
    if (!_store.IsOpen())
    {
        error = ClosedStore;
        return false;
    }
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT key, value FROM settings", error);
    if (!rows)
        return false;
    while (rows->Step(error))
        values[rows->Text(0)] = rows->Text(1);
    return error.empty();
}

bool PanelSettingStore::Write(SettingWrite const& write, std::string& error)
{
    return WriteMany(std::span<SettingWrite const>(&write, 1), error);
}

bool PanelSettingStore::WriteMany(std::span<SettingWrite const> writes, std::string& error)
{
    std::lock_guard const lock(_mutex);
    if (!_store.IsOpen())
    {
        error = ClosedStore;
        return false;
    }
    if (!_store.Begin(error))
        return false;
    auto const fail = [this]
    {
        _store.Rollback();
        return false;
    };
    for (SettingWrite const& write : writes)
    {
        int64 const when = write.EpochSeconds * 1000;
        if (write.Persisted)
        {
            std::optional<PanelStore::Statement> set = _store.Prepare(
                "INSERT INTO settings (key, value, updated_by, updated_epoch_ms) VALUES (?1, ?2, ?3, ?4) "
                "ON CONFLICT (key) DO UPDATE SET value = excluded.value, updated_by = excluded.updated_by, updated_epoch_ms = excluded.updated_epoch_ms",
                error);
            if (!set)
                return fail();
            set->Bind(1, write.Key);
            set->Bind(2, *write.Persisted);
            set->Bind(3, write.Author.Who);
            set->Bind(4, when);
            if (!set->Run(error))
                return fail();
        }
        else
        {
            std::optional<PanelStore::Statement> remove = _store.Prepare("DELETE FROM settings WHERE key = ?1", error);
            if (!remove)
                return fail();
            remove->Bind(1, write.Key);
            if (!remove->Run(error))
                return fail();
        }
        std::optional<PanelStore::Statement> audit = _store.Prepare(
            "INSERT INTO setting_audit (key, old_value, new_value, who, account_id, source, reason, created_epoch_ms) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)", error);
        if (!audit)
            return fail();
        audit->Bind(1, write.Key);
        audit->Bind(2, write.OldValue);
        audit->Bind(3, write.NewValue);
        audit->Bind(4, write.Author.Who);
        audit->Bind(5, static_cast<int64>(write.Author.AccountId));
        audit->Bind(6, write.Author.Source);
        audit->Bind(7, write.Reason);
        audit->Bind(8, when);
        if (!audit->Run(error))
            return fail();
    }
    if (!_store.Commit(error))
        return fail();
    return true;
}

bool PanelSettingStore::History(std::string const& key, std::size_t limit, std::vector<SettingAuditEntry>& entries, std::string& error)
{
    std::lock_guard const lock(_mutex);
    if (!_store.IsOpen())
    {
        error = ClosedStore;
        return false;
    }
    std::optional<PanelStore::Statement> rows = _store.Prepare(
        "SELECT id, key, old_value, new_value, who, account_id, source, reason, created_epoch_ms FROM setting_audit WHERE key = ?1 "
        "ORDER BY created_epoch_ms DESC, id DESC LIMIT ?2",
        error);
    if (!rows)
        return false;
    rows->Bind(1, key);
    rows->Bind(2, static_cast<int64>(limit));
    while (rows->Step(error))
    {
        SettingAuditEntry entry;
        entry.Id = static_cast<uint64>(rows->Int64(0));
        entry.Key = rows->Text(1);
        entry.OldValue = rows->Text(2);
        entry.NewValue = rows->Text(3);
        entry.Who = rows->Text(4);
        entry.AccountId = static_cast<uint64>(rows->Int64(5));
        entry.Source = rows->Text(6);
        entry.Reason = rows->Text(7);
        entry.EpochSeconds = rows->Int64(8) / 1000;
        entries.push_back(std::move(entry));
    }
    return error.empty();
}
