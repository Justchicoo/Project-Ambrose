/*
 * Project Ambrose by Imjustchico
 * Stores and loads wizards in the characters database: creating a character with its appearance and the guid high-water mark in one transaction, which a caller that must not block its network thread can build and commit itself, listing and counting an account's live characters, loading one by guid even when deleted, soft deletion of offline characters and restoring, the online flag, the highest guid ever used, and a wizard's character_stats row, read through the wizard so a missing wizard, a wizard with no row yet and a failed read are told apart, and saved whole, with statement builders and row readers for callers that query or save asynchronously.
 */

#ifndef AMBROSE_CHARACTERREPOSITORY_H
#define AMBROSE_CHARACTERREPOSITORY_H

#include "CharacterStats.h"
#include "CharacterSummary.h"
#include "DatabaseEnv.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

enum class CharacterOpResult : uint8
{
    Ok,
    NotFound,
    AlreadyExists,
    InvalidData,
    CharacterOnline,
    DatabaseError
};

struct CharacterLoad
{
    CharacterOpResult Result = CharacterOpResult::DatabaseError;
    std::optional<CharacterSummary> Character;
};

struct CharacterStatsLoad
{
    CharacterOpResult Result = CharacterOpResult::DatabaseError;
    std::optional<CharacterStats> Stats;
};

struct CharacterList
{
    CharacterOpResult Result = CharacterOpResult::DatabaseError;
    std::vector<CharacterSummary> Characters;
};

class CharacterRepository
{
public:
    using Statement = std::unique_ptr<PreparedStatement<CharacterDatabaseConnection>>;
    using CreateTransaction = std::shared_ptr<Transaction<CharacterDatabaseConnection>>;

    static constexpr std::string_view GuidSequence = "character";
    static constexpr std::size_t MaxCustomNameBytes = 64;
    static constexpr std::size_t MaxZoneBytes = 128;

    CharacterRepository() = delete;

    static CharacterOpResult Create(CharacterSummary const& character);
    static CreateTransaction PrepareCreate(CharacterSummary const& character);
    static CharacterList LoadByAccount(uint64 account);
    static CharacterLoad Load(uint64 guid);
    static std::optional<uint32> CountByAccount(uint64 account);
    static CharacterOpResult SoftDelete(uint64 guid, uint64 account, uint64 deletedAt);
    static CharacterOpResult Restore(uint64 guid);
    static CharacterOpResult SetOnline(uint64 guid, bool online);
    static std::optional<uint64> GetMaxGuid();
    static CharacterStatsLoad LoadStats(uint64 guid);
    static CharacterOpResult SaveStats(uint64 guid, CharacterStats const& stats);

    static Statement PrepareLoadByAccount(uint64 account);
    static Statement PrepareLoad(uint64 guid);
    static Statement PrepareCountByAccount(uint64 account);
    static std::vector<CharacterSummary> ReadCharacters(PreparedResultSet& result);
    static Statement PrepareLoadStats(uint64 guid);
    static Statement PrepareSaveStats(uint64 guid, CharacterStats const& stats);
    static std::optional<CharacterStats> ReadStats(PreparedResultSet& result);
    static bool IsValidStats(CharacterStats const& stats) noexcept;
    static std::string_view GetResultName(CharacterOpResult result) noexcept;
};

#endif
