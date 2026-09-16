/*
 * Project Ambrose by Imjustchico
 * Groups an archive's .lang entries under Locale/<locale>/ by locale in name order and builds a locale's table once, under that locale's own lock, on first use: each file is parsed into full keys held once in the text map with each table listing its keys in file order, a key defined again counts as a duplicate whose later text is kept, and a file that fails to read or parse is skipped and named on the table; a locale none of whose files load stays failed, while a build that throws is retried on the next lookup. Loads and default changes are serialized and publish only after the default locale and every locale already built in the previous snapshot build again, so a failed reload leaves the previous snapshot resolving keys.
 */

#include "LocaleStore.h"
#include "ConfigMgr.h"
#include "KiwadArchive.h"
#include "LangFile.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <exception>

struct LocaleStore::Index
{
    struct Slot
    {
        std::vector<KiwadEntry const*> Files;
        mutable std::mutex Mutex;
        mutable std::atomic<bool> Done{ false };
        mutable std::shared_ptr<LocaleTable const> Table;
        mutable std::string Error;
    };

    std::shared_ptr<KiwadArchive const> Archive;
    std::map<std::string, std::unique_ptr<Slot>, std::less<>> Locales;

    std::string DescribeLocales() const
    {
        std::string names;
        for (auto const& [name, slot] : Locales)
            names += (names.empty() ? "" : ", ") + name;
        return names;
    }

    void Build(std::string const& locale, Slot const& slot) const
    {
        LocaleTable::TextMap texts;
        LocaleTable::StemMap stems;
        std::vector<std::string> problems;
        uint64 entries = 0;
        uint64 duplicates = 0;
        std::size_t loaded = 0;
        for (KiwadEntry const* file : slot.Files)
        {
            KiwadReadResult read = Archive->Read(*file, LangFile::MaxFileBytes);
            if (!read.Succeeded())
            {
                problems.push_back(fmt::format("{}: {}", file->Name, read.Error));
                continue;
            }
            LangParseResult parsed = LangFile::Parse(read.Data);
            if (!parsed.Ok())
            {
                problems.push_back(fmt::format("{}: {}", file->Name, parsed.Error));
                continue;
            }
            ++loaded;
            std::vector<std::string const*>& keys = stems[parsed.Stem];
            for (LangEntry& entry : parsed.Entries)
            {
                ++entries;
                auto const [position, inserted] = texts.try_emplace(LangFile::MakeKey(parsed.Stem, entry.Key), std::move(entry.Text));
                if (inserted)
                {
                    keys.push_back(&position->first);
                    continue;
                }
                ++duplicates;
                position->second = std::move(entry.Text);
            }
        }
        if (loaded == 0)
        {
            slot.Error = problems.empty() ? std::string("it holds no .lang files") : fmt::format("none of its {} files load; the first fails with {}", slot.Files.size(), problems.front());
            return;
        }
        slot.Table = std::make_shared<LocaleTable const>(locale, std::move(texts), std::move(stems), loaded, entries, duplicates, std::move(problems));
    }

    std::shared_ptr<LocaleTable const> Get(std::string_view locale, std::string* error) const
    {
        auto const found = Locales.find(locale);
        if (found == Locales.end())
        {
            if (error)
                *error = fmt::format("there is no {} locale; the install has {}", locale, DescribeLocales());
            return nullptr;
        }
        Slot const& slot = *found->second;
        if (!slot.Done.load(std::memory_order_acquire))
        {
            std::lock_guard const lock(slot.Mutex);
            if (!slot.Done.load(std::memory_order_relaxed))
            {
                try
                {
                    Build(found->first, slot);
                    slot.Done.store(true, std::memory_order_release);
                }
                catch (std::exception const& failure)
                {
                    if (error)
                        *error = fmt::format("the {} locale could not be built this time: {}", locale, failure.what());
                    return nullptr;
                }
            }
        }
        if (!slot.Table && error)
            *error = fmt::format("the {} locale cannot be loaded: {}", locale, slot.Error);
        return slot.Table;
    }

    std::vector<std::string> GetBuiltLocales() const
    {
        std::vector<std::string> built;
        for (auto const& [name, slot] : Locales)
            if (slot->Done.load(std::memory_order_acquire) && slot->Table)
                built.push_back(name);
        return built;
    }
};

struct LocaleStore::Snapshot
{
    std::shared_ptr<Index const> Data;
    std::string DefaultLocale;
};

LocaleTable::LocaleTable(std::string locale, TextMap texts, StemMap stems, std::size_t files, uint64 entries, uint64 duplicates, std::vector<std::string> problems)
    : _locale(std::move(locale)), _texts(std::move(texts)), _stems(std::move(stems)), _files(files), _entries(entries), _duplicates(duplicates), _problems(std::move(problems))
{
}

std::vector<std::string> LocaleTable::GetStems() const
{
    std::vector<std::string> stems;
    stems.reserve(_stems.size());
    for (auto const& [stem, keys] : _stems)
        stems.push_back(stem);
    return stems;
}

bool LocaleTable::HasStem(std::string_view stem) const
{
    return _stems.find(stem) != _stems.end();
}

std::string const* LocaleTable::Find(std::string_view key) const
{
    auto const found = _texts.find(key);
    return found == _texts.end() ? nullptr : &found->second;
}

std::vector<std::string> LocaleTable::FindKeys(std::string_view text) const
{
    std::vector<std::string> keys;
    for (auto const& [key, value] : _texts)
        if (value == text)
            keys.push_back(key);
    std::sort(keys.begin(), keys.end());
    return keys;
}

