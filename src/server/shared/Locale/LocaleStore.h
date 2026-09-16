/*
 * Project Ambrose by Imjustchico
 * The locale store (sLocaleStore) resolving client locale keys such as QuestTitle_00001718 to their text in any installed language: loading indexes the .lang files under Locale/<locale>/ in the user's own Root.wad and builds the default locale at once, every other locale builds on first use, a file that cannot be read or parsed is skipped and reported on its locale's table, a reload or default change builds and checks its data, including every locale already in use, before swapping it in and keeps the previous store on failure, and lookups read an immutable snapshot.
 */

#ifndef AMBROSE_LOCALESTORE_H
#define AMBROSE_LOCALESTORE_H

#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class KiwadArchive;

struct LocaleKeyHash
{
    using is_transparent = void;

    std::size_t operator()(std::string_view key) const noexcept { return std::hash<std::string_view>{}(key); }
};

class LocaleTable
{
public:
    using TextMap = std::unordered_map<std::string, std::string, LocaleKeyHash, std::equal_to<>>;
    using StemMap = std::map<std::string, std::vector<std::string const*>, std::less<>>;

    LocaleTable(std::string locale, TextMap texts, StemMap stems, std::size_t files, uint64 entries, uint64 duplicates, std::vector<std::string> problems);

    LocaleTable(LocaleTable const&) = delete;
    LocaleTable& operator=(LocaleTable const&) = delete;

    std::string const& GetLocale() const noexcept { return _locale; }
    std::size_t GetFileCount() const noexcept { return _files; }
    uint64 GetEntryCount() const noexcept { return _entries; }
    std::size_t GetKeyCount() const noexcept { return _texts.size(); }
    uint64 GetDuplicateCount() const noexcept { return _duplicates; }
    std::vector<std::string> const& GetProblems() const noexcept { return _problems; }
    std::vector<std::string> GetStems() const;
    bool HasStem(std::string_view stem) const;

    std::string const* Find(std::string_view key) const;
    std::vector<std::string> FindKeys(std::string_view text) const;
    std::vector<std::pair<std::string, std::string>> GetEntries(std::string_view stem) const;

private:
    std::string _locale;
    TextMap _texts;
    StemMap _stems;
    std::size_t _files;
    uint64 _entries;
    uint64 _duplicates;
    std::vector<std::string> _problems;
};

class LocaleStore
{
public:
    static constexpr std::string_view LocaleFolder = "Locale/";
    static constexpr std::string_view LangExtension = ".lang";

    static LocaleStore& Instance();

    LocaleStore() = default;
    LocaleStore(LocaleStore const&) = delete;
    LocaleStore& operator=(LocaleStore const&) = delete;

    bool Load(std::filesystem::path const& rootWad, std::string_view defaultLocale, std::string& error);
    bool Load(std::shared_ptr<KiwadArchive const> archive, std::string_view defaultLocale, std::string& error);
    bool SetDefaultLocale(std::string_view locale, std::string& error);
    void Clear();

    bool IsLoaded() const;
    std::string GetDefaultLocale() const;
    std::vector<std::string> GetLocales() const;
    uint64 GetGeneration() const;
    std::shared_ptr<LocaleTable const> GetTable(std::string_view locale = {}, std::string* error = nullptr) const;
    std::optional<std::string> Resolve(std::string_view key, std::string_view locale = {}) const;
    bool HasKey(std::string_view key, std::string_view locale = {}) const;

private:
    struct Index;
    struct Snapshot;

    std::shared_ptr<Snapshot const> Current() const;
    void Publish(std::shared_ptr<Snapshot const> snapshot);

    std::mutex _writeMutex;
    mutable std::mutex _mutex;
    std::shared_ptr<Snapshot const> _snapshot;
    uint64 _generation = 0;
};

#define sLocaleStore LocaleStore::Instance()

#endif
