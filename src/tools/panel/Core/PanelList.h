/*
 * Project Ambrose by Imjustchico
 * The panels the program knows, kept in its own SQLite file under PanelApp in the Ambrose data folder: each entry's name, address, how its certificate is trusted with the pin when it has one, the username last used and when it was last opened. It holds no password and has no place for one. Entries are added, renamed, edited and forgotten, a name or an address is never listed twice, and forgetting an entry deletes its web view profile with everything in it.
 */

#ifndef AMBROSE_PANELLIST_H
#define AMBROSE_PANELLIST_H

#include "PanelAddress.h"
#include "Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct sqlite3;

struct PanelEntry
{
    int64 Id = 0;
    std::string Name = {};
    PanelAddress Address = {};
    std::string LastUser = {};
    int64 LastOpenedMs = 0;
};

class PanelList
{
public:
    static constexpr char const* FileName = "panels.sqlite3";
    static constexpr std::size_t MaxNameLength = 80;

    PanelList() = default;
    ~PanelList();
    PanelList(PanelList const&) = delete;
    PanelList& operator=(PanelList const&) = delete;

    bool Open(std::filesystem::path const& dataFolder, std::string& error);
    void Close();
    bool IsOpen() const { return _database != nullptr; }

    std::vector<PanelEntry> Entries(std::string& error) const;
    std::optional<PanelEntry> Find(int64 id, std::string& error) const;
    std::optional<int64> Add(std::string const& name, PanelAddress const& address, std::string& error);
    bool Rename(int64 id, std::string const& name, std::string& error);
    bool Edit(int64 id, PanelAddress const& address, std::string& error);
    bool Opened(int64 id, std::string const& user, int64 whenMs, std::string& error);
    bool Forget(int64 id, std::string& error);

private:
    using Value = std::variant<std::string, int64>;
    bool Run(char const* sql, std::vector<Value> const& values, std::string& error);
    static bool NameIsUsable(std::string const& name, std::string& error);

    sqlite3* _database = nullptr;
    std::filesystem::path _dataFolder;
};

#endif
