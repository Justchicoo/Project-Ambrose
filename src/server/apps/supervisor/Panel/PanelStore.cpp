/*
 * Project Ambrose by Imjustchico
 * Opens the file through SQLite with WAL, foreign keys and a busy timeout, makes the folder it lives in, creates its updates table when the file is new, and applies every dated file it has not recorded: each is split into statements the way an SQL update file is, run inside one transaction so a failure leaves the store as it was, and recorded with the hash of its text with line endings normalized, which a later start compares so a changed file is reported rather than applied again.
 */

#include "PanelStore.h"
#include "ConfigMgr.h"
#include "SqlScript.h"
#include "UpdateFetcher.h"

#include <sqlite3.h>

#include <fmt/format.h>

#include <chrono>
#include <map>
#include <utility>

namespace
{
    std::string Message(sqlite3* database, std::string_view what)
    {
        char const* const text = database ? sqlite3_errmsg(database) : "no database is open";
        return fmt::format("{}: {}", what, text ? text : "unknown error");
    }
}

PanelStore::Statement::~Statement()
{
    if (_statement)
        sqlite3_finalize(_statement);
}

PanelStore::Statement::Statement(Statement&& other) noexcept
    : _database(std::exchange(other._database, nullptr)), _statement(std::exchange(other._statement, nullptr))
{
}

PanelStore::Statement& PanelStore::Statement::operator=(Statement&& other) noexcept
{
    if (this != &other)
    {
        if (_statement)
            sqlite3_finalize(_statement);
        _database = std::exchange(other._database, nullptr);
        _statement = std::exchange(other._statement, nullptr);
    }
    return *this;
}

void PanelStore::Statement::Bind(int index, int64 value)
{
    sqlite3_bind_int64(_statement, index, value);
}

