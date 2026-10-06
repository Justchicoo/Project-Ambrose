/*
 * Project Ambrose by Imjustchico
 * Opens the list's SQLite file with a busy timeout and makes its one table, whose name and origin are each unique and which has no column for a password, reads entries back into addresses the way they were checked when added, and on forgetting an entry removes its row and then its web view profile, reporting a profile that could not be deleted.
 */

#include "PanelList.h"

#include "ConfigMgr.h"
#include "ShellProfiles.h"

#include <sqlite3.h>

#include <fmt/format.h>

#include <system_error>

namespace
{
    constexpr char const* Schema = "CREATE TABLE IF NOT EXISTS panel ("
                                   "id INTEGER PRIMARY KEY,"
                                   "name TEXT NOT NULL UNIQUE,"
                                   "origin TEXT NOT NULL UNIQUE,"
                                   "trust TEXT NOT NULL,"
                                   "pin TEXT NOT NULL DEFAULT '',"
                                   "last_user TEXT NOT NULL DEFAULT '',"
                                   "last_opened INTEGER NOT NULL DEFAULT 0)";

    constexpr char const* Columns = "SELECT id, name, origin, trust, pin, last_user, last_opened FROM panel";

    std::string Column(sqlite3_stmt* statement, int column)
    {
        unsigned char const* const text = sqlite3_column_text(statement, column);
        return text == nullptr ? std::string() : std::string(reinterpret_cast<char const*>(text));
    }

    std::optional<PanelEntry> Read(sqlite3_stmt* statement)
    {
        PanelEntry entry;
        entry.Id = sqlite3_column_int64(statement, 0);
        entry.Name = Column(statement, 1);
        std::optional<ShellOrigin> const origin = ShellOrigin::Of(Column(statement, 2));
        std::optional<PanelTrust> const trust = PanelAddress::TrustNamed(Column(statement, 3));
        if (!origin || !trust)
            return std::nullopt;
        entry.Address.Origin = *origin;
        entry.Address.Trust = *trust;
        entry.Address.Pin = Column(statement, 4);
        entry.LastUser = Column(statement, 5);
        entry.LastOpenedMs = sqlite3_column_int64(statement, 6);
        return entry;
    }
}

PanelList::~PanelList()
{
    Close();
}

void PanelList::Close()
{
    if (_database != nullptr)
        sqlite3_close(_database);
    _database = nullptr;
}

bool PanelList::Open(std::filesystem::path const& dataFolder, std::string& error)
{
    Close();
    std::error_code made;
    std::filesystem::create_directories(dataFolder, made);
    std::string const file = ConfigMgr::PathToUtf8(dataFolder / FileName);
    if (sqlite3_open_v2(file.c_str(), &_database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        error = fmt::format("the panel list {} could not be opened: {}", file, _database ? sqlite3_errmsg(_database) : "no memory");
        Close();
        return false;
    }
    sqlite3_busy_timeout(_database, 5000);
    _dataFolder = dataFolder;
    return Run(Schema, {}, error);
}

bool PanelList::Run(char const* sql, std::vector<Value> const& values, std::string& error)
{
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(_database, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        error = sqlite3_errmsg(_database);
        return false;
    }
    int index = 1;
    for (Value const& value : values)
    {
        if (std::holds_alternative<int64>(value))
            sqlite3_bind_int64(statement, index++, std::get<int64>(value));
        else
        {
            std::string const& text = std::get<std::string>(value);
            sqlite3_bind_text(statement, index++, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
        }
    }
    int const stepped = sqlite3_step(statement);
    if (stepped != SQLITE_DONE && stepped != SQLITE_ROW)
    {
        int const extended = sqlite3_extended_errcode(_database);
        error = extended == SQLITE_CONSTRAINT_UNIQUE ? std::string("a panel with that name or address is already listed") : std::string(sqlite3_errmsg(_database));
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);
    return true;
}

bool PanelList::NameIsUsable(std::string const& name, std::string& error)
{
    if (name.empty() || name.size() > MaxNameLength)
    {
        error = fmt::format("a panel's name is 1 to {} characters", MaxNameLength);
        return false;
    }
    return true;
}

std::vector<PanelEntry> PanelList::Entries(std::string& error) const
{
    std::vector<PanelEntry> entries;
    sqlite3_stmt* statement = nullptr;
    std::string const sql = std::string(Columns) + " ORDER BY last_opened DESC, name";
    if (sqlite3_prepare_v2(_database, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
    {
        error = sqlite3_errmsg(_database);
        return entries;
    }
    while (sqlite3_step(statement) == SQLITE_ROW)
        if (std::optional<PanelEntry> entry = Read(statement))
            entries.push_back(std::move(*entry));
    sqlite3_finalize(statement);
    return entries;
}

std::optional<PanelEntry> PanelList::Find(int64 id, std::string& error) const
{
    for (PanelEntry& entry : Entries(error))
        if (entry.Id == id)
            return std::move(entry);
    error = fmt::format("no panel {} is listed", id);
    return std::nullopt;
}

std::optional<int64> PanelList::Add(std::string const& name, PanelAddress const& address, std::string& error)
{
    if (!NameIsUsable(name, error))
        return std::nullopt;
    if (!Run("INSERT INTO panel (name, origin, trust, pin) VALUES (?, ?, ?, ?)",
            { name, address.Origin.Describe(), std::string(PanelAddress::Name(address.Trust)), address.Pin }, error))
        return std::nullopt;
    return sqlite3_last_insert_rowid(_database);
}

bool PanelList::Rename(int64 id, std::string const& name, std::string& error)
{
    if (!NameIsUsable(name, error) || !Find(id, error))
        return false;
    return Run("UPDATE panel SET name = ? WHERE id = ?", { name, id }, error);
}

bool PanelList::Edit(int64 id, PanelAddress const& address, std::string& error)
{
    std::optional<PanelEntry> const before = Find(id, error);
    if (!before)
        return false;
    if (!Run("UPDATE panel SET origin = ?, trust = ?, pin = ? WHERE id = ?",
            { address.Origin.Describe(), std::string(PanelAddress::Name(address.Trust)), address.Pin, id }, error))
        return false;
    if (before->Address.Origin != address.Origin)
        return ShellProfiles::Delete(_dataFolder, before->Address.Origin, error);
    return true;
}

bool PanelList::Opened(int64 id, std::string const& user, int64 whenMs, std::string& error)
{
    return Run("UPDATE panel SET last_user = ?, last_opened = ? WHERE id = ?", { user, whenMs, id }, error);
}

bool PanelList::Forget(int64 id, std::string& error)
{
    std::optional<PanelEntry> const entry = Find(id, error);
    if (!entry)
        return false;
    if (!Run("DELETE FROM panel WHERE id = ?", { id }, error))
        return false;
    return ShellProfiles::Delete(_dataFolder, entry->Address.Origin, error);
}