std::vector<std::pair<std::string, std::string>> LocaleTable::GetEntries(std::string_view stem) const
{
    std::vector<std::pair<std::string, std::string>> entries;
    auto const found = _stems.find(stem);
    if (found == _stems.end())
        return entries;
    entries.reserve(found->second.size());
    for (std::string const* key : found->second)
        entries.emplace_back(*key, *Find(*key));
    return entries;
}

LocaleStore& LocaleStore::Instance()
{
    static LocaleStore store;
    return store;
}

bool LocaleStore::Load(std::filesystem::path const& rootWad, std::string_view defaultLocale, std::string& error)
{
    std::string openError;
    std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(rootWad, openError);
    if (!archive)
    {
        error = openError;
        return false;
    }
    return Load(std::shared_ptr<KiwadArchive const>(std::move(archive)), defaultLocale, error);
}

bool LocaleStore::Load(std::shared_ptr<KiwadArchive const> archive, std::string_view defaultLocale, std::string& error)
{
    if (!archive)
    {
        error = "no archive was given";
        return false;
    }
    std::lock_guard const writer(_writeMutex);
    auto index = std::make_shared<Index>();
    index->Archive = archive;
    for (KiwadEntry const& entry : archive->GetEntries())
    {
        std::string_view name = entry.Name;
        if (!name.starts_with(LocaleFolder) || name.size() <= LocaleFolder.size() + LangExtension.size()
            || !Ambrose::EqualsIgnoreCase(name.substr(name.size() - LangExtension.size()), LangExtension))
            continue;
        name.remove_prefix(LocaleFolder.size());
        std::size_t const slash = name.find('/');
        if (slash == 0 || slash == std::string_view::npos || name.find('/', slash + 1) != std::string_view::npos)
            continue;
        std::unique_ptr<Index::Slot>& slot = index->Locales[std::string(name.substr(0, slash))];
        if (!slot)
            slot = std::make_unique<Index::Slot>();
        slot->Files.push_back(&entry);
    }
    if (index->Locales.empty())
    {
        error = fmt::format("{} holds no {}<locale>/*{} files", ConfigMgr::PathToUtf8(archive->GetPath().filename()), LocaleFolder, LangExtension);
        return false;
    }
    for (auto& [locale, slot] : index->Locales)
        std::sort(slot->Files.begin(), slot->Files.end(), [](KiwadEntry const* left, KiwadEntry const* right) { return left->Name < right->Name; });
    if (!index->Get(defaultLocale, &error))
        return false;
    if (std::shared_ptr<Snapshot const> const current = Current())
    {
        for (std::string const& locale : current->Data->GetBuiltLocales())
        {
            std::string problem;
            if (!index->Get(locale, &problem))
            {
                error = fmt::format("the {} locale is in use, and the new data cannot replace it: {}", locale, problem);
                return false;
            }
        }
    }
    Publish(std::make_shared<Snapshot const>(Snapshot{ std::move(index), std::string(defaultLocale) }));
    return true;
}

bool LocaleStore::SetDefaultLocale(std::string_view locale, std::string& error)
{
    std::lock_guard const writer(_writeMutex);
    std::shared_ptr<Snapshot const> const current = Current();
    if (!current)
    {
        error = "no locale data is loaded";
        return false;
    }
    if (!current->Data->Get(locale, &error))
        return false;
    Publish(std::make_shared<Snapshot const>(Snapshot{ current->Data, std::string(locale) }));
    return true;
}

void LocaleStore::Clear()
{
    std::lock_guard const writer(_writeMutex);
    Publish(nullptr);
}

bool LocaleStore::IsLoaded() const
{
    return Current() != nullptr;
}

std::string LocaleStore::GetDefaultLocale() const
{
    std::shared_ptr<Snapshot const> const current = Current();
    return current ? current->DefaultLocale : std::string();
}

std::vector<std::string> LocaleStore::GetLocales() const
{
    std::vector<std::string> locales;
    if (std::shared_ptr<Snapshot const> const current = Current())
        for (auto const& [locale, slot] : current->Data->Locales)
            locales.push_back(locale);
    return locales;
}

uint64 LocaleStore::GetGeneration() const
{
    std::lock_guard const lock(_mutex);
    return _generation;
}

std::shared_ptr<LocaleTable const> LocaleStore::GetTable(std::string_view locale, std::string* error) const
{
    std::shared_ptr<Snapshot const> const current = Current();
    if (!current)
    {
        if (error)
            *error = "no locale data is loaded";
        return nullptr;
    }
    return current->Data->Get(locale.empty() ? std::string_view(current->DefaultLocale) : locale, error);
}

std::optional<std::string> LocaleStore::Resolve(std::string_view key, std::string_view locale) const
{
    std::shared_ptr<LocaleTable const> const table = GetTable(locale);
    if (!table)
        return std::nullopt;
    std::string const* const text = table->Find(key);
    return text ? std::optional<std::string>(*text) : std::nullopt;
}

bool LocaleStore::HasKey(std::string_view key, std::string_view locale) const
{
    std::shared_ptr<LocaleTable const> const table = GetTable(locale);
    return table && table->Find(key);
}

std::shared_ptr<LocaleStore::Snapshot const> LocaleStore::Current() const
{
    std::lock_guard const lock(_mutex);
    return _snapshot;
}

void LocaleStore::Publish(std::shared_ptr<Snapshot const> snapshot)
{
    std::lock_guard const lock(_mutex);
    _snapshot = std::move(snapshot);
    ++_generation;
}