void PanelStore::Statement::Bind(int index, std::string_view value)
{
    sqlite3_bind_text(_statement, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void PanelStore::Statement::BindNull(int index)
{
    sqlite3_bind_null(_statement, index);
}

bool PanelStore::Statement::Step(std::string& error)
{
    error.clear();
    int const status = sqlite3_step(_statement);
    if (status == SQLITE_ROW)
        return true;
    if (status != SQLITE_DONE)
        error = Message(_database, "a statement failed");
    return false;
}

bool PanelStore::Statement::Run(std::string& error)
{
    while (Step(error))
    {
    }
    return error.empty();
}

void PanelStore::Statement::Reset()
{
    sqlite3_reset(_statement);
    sqlite3_clear_bindings(_statement);
}

bool PanelStore::Statement::IsNull(int column) const
{
    return sqlite3_column_type(_statement, column) == SQLITE_NULL;
}

int64 PanelStore::Statement::Int64(int column) const
{
    return sqlite3_column_int64(_statement, column);
}

std::string PanelStore::Statement::Text(int column) const
{
    unsigned char const* const text = sqlite3_column_text(_statement, column);
    int const bytes = sqlite3_column_bytes(_statement, column);
    return text == nullptr ? std::string() : std::string(reinterpret_cast<char const*>(text), static_cast<std::size_t>(bytes));
}

PanelStore::PanelStore() = default;

PanelStore::~PanelStore()
{
    Close();
}

int64 PanelStore::NowEpochMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void PanelStore::Close()
{
    if (_database)
    {
        sqlite3_close_v2(_database);
        _database = nullptr;
    }
}

bool PanelStore::Open(std::filesystem::path const& file, std::filesystem::path const& sourceFolder, std::vector<std::string>& warnings, std::string& error)
{
    Close();
    error.clear();
    _file = file;
    _applied.clear();
    std::error_code code;
    if (std::filesystem::path const folder = file.parent_path(); !folder.empty())
    {
        std::filesystem::create_directories(folder, code);
        if (code)
        {
            error = fmt::format("the folder {} could not be made: {}", ConfigMgr::PathToUtf8(folder), code.message());
            return false;
        }
    }
    std::string const path = ConfigMgr::PathToUtf8(file);
    if (sqlite3_open_v2(path.c_str(), &_database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        error = Message(_database, fmt::format("the panel store {} could not be opened", path));
        Close();
        return false;
    }
    sqlite3_busy_timeout(_database, BusyTimeoutMs);
    for (char const* const pragma : { "PRAGMA journal_mode = WAL", "PRAGMA synchronous = NORMAL", "PRAGMA foreign_keys = ON" })
    {
        if (!Execute(pragma, error))
        {
            Close();
            return false;
        }
    }
    if (!Execute("CREATE TABLE IF NOT EXISTS updates (name TEXT PRIMARY KEY, hash TEXT NOT NULL, applied_epoch_ms INTEGER NOT NULL, took_ms INTEGER NOT NULL)", error))
    {
        Close();
        return false;
    }
    if (!ApplyUpdates(sourceFolder / ConfigMgr::PathFromUtf8(UpdateFolder), warnings, error))
    {
        Close();
        return false;
    }
    return true;
}

bool PanelStore::Execute(std::string_view sql, std::string& error)
{
    error.clear();
    std::string const text(sql);
    char* message = nullptr;
    if (sqlite3_exec(_database, text.c_str(), nullptr, nullptr, &message) == SQLITE_OK)
        return true;
    error = fmt::format("{}: {}", SqlScript::Excerpt(sql, 120), message ? message : "unknown error");
    sqlite3_free(message);
    return false;
}

std::optional<PanelStore::Statement> PanelStore::Prepare(std::string_view sql, std::string& error)
{
    error.clear();
    sqlite3_stmt* prepared = nullptr;
    if (sqlite3_prepare_v2(_database, sql.data(), static_cast<int>(sql.size()), &prepared, nullptr) != SQLITE_OK)
    {
        error = Message(_database, fmt::format("{} could not be prepared", SqlScript::Excerpt(sql, 120)));
        return std::nullopt;
    }
    return Statement(_database, prepared);
}

int64 PanelStore::LastInsertId() const
{
    return sqlite3_last_insert_rowid(_database);
}

int64 PanelStore::Changed() const
{
    return sqlite3_changes64(_database);
}

bool PanelStore::Begin(std::string& error)
{
    return Execute("BEGIN IMMEDIATE", error);
}

bool PanelStore::Commit(std::string& error)
{
    return Execute("COMMIT", error);
}

void PanelStore::Rollback()
{
    std::string ignored;
    Execute("ROLLBACK", ignored);
}

bool PanelStore::ApplyFile(std::filesystem::path const& path, std::string const& name, std::string const& contents, std::string const& hash, std::string& error)
{
    std::vector<SqlScript::Statement> statements;
    std::string problem;
    if (!SqlScript::Split(contents, statements, problem))
    {
        error = fmt::format("{} cannot be applied: {}", name, problem);
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    if (!Begin(error))
        return false;
    for (SqlScript::Statement const& statement : statements)
    {
        if (statement.Text.empty())
            continue;
        if (!Execute(statement.Text, error))
        {
            error = fmt::format("{} failed on line {}: {}", name, statement.Line, error);
            Rollback();
            return false;
        }
    }
    int64 const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    std::optional<Statement> record = Prepare("INSERT INTO updates (name, hash, applied_epoch_ms, took_ms) VALUES (?, ?, ?, ?)", error);
    if (!record)
    {
        Rollback();
        return false;
    }
    record->Bind(1, name);
    record->Bind(2, hash);
    record->Bind(3, NowEpochMs());
    record->Bind(4, took);
    if (!record->Run(error))
    {
        error = fmt::format("{} was applied but could not be recorded: {}", ConfigMgr::PathToUtf8(path), error);
        Rollback();
        return false;
    }
    record.reset();
    if (!Commit(error))
    {
        Rollback();
        return false;
    }
    _applied.push_back(name);
    return true;
}

bool PanelStore::ApplyUpdates(std::filesystem::path const& folder, std::vector<std::string>& warnings, std::string& error)
{
    std::error_code code;
    if (!std::filesystem::is_directory(folder, code))
    {
        error = fmt::format("the panel store's update folder {} does not exist; set Updates.SourcePath to the Project Ambrose folder", ConfigMgr::PathToUtf8(folder));
        return false;
    }
    std::vector<std::filesystem::path> files;
    if (!UpdateFetcher::ListSqlFiles(folder, files, error))
        return false;

    std::map<std::string, std::string, std::less<>> recorded;
    std::optional<Statement> rows = Prepare("SELECT name, hash FROM updates", error);
    if (!rows)
        return false;
    while (rows->Step(error))
        recorded.emplace(rows->Text(0), rows->Text(1));
    if (!error.empty())
        return false;
    rows.reset();

    for (std::filesystem::path const& path : files)
    {
        std::string const name = ConfigMgr::PathToUtf8(path.filename());
        if (!UpdateFetcher::IsReleasedFileName(name))
        {
            error = fmt::format("{} in {} is not named YYYY_MM_DD_NN.sql", name, ConfigMgr::PathToUtf8(folder));
            return false;
        }
        std::string contents;
        if (!UpdateFetcher::ReadFile(path, contents, error))
            return false;
        std::string const hash = UpdateFetcher::HashContents(contents);
        if (auto const found = recorded.find(name); found != recorded.end())
        {
            if (found->second != hash)
                warnings.push_back(fmt::format("{} changed after it was applied to the panel store; the recorded hash is {}, the file's is {}", name, found->second, hash));
            continue;
        }
        if (!ApplyFile(path, name, contents, hash, error))
            return false;
    }
    return true;
}
