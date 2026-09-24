/*
 * Project Ambrose by Imjustchico
 * The character name manager (sCharacterNameMgr): loads the name tables and disallowed names from one consistent read of the world database into a validated snapshot and swaps it in only when every row is valid, keeping the previous snapshot and reporting each problem otherwise, warns when the new tables leave its default locale without human names, and checks, formats and matches wizard name indices in a named locale or its default locale against the current snapshot.
 */

#ifndef AMBROSE_CHARACTERNAMEMGR_H
#define AMBROSE_CHARACTERNAMEMGR_H

#include "CharacterNames.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class PreparedResultSet;

struct CharacterNameLoadResult
{
    bool Loaded = false;
    std::vector<std::string> Errors;
    std::vector<std::string> Warnings;
    std::size_t Tables = 0;
    std::size_t Parts = 0;
    std::size_t Disallowed = 0;
    std::size_t HumanLocales = 0;
};

class CharacterNameMgr
{
public:
    static constexpr std::string_view DefaultLocale = "en-US";

    static CharacterNameMgr& Instance();

    CharacterNameMgr();
    CharacterNameMgr(CharacterNameMgr const&) = delete;
    CharacterNameMgr& operator=(CharacterNameMgr const&) = delete;

    CharacterNameLoadResult Load();
    void SetDefaultLocale(std::string locale);
    std::string GetDefaultLocale() const;
    std::shared_ptr<CharacterNameSet const> GetNames() const;
    uint64 GetGeneration() const;
    void Clear();

    NameCheck Check(uint32 nameIndices, uint32 gender, std::string_view locale = {}, std::optional<uint32> localeId = std::nullopt) const;
    bool IsValidIndices(uint32 nameIndices, uint32 gender, std::string_view locale = {}) const;
    bool IsDisallowed(uint32 nameIndices, uint32 gender, std::optional<uint32> localeId = std::nullopt) const;
    std::optional<std::string> FormatName(uint32 nameIndices, uint32 gender, std::string_view locale = {}) const;

    static std::vector<CharacterNameTable> ReadTables(PreparedResultSet* result, std::vector<std::string>& errors);
    static std::vector<DisallowedName> ReadDisallowed(PreparedResultSet* result);

private:
    struct Snapshot
    {
        std::shared_ptr<CharacterNameSet const> Names;
        std::string DefaultLocale;
    };

    Snapshot Current() const;

    mutable std::mutex _mutex;
    std::mutex _loadMutex;
    Snapshot _snapshot;
    uint64 _generation = 0;
};

#define sCharacterNameMgr CharacterNameMgr::Instance()

#endif
