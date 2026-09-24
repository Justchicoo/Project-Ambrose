/*
 * Project Ambrose by Imjustchico
 * The supervisor's own store: one SQLite file in the Ambrose data folder, opened in WAL mode with foreign keys on and a busy timeout, brought up to date at start by the dated files in data/sql/panel, applied in order inside one transaction each and recorded in its own updates table with their hash, and read and written through prepared statements that bind and read by type, so the panel keeps its sessions, its audit rows and later its users before any game database exists.
 */

#ifndef AMBROSE_PANELSTORE_H
#define AMBROSE_PANELSTORE_H

#include "Types.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

class PanelStore
{
public:
    static constexpr int BusyTimeoutMs = 5000;
    static constexpr std::string_view UpdateFolder = "data/sql/panel";

    class Statement
    {
    public:
        Statement() noexcept = default;
        ~Statement();
        Statement(Statement&& other) noexcept;
        Statement& operator=(Statement&& other) noexcept;
        Statement(Statement const&) = delete;
        Statement& operator=(Statement const&) = delete;

        explicit operator bool() const noexcept { return _statement != nullptr; }

        void Bind(int index, int64 value);
        void Bind(int index, std::string_view value);
        void BindNull(int index);
        bool Step(std::string& error);
        bool Run(std::string& error);
        void Reset();

        bool IsNull(int column) const;
        int64 Int64(int column) const;
        std::string Text(int column) const;

    private:
        friend class PanelStore;

        Statement(sqlite3* database, sqlite3_stmt* statement) noexcept : _database(database), _statement(statement) {}

        sqlite3* _database = nullptr;
        sqlite3_stmt* _statement = nullptr;
    };

    PanelStore();
    ~PanelStore();

    PanelStore(PanelStore const&) = delete;
    PanelStore& operator=(PanelStore const&) = delete;

    bool Open(std::filesystem::path const& file, std::filesystem::path const& sourceFolder, std::vector<std::string>& warnings, std::string& error);
    void Close();

    bool IsOpen() const noexcept { return _database != nullptr; }
    std::filesystem::path const& GetFile() const noexcept { return _file; }
    std::vector<std::string> const& GetApplied() const noexcept { return _applied; }

    std::optional<Statement> Prepare(std::string_view sql, std::string& error);
    bool Execute(std::string_view sql, std::string& error);
    int64 LastInsertId() const;
    int64 Changed() const;

    bool Begin(std::string& error);
    bool Commit(std::string& error);
    void Rollback();

    static int64 NowEpochMs();

private:
    bool ApplyUpdates(std::filesystem::path const& folder, std::vector<std::string>& warnings, std::string& error);
    bool ApplyFile(std::filesystem::path const& path, std::string const& name, std::string const& contents, std::string const& hash, std::string& error);

    std::filesystem::path _file;
    sqlite3* _database = nullptr;
    std::vector<std::string> _applied;
};

#endif
